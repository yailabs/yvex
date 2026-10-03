// Native computational plans and publication retain their own semantics and allocation lifetimes.
use super::{Error, argument, execution::checked, extent_error, pipeline::cancelled, raw};
use std::{ffi::c_void, sync::atomic::AtomicBool};

extern "C" fn publication_admitted(
    _descriptor: std::ffi::c_int,
    _count: usize,
    context: *mut c_void,
    failure: *mut raw::yvex_error,
) -> i32 {
    if cancelled(context) == 0 {
        return 0;
    }
    let status = raw::yvex_status_YVEX_ERR_CANCELLED;
    unsafe {
        raw::yvex_error_set(
            failure,
            status,
            c"component.publication".as_ptr(),
            c"component cancelled before output publication".as_ptr(),
        );
    }
    status
}

struct Snapshot {
    bytes: *mut u8,
    count: usize,
}
impl Drop for Snapshot {
    fn drop(&mut self) {
        unsafe {
            raw::yvex_core_allocate(
                raw::yvex_core_allocation_operation_YVEX_CORE_ALLOCATE_FREE,
                self.bytes.cast(),
                0,
                0,
            );
        }
    }
}
impl Snapshot {
    fn exact(path: &str, expected: u64) -> Result<Self, Error> {
        let extent = usize::try_from(expected)
            .ok()
            .filter(|n| *n > 0 && *n <= isize::MAX as usize)
            .ok_or_else(extent_error)?;
        let path = argument(Some(path))?.expect("required input path");
        let mut snapshot = Self {
            bytes: std::ptr::null_mut(),
            count: 0,
        };
        let mut receipt = raw::yvex_core_file_result::default();
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe {
                raw::yvex_core_file_read_snapshot(
                    path.as_ptr(),
                    extent,
                    &mut snapshot.bytes,
                    &mut snapshot.count,
                    &mut receipt,
                    &mut failure,
                )
            },
            &failure,
        )?;
        if snapshot.count != extent || snapshot.bytes.is_null() || extent % size_of::<f32>() != 0 {
            return Err(Error {
                code: raw::yvex_status_YVEX_ERR_FORMAT,
                owner: "media.input".into(),
                message: "input does not match its exact planar F32 extent".into(),
            });
        }
        // The native core snapshot allocator returns malloc-aligned storage, not a Vec<u8> borrow.
        Ok(snapshot)
    }
}

#[derive(Default)]
struct Artifact {
    artifact: *mut raw::yvex_artifact,
    gguf: *mut raw::yvex_gguf,
    tensors: *mut raw::yvex_tensor_table,
}
impl Drop for Artifact {
    fn drop(&mut self) {
        unsafe {
            raw::yvex_tensor_table_close(self.tensors);
            raw::yvex_gguf_close(self.gguf);
            raw::yvex_artifact_close(self.artifact);
        }
    }
}
impl Artifact {
    fn open(path: &str) -> Result<Self, Error> {
        let path = argument(Some(path))?.expect("required artifact path");
        let mut owner = Self::default();
        let mut failure = raw::yvex_error::default();
        let options = raw::yvex_artifact_options {
            path: path.as_ptr(),
            readonly: 1,
            ..Default::default()
        };
        checked(
            unsafe { raw::yvex_artifact_open(&mut owner.artifact, &options, &mut failure) },
            &failure,
        )?;
        checked(
            unsafe { raw::yvex_gguf_open(&mut owner.gguf, owner.artifact, &mut failure) },
            &failure,
        )?;
        checked(
            unsafe {
                raw::yvex_tensor_table_from_gguf(&mut owner.tensors, owner.gguf, &mut failure)
            },
            &failure,
        )?;
        Ok(owner)
    }
}

