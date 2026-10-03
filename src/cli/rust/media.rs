// CLI grammar/presentation composes typed native component and synchronized-media owners.
use crate::{
    attention::{Signals, expanded},
    ffi::{self, media, raw},
    pipeline::{number, required},
    presentation,
    registry::Invocation,
};
use serde_json::{Value, json};
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;
type Fields = Vec<(&'static str, Value)>;

fn invalid(message: &str) -> ffi::Error {
    ffi::Error {
        code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
        owner: "media.grammar".into(),
        message: message.into(),
    }
}
fn nonzero(invocation: &Invocation<'_>, name: &str) -> Result<u64> {
    let value = number(invocation, name, 0, false)?;
    if value == 0 {
        return Err(invalid(&format!("{name} is required")).into());
    }
    Ok(value)
}
fn file(invocation: &Invocation<'_>, name: &str) -> Result<String> {
    expanded(required(invocation, name)?)
}

fn component(
    invocation: &Invocation<'_>,
    cancel: &std::sync::atomic::AtomicBool,
) -> Result<Fields> {
    let component = if invocation.operation.operation_id.ends_with("audio-vae") {
        "audio-vae"
    } else {
        "video-vae"
    };
    let geometry = if component == "audio-vae" {
        if ["--latent-frames", "--latent-height", "--latent-width"]
            .iter()
            .any(|v| invocation.has(v))
        {
            return Err(invalid("audio-vae requires latent steps, not video geometry").into());
        }
        vec![nonzero(invocation, "--latent-steps")?]
    } else {
        if invocation.has("--latent-steps") {
            return Err(invalid("video-vae requires latent frames, height and width").into());
        }
        vec![
            nonzero(invocation, "--latent-frames")?,
            nonzero(invocation, "--latent-height")?,
            nonzero(invocation, "--latent-width")?,
        ]
    };
    let artifact = file(invocation, "--artifact")?;
    let input = file(invocation, "--input-file")?;
    let output = file(invocation, "--out")?;
    let target = required(invocation, "--target")?;
    let backend = required(invocation, "--backend")?;
    let result = media::component(
        media::Component {
            target,
            component,
            backend: ffi::pipeline::backend(backend)?,
            batch: number(invocation, "--batch", 1, false)?,
            geometry: &geometry,
            host_bytes: number(invocation, "--max-host-bytes", 256 * 1024 * 1024, false)?,
            artifact: &artifact,
            input: &input,
            output: &output,
        },
        cancel,
    )?;
    Ok(vec![
        ("status", json!("component-decode-complete")),
        ("target", json!(target)),
        ("component", json!(component)),
        ("backend", json!(backend)),
        (
            "artifact_identity",
            json!(ffi::text(&result.artifact_identity)),
        ),
        (
            "execution_identity",
            json!(ffi::text(&result.execution_identity)),
        ),
        ("batch", json!(result.batch)),
        ("output_values", json!(result.output_values)),
        ("output_rank", json!(result.output_rank)),
        ("tensor_reads", json!(result.tensor_reads)),
        ("payload_bytes_read", json!(result.payload_bytes_read)),
        ("peak_workspace_bytes", json!(result.peak_workspace_bytes)),
        ("output_path", json!(output)),
        (
            "published_bytes",
            json!(result.output_values * size_of::<f32>() as u64),
        ),
        ("published", json!(true)),
    ])
}

fn publish(invocation: &Invocation<'_>, cancel: &std::sync::atomic::AtomicBool) -> Result<Fields> {
    let video = file(invocation, "--video-file")?;
    let audio = file(invocation, "--audio-file")?;
    let output = file(invocation, "--out")?;
    let frames = nonzero(invocation, "--frames")?;
    let width = nonzero(invocation, "--width")?;
    let height = nonzero(invocation, "--height")?;
    let channels = number(invocation, "--audio-channels", 2, false)?;
    if channels > 2 {
        return Err(invalid("media publish supports mono or stereo PCM").into());
    }
    let fps = [
        number(invocation, "--fps-numerator", 24, false)?,
        number(invocation, "--fps-denominator", 1, false)?,
    ];
    let rate = number(invocation, "--sample-rate", 32000, false)?;
    let result = media::publish(
        media::Publish {
            video: &video,
            audio: &audio,
            output: &output,
            frames,
            width,
            height,
            fps,
            channels,
            samples: nonzero(invocation, "--audio-samples")?,
            rate,
            host_bytes: number(invocation, "--max-host-bytes", 256 * 1024 * 1024, false)?,
            output_bytes: number(
                invocation,
                "--max-output-bytes",
                4 * 1024 * 1024 * 1024,
                false,
            )?,
        },
        cancel,
    )?;
    let mut fields = vec![
        ("status", json!("media-published")),
        ("container", json!("avi-bgr24-pcm-s16le")),
        ("output", json!(output)),
        ("video_frames", json!(result.video_frames)),
        ("video_width", json!(width)),
        ("video_height", json!(height)),
        ("video_fps", json!(format!("{}/{}", fps[0], fps[1]))),
        ("audio_channels", json!(channels)),
        ("audio_sample_rate", json!(rate)),
        ("audio_samples_used", json!(result.audio_samples_used)),
        ("audio_samples_trimmed", json!(result.audio_samples_trimmed)),
        ("file_bytes", json!(result.file_bytes)),
        ("file_identity", json!(ffi::text(&result.file_identity))),
        (
            "publication_identity",
            json!(ffi::text(&result.publication_identity)),
        ),
        ("video_identity", json!(ffi::text(&result.video_identity))),
        ("audio_identity", json!(ffi::text(&result.audio_identity))),
        (
            "execution_identity",
            json!(ffi::text(&result.execution_identity)),
        ),
        (
            "duration",
            json!(format!(
                "{}/{} seconds",
                result.video_duration_numerator, result.video_duration_denominator
            )),
        ),
        ("peak_workspace_bytes", json!(result.peak_workspace_bytes)),
    ];
    fields.extend(disposition(false));
    Ok(fields)
}

fn generate(invocation: &Invocation<'_>, cancel: &std::sync::atomic::AtomicBool) -> Result<Fields> {
    let output = file(invocation, "--out")?;
    let paths = [
        file(invocation, "--text-artifact")?,
        file(invocation, "--transformer-artifact")?,
        file(invocation, "--video-artifact")?,
        file(invocation, "--audio-artifact")?,
    ];
    let target = required(invocation, "--target")?;
    let result = media::generate(
        media::Generate {
            target,
            prompt: required(invocation, "--prompt")?,
            output: &output,
            artifacts: [&paths[0], &paths[1], &paths[2], &paths[3]],
            frames: nonzero(invocation, "--frames")?,
            width: nonzero(invocation, "--width")?,
            height: nonzero(invocation, "--height")?,
            fps: [
                number(invocation, "--fps-numerator", 24, false)?,
                number(invocation, "--fps-denominator", 1, false)?,
            ],
            steps: u32::try_from(number(invocation, "--steps", 1, false)?)
                .map_err(|_| invalid("--steps exceeds its unsigned 32-bit bound"))?,
            blocks: number(invocation, "--blocks", 50, false)?,
            seed: number(invocation, "--seed", 42, true)?,
            host_bytes: number(
                invocation,
                "--max-host-bytes",
                96 * 1024 * 1024 * 1024,
                false,
            )?,
            device_bytes: number(
                invocation,
                "--max-device-bytes",
                4 * 1024 * 1024 * 1024,
                false,
            )?,
            workspace_bytes: number(
                invocation,
                "--max-workspace-bytes",
                4 * 1024 * 1024 * 1024,
                false,
            )?,
            output_bytes: number(
                invocation,
                "--max-output-bytes",
                4 * 1024 * 1024 * 1024,
                false,
            )?,
        },
        cancel,
    )?;
    let mut fields = vec![
        ("status", json!("generation-complete")),
        ("target", json!(target)),
        ("output", json!(output)),
        ("prompt_tokens", json!(result.prompt_tokens)),
        ("frames", json!(result.frames)),
        (
            "geometry",
            json!(format!("{}x{}", result.width, result.height)),
        ),
        ("audio_samples", json!(result.audio_samples)),
        ("model_evaluations", json!(result.model_evaluations)),
        ("kernel_launches", json!(result.kernel_launches)),
        ("peak_device_bytes", json!(result.peak_device_bytes)),
        ("peak_workspace_bytes", json!(result.peak_workspace_bytes)),
        ("file_bytes", json!(result.file_bytes)),
        (
            "execution_identity",
            json!(ffi::text(&result.execution_identity)),
        ),
        ("file_identity", json!(ffi::text(&result.file_identity))),
        (
            "publication_identity",
            json!(ffi::text(&result.publication_identity)),
        ),
        ("prompt_identity", json!(ffi::text(&result.prompt_identity))),
        (
            "conditioning_identity",
            json!(ffi::text(&result.conditioning_identity)),
        ),
        ("plan_identity", json!(ffi::text(&result.plan_identity))),
        ("layout_identity", json!(ffi::text(&result.layout_identity))),
        ("latent_identity", json!(ffi::text(&result.latent_identity))),
        (
            "vae_input_identity",
            json!(ffi::text(&result.vae_input_identity)),
        ),
        ("video_identity", json!(ffi::text(&result.video_identity))),
        ("audio_identity", json!(ffi::text(&result.audio_identity))),
    ];
    fields.extend(disposition(true));
    Ok(fields)
}

fn disposition(end_user: bool) -> Fields {
    vec![
        ("production_capability_available", json!(true)),
        ("production_api_available", json!(true)),
        ("operator_command_available", json!(true)),
        ("end_user_path_available", json!(end_user)),
        ("cli_applicability", json!("applicable")),
    ]
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<String> {
    let mode = invocation.value("--output").unwrap_or("normal");
    if !matches!(mode, "normal" | "table" | "audit" | "json" | "csv") {
        return Err(invalid("unsupported component/media output mode").into());
    }
    let signals = Signals::new()?;
    let fields = match invocation.operation.operation_id.as_str() {
        "execute.graph.component.audio-vae" | "execute.graph.component.video-vae" => {
            component(invocation, &signals.cancel)?
        }
        "execute.media.publish" => publish(invocation, &signals.cancel)?,
        "execute.media.generate" => generate(invocation, &signals.cancel)?,
        _ => return Err(invalid("registry operation has no media adapter").into()),
    };
    if mode == "json" {
        let object = fields
            .into_iter()
            .map(|(k, v)| (k.to_string(), v))
            .collect::<serde_json::Map<_, _>>();
        return Ok(format!("{}\n", Value::Object(object)));
    }
    if mode == "csv" {
        return crate::pipeline_projection::present(&fields, mode, width, styled);
    }
    let compact = [
        "status",
        "target",
        "component",
        "backend",
        "output_values",
        "video_frames",
        "video_width",
        "video_height",
        "video_fps",
        "audio_channels",
        "audio_sample_rate",
        "audio_samples_used",
        "audio_samples_trimmed",
        "file_bytes",
        "frames",
        "geometry",
        "audio_samples",
        "output",
        "output_path",
    ];
    let rows = fields
        .iter()
        .filter(|(k, _)| mode == "audit" || compact.contains(k))
        .map(|(k, v)| {
            (
                *k,
                v.as_str()
                    .map(str::to_owned)
                    .unwrap_or_else(|| v.to_string()),
            )
        })
        .collect::<Vec<_>>();
    let views = rows
        .iter()
        .map(|(k, v)| (*k, v.as_str()))
        .collect::<Vec<_>>();
    presentation::record("MEDIA  complete", &views, width, styled).map_err(Into::into)
}