pub(crate) struct Component<'a> {
    pub target: &'a str,
    pub component: &'a str,
    pub backend: raw::yvex_backend_kind,
    pub batch: u64,
    pub geometry: &'a [u64],
    pub host_bytes: u64,
    pub artifact: &'a str,
    pub input: &'a str,
    pub output: &'a str,
}
pub(crate) fn component(
    input: Component<'_>,
    cancel: &AtomicBool,
) -> Result<raw::yvex_component_execution_result, Error> {
    let target = argument(Some(input.target))?.expect("required target");
    let name = argument(Some(input.component))?.expect("required component");
    let mut request = raw::yvex_component_plan_request {
        target_id: target.as_ptr(),
        component_id: name.as_ptr(),
        backend: input.backend,
        batch: input.batch,
        geometry_rank: input.geometry.len() as u32,
        maximum_host_bytes: input.host_bytes,
        ..Default::default()
    };
    if input.geometry.len() > request.geometry.len() {
        return Err(extent_error());
    }
    request.geometry[..input.geometry.len()].copy_from_slice(input.geometry);
    let api = unsafe { raw::yvex_runtime_component_api_get().as_ref() }.ok_or_else(extent_error)?;
    let mut failure = raw::yvex_error::default();
    let mut reason = raw::yvex_component_failure::default();
    let mut plan = raw::yvex_component_plan::default();
    checked(
        unsafe {
            api.plan_build.ok_or_else(extent_error)?(&request, &mut plan, &mut reason, &mut failure)
        },
        &failure,
    )?;
    if plan.complete == 0
        || plan.output_bytes % size_of::<f32>() as u64 != 0
        || plan.output_bytes / size_of::<f32>() as u64 != plan.output_values
    {
        return Err(extent_error());
    }
    let source = Snapshot::exact(input.input, plan.input_bytes)?;
    let count = usize::try_from(plan.output_values)
        .ok()
        .filter(|n| {
            n.checked_mul(size_of::<f32>())
                .is_some_and(|v| v <= isize::MAX as usize)
        })
        .ok_or_else(extent_error)?;
    let mut output = Vec::<f32>::new();
    output.try_reserve_exact(count).map_err(|_| Error {
        code: raw::yvex_status_YVEX_ERR_NOMEM,
        owner: "component.output".into(),
        message: "component output allocation failed".into(),
    })?;
    output.resize(count, 0.0);
    let artifact = Artifact::open(input.artifact)?;
    let execution = raw::yvex_component_execution_request {
        plan: &plan,
        input: source.bytes.cast(),
        output: output.as_mut_ptr(),
        output_capacity: plan.output_values,
        cancelled: Some(cancelled),
        cancellation_context: (cancel as *const AtomicBool).cast_mut().cast(),
    };
    let mut result = raw::yvex_component_execution_result::default();
    checked(
        unsafe {
            api.execute.ok_or_else(extent_error)?(
                artifact.artifact,
                artifact.gguf,
                artifact.tensors,
                &execution,
                &mut result,
                &mut reason,
                &mut failure,
            )
        },
        &failure,
    )?;
    if result.complete == 0
        || result.output_values != plan.output_values
        || output.iter().any(|v| !v.is_finite())
    {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "component.publication".into(),
            message: "component output is incomplete or non-finite".into(),
        });
    }
    let path = argument(Some(input.output))?.expect("required output path");
    let mut receipt = raw::yvex_core_file_result::default();
    checked(
        unsafe {
            raw::yvex_core_file_publish_noreplace(
                path.as_ptr(),
                output.as_ptr().cast(),
                count * size_of::<f32>(),
                std::ptr::null(),
                Some(publication_admitted),
                (cancel as *const AtomicBool).cast_mut().cast(),
                &mut receipt,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok(result)
}

pub(crate) struct Publish<'a> {
    pub output: &'a str,
    pub video: &'a str,
    pub audio: &'a str,
    pub frames: u64,
    pub width: u64,
    pub height: u64,
    pub fps: [u64; 2],
    pub channels: u64,
    pub samples: u64,
    pub rate: u64,
    pub host_bytes: u64,
    pub output_bytes: u64,
}
pub(crate) fn publish(
    input: Publish<'_>,
    cancel: &AtomicBool,
) -> Result<raw::yvex_media_avi_result, Error> {
    let video_bytes = [
        3u64,
        input.frames,
        input.width,
        input.height,
        size_of::<f32>() as u64,
    ]
    .into_iter()
    .try_fold(1u64, u64::checked_mul)
    .ok_or_else(extent_error)?;
    let audio_bytes = input
        .channels
        .checked_mul(input.samples)
        .and_then(|v| v.checked_mul(size_of::<f32>() as u64))
        .ok_or_else(extent_error)?;
    let input_bytes = video_bytes
        .checked_add(audio_bytes)
        .filter(|n| *n < input.host_bytes)
        .ok_or_else(extent_error)?;
    let video = Snapshot::exact(input.video, video_bytes)?;
    let audio = Snapshot::exact(input.audio, audio_bytes)?;
    let path = argument(Some(input.output))?.expect("required output path");
    let request = raw::yvex_media_avi_request {
        schema_version: raw::YVEX_MEDIA_AVI_SCHEMA_V1,
        path: path.as_ptr(),
        video: video.bytes.cast(),
        audio: audio.bytes.cast(),
        video_channels: 3,
        frames: input.frames,
        width: input.width,
        height: input.height,
        fps_numerator: input.fps[0],
        fps_denominator: input.fps[1],
        audio_channels: input.channels,
        audio_samples: input.samples,
        audio_sample_rate: input.rate,
        maximum_file_bytes: input.output_bytes.min(input.host_bytes - input_bytes),
        cancel_requested: Some(cancelled),
        cancel_context: (cancel as *const AtomicBool).cast_mut().cast(),
        ..Default::default()
    };
    let mut result = raw::yvex_media_avi_result::default();
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe { raw::yvex_media_avi_publish(&request, &mut result, &mut failure) },
        &failure,
    )?;
    if result.complete == 0 {
        return Err(extent_error());
    }
    Ok(result)
}

pub(crate) struct Generate<'a> {
    pub target: &'a str,
    pub prompt: &'a str,
    pub output: &'a str,
    pub artifacts: [&'a str; 4],
    pub frames: u64,
    pub width: u64,
    pub height: u64,
    pub fps: [u64; 2],
    pub steps: u32,
    pub blocks: u64,
    pub seed: u64,
    pub host_bytes: u64,
    pub device_bytes: u64,
    pub workspace_bytes: u64,
    pub output_bytes: u64,
}
pub(crate) fn generate(
    input: Generate<'_>,
    cancel: &AtomicBool,
) -> Result<raw::yvex_runtime_av_generation_result, Error> {
    let strings = [
        input.target,
        input.prompt,
        input.output,
        input.artifacts[0],
        input.artifacts[1],
        input.artifacts[2],
        input.artifacts[3],
    ]
    .into_iter()
    .map(|v| argument(Some(v)).map(|v| v.expect("required generation input")))
    .collect::<Result<Vec<_>, _>>()?;
    let adapter = unsafe { raw::yvex_graph_component_variant_find(strings[0].as_ptr()).as_ref() };
    let execution = adapter.and_then(|v| unsafe { v.media_execution.as_ref() });
    let profile = adapter.and_then(|v| v.media_target_profile);
    let (Some(execution), Some(profile)) = (execution, profile) else {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_UNSUPPORTED,
            owner: "media.generate".into(),
            message: "the requested target has no admitted media adapter".into(),
        });
    };
    if execution.schema_version != raw::YVEX_MEDIA_EXECUTION_RECIPE_SCHEMA_V2
        || execution.conditioning.is_null()
    {
        return Err(extent_error());
    }
    let mut target = raw::yvex_media_target_profile::default();
    let mut failure = raw::yvex_error::default();
    checked(unsafe { profile(&mut target, &mut failure) }, &failure)?;
    let conditioning = unsafe { execution.conditioning.as_ref() }.ok_or_else(extent_error)?;
    // These are immutable native family facts, not Rust-authored model semantics.
    let mut request = Box::new(raw::yvex_runtime_av_generation_request {
        schema_version: raw::YVEX_RUNTIME_AV_GENERATION_SCHEMA_V3,
        target: strings[0].as_ptr(),
        prompt: strings[1].as_ptr(),
        output_path: strings[2].as_ptr(),
        text_artifact_path: strings[3].as_ptr(),
        transformer_artifact_path: strings[4].as_ptr(),
        video_artifact_path: strings[5].as_ptr(),
        audio_artifact_path: strings[6].as_ptr(),
        source_identity: target.source_identity,
        frames: input.frames,
        width: input.width,
        height: input.height,
        fps_numerator: input.fps[0],
        fps_denominator: input.fps[1],
        audio_sample_rate: target.audio_sample_rate,
        inference_steps: input.steps,
        conditioning_layers: execution.conditioning_layers,
        conditioning_width: conditioning.hidden_width,
        transformer_blocks: input.blocks,
        seed: input.seed,
        keyframe_encode_seed: target.keyframe_encode_seed,
        maximum_prompt_tokens: execution.maximum_prompt_tokens,
        maximum_packed_rows: execution.maximum_packed_rows,
        maximum_host_bytes: input.host_bytes,
        maximum_device_bytes: input.device_bytes,
        maximum_workspace_bytes: input.workspace_bytes,
        maximum_file_bytes: input.output_bytes,
        component_backend: execution.component_backend,
        video_temporal_ratio: target.video_temporal_ratio,
        video_clip_length: target.video_clip_length,
        video_token_drop: target.video_token_drop,
        video_spatial_ratio: target.video_spatial_ratio,
        video_tile_size: target.video_tile_size,
        video_minimum_tile_overlap: target.video_minimum_tile_overlap,
        video_mean: target.video_mean,
        video_std: target.video_std,
        audio_mean: target.audio_mean,
        audio_std: target.audio_std,
        pixel_mean: target.pixel_mean,
        pixel_std: target.pixel_std,
        video_channels: target.video_channels,
        audio_channels: target.audio_channels,
        pixel_channels: target.pixel_channels,
        audio_output_channels: target.audio_output_channels,
        audio_samples_per_step: target.audio_samples_per_step,
        plan_build: execution.plan_build,
        layout_build: execution.layout_build,
        component_admit: execution.component_admit,
        condition: execution.condition,
        keyframe_encode: execution.keyframe_encode,
        latent: execution.latent,
        video_decode: execution.video_decode,
        audio_decode: execution.audio_decode,
        cancel_requested: Some(cancelled),
        cancel_context: (cancel as *const AtomicBool).cast_mut().cast::<c_void>(),
        ..Default::default()
    });
    if !execution.output_semantic_domain.is_null()
        || !execution.video_output_requirement.is_null()
        || !execution.audio_output_requirement.is_null()
    {
        checked(
            unsafe {
                raw::yvex_runtime_media_request_specialize(
                    request.as_mut(),
                    execution.output_semantic_domain,
                    execution.video_output_requirement,
                    execution.audio_output_requirement,
                    &mut failure,
                )
            },
            &failure,
        )?;
    }
    let mut result = raw::yvex_runtime_av_generation_result::default();
    checked(
        unsafe { raw::yvex_runtime_av_generate(request.as_ref(), &mut result, &mut failure) },
        &failure,
    )?;
    if result.complete == 0 {
        return Err(extent_error());
    }
    Ok(result)
}
