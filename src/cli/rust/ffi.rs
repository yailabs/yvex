// Only this module crosses native ownership. No argv dispatcher or C renderer is bound.
use std::{
    ffi::{CStr, CString},
    fmt,
    ptr::NonNull,
};

/// Render the producer's wall-clock event time in UTC, independent of client TZ.
/// Missing/unrepresentable time stays explicit; never substitute client time.
pub(crate) fn event_timestamp(wall_time_ns: u64, full: bool) -> String {
    let missing = "--:--:--.---";
    if wall_time_ns == 0 {
        return missing.into();
    }
    let Ok(seconds) = raw::time_t::try_from(wall_time_ns / 1_000_000_000) else {
        return missing.into();
    };
    let mut clock = raw::tm::default();
    let mut stamp = [0; 32];
    let format = if full {
        c"%Y-%m-%dT%H:%M:%S"
    } else {
        c"%H:%M:%S"
    };
    // POSIX writes only to our initialized tm and bounded caller-owned buffer.
    if unsafe { raw::gmtime_r(&seconds, &mut clock) }.is_null()
        || unsafe { raw::strftime(stamp.as_mut_ptr(), stamp.len(), format.as_ptr(), &clock) } == 0
    {
        return missing.into();
    }
    format!("{}.{:03}Z", text(&stamp), wall_time_ns / 1_000_000 % 1000)
}

pub(crate) mod acquisition;
mod artifact;
pub(crate) mod catalog;
pub(crate) mod distribution;
pub(crate) mod execution;
pub(crate) mod finite;
pub(crate) mod generation;
pub(crate) mod media;
pub(crate) mod pipeline;
pub(crate) mod preparation;
pub(crate) use catalog::{RemoteRequest, remote_catalog};
mod quant;
mod registry;
mod source;
pub(crate) mod target;
pub(crate) mod variant;
pub(crate) use variant::{Variant, VariantRequest};
pub(crate) fn tensor_role(role: raw::yvex_tensor_role) -> String {
    unsafe { borrowed_text(raw::yvex_tensor_role_name(role)).unwrap_or_else(|_| "unknown".into()) }
}
pub(crate) fn explicit_token_count(value: &str) -> Result<u64, Error> {
    let value = argument(Some(value))?.expect("explicit token list");
    let mut input = Box::<raw::yvex_token_input>::default();
    let mut failure = raw::yvex_error::default();
    execution::checked(
        unsafe {
            raw::yvex_token_input_parse_explicit(value.as_ptr(), input.as_mut(), &mut failure)
        },
        &failure,
    )?;
    if input.token_count as usize > input.tokens.len() {
        return Err(extent_error());
    }
    Ok(input.token_count)
}
pub(crate) fn quant_rmse(metrics: &raw::yvex_quant_metrics) -> f64 {
    unsafe { raw::yvex_quant_metrics_rmse(metrics) }
}
pub(crate) use artifact::{ConversionReport, ConversionRequest, artifact_convert, qtype_support};
pub(crate) use artifact::{EmitRequest, artifact_emit, artifact_template};
pub(crate) use artifact::{GateExpected, GateRequest, artifact_gate};
pub(crate) use artifact::{MappingRequest, tensor_mapping};
pub(crate) use quant::{
    ImatrixInput, ImatrixOptions, JobInput, JobOptions, PolicyInput, imatrix, policy,
    policy_presets, quant_job,
};
pub(crate) use registry::Reference;
pub(crate) use registry::default_path as registry_default_path;
pub(crate) use registry::{
    Deployment, ProfileRequest, profile_create, profile_replace, profile_verify,
};
pub(crate) use source::native_weights;
pub(crate) use source::{
    ManifestRequest, ReportRequest, SourceReport, source_manifest, source_report,
    source_verification_target, source_verify,
};

pub(crate) fn snapshot(path: &str, maximum: usize) -> Result<Vec<u8>, Error> {
    let path = argument(Some(path))?.expect("required snapshot path");
    let mut bytes = std::ptr::null_mut();
    let mut count = 0;
    let mut result = raw::yvex_core_file_result::default();
    let mut failure = raw::yvex_error::default();
    let status = unsafe {
        raw::yvex_core_file_read_snapshot(
            path.as_ptr(),
            maximum,
            &mut bytes,
            &mut count,
            &mut result,
            &mut failure,
        )
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    // Native snapshots use the observed core allocator, not the Rust allocator.
    let copied = if count > maximum {
        Err(extent_error())
    } else {
        unsafe { copy_view(bytes, count as u64) }
    };
    unsafe {
        raw::yvex_core_allocate(
            raw::yvex_core_allocation_operation_YVEX_CORE_ALLOCATE_FREE,
            bytes.cast(),
            0,
            0,
        );
    }
    copied
}

pub(crate) fn publish(path: &str, bytes: &[u8], replace: bool) -> Result<(), Error> {
    let path = argument(Some(path))?.expect("required publication path");
    let mut result = raw::yvex_core_file_result::default();
    let mut failure = raw::yvex_error::default();
    // The leaf mechanism owns no-clobber/replace, private mode, sync and cleanup.
    let status = unsafe {
        if replace {
            raw::yvex_core_file_publish_replace(
                path.as_ptr(),
                bytes.as_ptr().cast(),
                bytes.len(),
                None,
                std::ptr::null_mut(),
                &mut result,
                &mut failure,
            )
        } else {
            raw::yvex_core_file_publish_noreplace(
                path.as_ptr(),
                bytes.as_ptr().cast(),
                bytes.len(),
                std::ptr::null(),
                None,
                std::ptr::null_mut(),
                &mut result,
                &mut failure,
            )
        }
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(())
}

pub(crate) fn digest(bytes: &[u8]) -> Result<String, Error> {
    let mut digest = [0; raw::YVEX_SHA256_HEX_CAP as usize];
    let mut failure = raw::yvex_error::default();
    let status = unsafe {
        raw::yvex_artifact_sha256_hex_bytes(
            bytes.as_ptr(),
            bytes.len() as u64,
            digest.as_mut_ptr(),
            &mut failure,
        )
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(text(&digest))
}

pub(crate) fn backend_kind(name: &str) -> Result<raw::yvex_backend_kind, Error> {
    let name = argument(Some(name))?.expect("required backend");
    let mut kind = raw::yvex_backend_kind_YVEX_BACKEND_KIND_CPU;
    let mut failure = raw::yvex_error::default();
    let status = unsafe { raw::yvex_backend_kind_parse(name.as_ptr(), &mut kind, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(kind)
}

pub(crate) fn backend_preflight(name: &str) -> Result<(String, bool), Error> {
    let kind = backend_kind(name)?;
    let mut failure = raw::yvex_error::default();
    let options = raw::yvex_backend_options {
        kind,
        ..Default::default()
    };
    let mut backend = std::ptr::null_mut();
    let status = unsafe { raw::yvex_backend_open(&mut backend, &options, &mut failure) };
    if status != 0 {
        if !backend.is_null() {
            let mut cleanup = raw::yvex_error::default();
            let cleanup_status =
                unsafe { raw::yvex_backend_close_checked(&mut backend, &mut cleanup) };
            if cleanup_status != 0 {
                return Err(error(cleanup_status, &cleanup));
            }
        }
        return Err(error(status, &failure));
    }
    let state = unsafe { raw::yvex_backend_status_of(backend) };
    let supported = unsafe {
        raw::yvex_backend_supports(
            backend,
            raw::yvex_backend_capability_YVEX_BACKEND_CAP_TENSOR_ALLOC,
        ) != 0
            && raw::yvex_backend_supports(
                backend,
                raw::yvex_backend_capability_YVEX_BACKEND_CAP_TENSOR_READ_WRITE,
            ) != 0
    };
    let status = unsafe { raw::yvex_backend_close_checked(&mut backend, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok((backend_status_name(state), supported))
}

#[allow(
    non_camel_case_types,
    non_snake_case,
    non_upper_case_globals,
    dead_code
)]
pub(crate) mod raw {
    include!(concat!(env!("OUT_DIR"), "/ffi.rs"));
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Error {
    pub code: i32,
    pub owner: String,
    pub message: String,
}

impl fmt::Display for Error {
    fn fmt(&self, out: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            out,
            "{} · {}: {}",
            status_name(self.code),
            self.owner,
            self.message
        )
    }
}
impl std::error::Error for Error {}

pub(crate) struct Paths {
    pub base: raw::yvex_paths,
    pub operator: raw::yvex_operator_paths,
}

pub(crate) fn profile_remove(alias: &str, registry: Option<&str>) -> Result<(), Error> {
    profile_remove_exact(alias, None, registry)
}
pub(crate) fn profile_remove_exact(
    alias: &str,
    expected: Option<&str>,
    registry: Option<&str>,
) -> Result<(), Error> {
    let alias = argument(Some(alias))?.expect("required alias");
    let expected = argument(expected)?;
    let registry = argument(registry)?;
    let mut failure = raw::yvex_error::default();
    let status = unsafe {
        raw::yvex_model_registry_remove_exact(
            alias.as_ptr(),
            pointer(&expected),
            pointer(&registry),
            &mut failure,
        )
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(())
}

pub(crate) struct ProfileCandidate {
    pub alias: String,
    pub family: String,
    pub model: String,
    pub scope: String,
    pub class: String,
    pub path: String,
    pub calibration: String,
    pub qprofile: String,
}

pub(crate) fn profile_scan(root: &str) -> Result<Vec<ProfileCandidate>, Error> {
    let root = argument(Some(root))?.expect("required scan root");
    let mut entries = std::ptr::null_mut();
    let mut count = 0;
    let mut failure = raw::yvex_error::default();
    let status = unsafe {
        raw::yvex_model_registry_scan_root(root.as_ptr(), &mut entries, &mut count, &mut failure)
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    let result = unsafe { copy_view(entries, count) }.map(|entries| {
        entries
            .into_iter()
            .map(|entry| {
                // Scan entries own NUL-terminated strings until scan_free; copy every selected fact first.
                let string = |pointer: *const std::ffi::c_char| {
                    if pointer.is_null() {
                        String::new()
                    } else {
                        unsafe { CStr::from_ptr(pointer) }
                            .to_string_lossy()
                            .into_owned()
                    }
                };
                ProfileCandidate {
                    alias: string(entry.alias),
                    family: string(entry.family),
                    model: string(entry.model),
                    scope: string(entry.scope),
                    class: string(entry.artifact_class),
                    path: string(entry.path),
                    calibration: string(entry.calibration),
                    qprofile: string(entry.qprofile),
                }
            })
            .collect()
    });
    unsafe {
        raw::yvex_model_registry_scan_free(entries, count);
    }
    result
}

impl Paths {
    pub(crate) fn with_models_root(root: Option<&str>) -> Result<Self, Error> {
        let mut paths = Self::open(None)?;
        let root = argument(root)?;
        let mut failure = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_operator_paths_resolve(
                &paths.base,
                pointer(&root),
                &mut paths.operator,
                &mut failure,
            )
        };
        if status != 0 {
            return Err(error(status, &failure));
        }
        Ok(paths)
    }
    pub(crate) fn open(project: Option<&str>) -> Result<Self, Error> {
        let project = argument(project)?;
        let mut paths = Self {
            base: raw::yvex_paths::default(),
            operator: raw::yvex_operator_paths::default(),
        };
        let mut failure = raw::yvex_error::default();
        // All destinations are caller-owned records; native resolution retains no pointer.
        let status = unsafe {
            if let Some(project) = project {
                raw::yvex_paths_project(&mut paths.base, project.as_ptr(), &mut failure)
            } else {
                raw::yvex_paths_default(&mut paths.base, &mut failure)
            }
        };
        if status != 0 {
            return Err(error(status, &failure));
        }
        let status = unsafe {
            raw::yvex_operator_paths_resolve(
                &paths.base,
                std::ptr::null(),
                &mut paths.operator,
                &mut failure,
            )
        };
        if status != 0 {
            return Err(error(status, &failure));
        }
        Ok(paths)
    }

    pub(crate) fn configure(&mut self, root: &str, create: bool) -> Result<(), Error> {
        let root = argument(Some(root))?.expect("required root");
        let mut failure = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_operator_paths_configure(
                &self.base,
                root.as_ptr(),
                create.into(),
                &mut self.operator,
                &mut failure,
            )
        };
        if status != 0 {
            return Err(error(status, &failure));
        }
        Ok(())
    }

    pub(crate) fn reset(&mut self) -> Result<bool, Error> {
        let mut removed = 0;
        let mut failure = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_operator_paths_reset(
                &self.base,
                &mut removed,
                &mut self.operator,
                &mut failure,
            )
        };
        if status != 0 {
            return Err(error(status, &failure));
        }
        Ok(removed != 0)
    }

    pub(crate) fn create(&self) -> Result<(), Error> {
        let mut failure = raw::yvex_error::default();
        let status = unsafe { raw::yvex_operator_paths_create(&self.operator, &mut failure) };
        if status != 0 {
            return Err(error(status, &failure));
        }
        Ok(())
    }

    pub(crate) fn resolve(&self, family: &str, kind: &str) -> Result<(String, bool), Error> {
        let family = argument(Some(family))?.expect("required family");
        let kind = argument(Some(kind))?.expect("required kind");
        let mut destination = [0; raw::YVEX_PATH_CAP as usize];
        let mut exists = 0;
        let mut failure = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_operator_paths_resolve_target(
                &self.operator,
                family.as_ptr(),
                kind.as_ptr(),
                destination.as_mut_ptr(),
                destination.len(),
                &mut exists,
                &mut failure,
            )
        };
        if status != 0 {
            return Err(error(status, &failure));
        }
        Ok((text(&destination), exists != 0))
    }

    pub(crate) fn run(&self, create: bool) -> Result<raw::yvex_run_dir, Error> {
        let mut run = raw::yvex_run_dir::default();
        let mut failure = raw::yvex_error::default();
        let mut status = unsafe {
            raw::yvex_run_dir_prepare(&mut run, &self.base, std::ptr::null(), &mut failure)
        };
        if status == 0 && create {
            status = unsafe { raw::yvex_run_dir_create(&run, &mut failure) };
        }
        if status != 0 {
            return Err(error(status, &failure));
        }
        Ok(run)
    }
}

pub(crate) fn text(bytes: &[std::ffi::c_char]) -> String {
    let extent = bytes
        .iter()
        .position(|value| *value == 0)
        .unwrap_or(bytes.len());
    let bytes: Vec<u8> = bytes[..extent]
        .iter()
        .map(|value| value.to_ne_bytes()[0])
        .collect();
    String::from_utf8_lossy(&bytes).into_owned()
}

fn error(status: i32, native: &raw::yvex_error) -> Error {
    Error {
        code: status,
        owner: text(&native.where_),
        message: text(&native.message),
    }
}

pub fn version() -> String {
    // C owns this immutable, NUL-terminated static string for the process lifetime.
    unsafe { CStr::from_ptr(raw::yvex_version_string()) }
        .to_string_lossy()
        .into_owned()
}

pub const LOCAL_PROTOCOL_VERSION: u32 = raw::YVEX_LOCAL_PROTOCOL_VERSION;

pub(crate) fn token_input(
    model: &str,
    explicit: Option<&str>,
    ids: &[u32],
) -> Result<raw::yvex_token_input, Error> {
    let model = argument(Some(model))?.expect("required model path");
    let explicit = argument(explicit)?;
    let mut input = Box::<raw::yvex_token_input>::default();
    let mut failure = raw::yvex_error::default();
    let status = unsafe {
        if let Some(value) = &explicit {
            raw::yvex_token_input_parse_explicit(value.as_ptr(), input.as_mut(), &mut failure)
        } else {
            raw::yvex_token_input_from_ids(
                raw::yvex_token_input_kind_YVEX_TOKEN_INPUT_PROMPT_TEXT,
                ids.as_ptr(),
                ids.len() as u64,
                input.as_mut(),
                &mut failure,
            )
        }
    };
    execution::checked(status, &failure)?;
    let mut vocabulary = 0;
    let status = unsafe {
        raw::yvex_model_context_vocab_size(model.as_ptr(), &mut vocabulary, &mut failure)
    };
    execution::checked(status, &failure)?;
    let status =
        unsafe { raw::yvex_token_input_validate_bounds(input.as_mut(), vocabulary, &mut failure) };
    execution::checked(status, &failure)?;
    if input.token_count as usize > input.tokens.len() {
        return Err(extent_error());
    }
    Ok(*input)
}

pub(crate) struct ModelView {
    native: raw::yvex_model_context,
}

pub(crate) struct DescriptorInfo {
    pub header: raw::yvex_gguf_header,
    pub architecture: String,
    pub name: String,
    pub alignment: u32,
    pub data_offset: u64,
    pub known_bytes: u64,
    pub unsupported_accounting: u64,
    pub context_length: u64,
}

pub(crate) struct MetadataEntry {
    pub key: String,
    pub value: MetadataValue,
}

pub(crate) enum MetadataValue {
    Unsigned(u64),
    Signed(i64),
    Float(f64),
    Boolean(bool),
    String(Vec<u8>),
    Array { element_type: String, count: u64 },
}

pub(crate) struct TensorInfo {
    pub name: String,
    pub role: String,
    pub role_id: raw::yvex_tensor_role,
    pub dtype: String,
    pub dimensions: Vec<u64>,
    pub storage_bytes: u64,
    pub relative_offset: u64,
    pub absolute_offset: u64,
    pub range: Option<raw::yvex_tensor_range>,
}

pub(crate) struct TokenizerInfo {
    pub architecture: String,
    pub name: String,
    pub kind: String,
    pub support: String,
    pub vocabulary: u64,
    pub plan: Option<raw::yvex_tokenizer_plan_summary>,
    pub specials: [Option<u32>; 4],
    pub chat_template: bool,
}

pub(crate) struct Encoding {
    pub ids: Vec<u32>,
    pub identity: String,
    pub runtime: bool,
    pub pieces: Vec<Vec<u8>>,
}

pub(crate) struct Decoding {
    pub bytes: Vec<u8>,
    pub identity: String,
    pub runtime: bool,
}

fn extent_error() -> Error {
    Error {
        code: raw::yvex_status_YVEX_ERR_BOUNDS,
        owner: "native.view".into(),
        message: "native result has an invalid pointer or extent".into(),
    }
}

// The caller proves the pointer is borrowed from a live successful native result.
unsafe fn copy_view<T: Copy>(pointer: *const T, count: u64) -> Result<Vec<T>, Error> {
    let count = usize::try_from(count).map_err(|_| extent_error())?;
    if count == 0 {
        return Ok(Vec::new());
    }
    if pointer.is_null() || count > isize::MAX as usize / std::mem::size_of::<T>() {
        return Err(extent_error());
    }
    // The native allocation is live, aligned and contains the admitted count.
    Ok(unsafe { std::slice::from_raw_parts(pointer, count) }.to_vec())
}

// Only call with a NUL-terminated string borrowed from a successful live owner.
unsafe fn borrowed_text(pointer: *const std::ffi::c_char) -> Result<String, Error> {
    if pointer.is_null() {
        return Err(extent_error());
    }
    Ok(unsafe { CStr::from_ptr(pointer) }
        .to_string_lossy()
        .into_owned())
}

unsafe fn metadata_value(value: *const raw::yvex_gguf_value) -> Result<MetadataValue, Error> {
    if value.is_null() {
        return Err(extent_error());
    }
    // The parser has admitted this immutable value; each getter validates its type.
    unsafe {
        let kind = raw::yvex_gguf_value_type_of(value);
        match kind {
            raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_UINT8
            | raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_UINT16
            | raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_UINT32
            | raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_UINT64 => {
                let mut result = 0;
                if raw::yvex_gguf_value_as_u64(value, &mut result) != 0 {
                    return Err(extent_error());
                }
                Ok(MetadataValue::Unsigned(result))
            }
            raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_INT8
            | raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_INT16
            | raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_INT32
            | raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_INT64 => {
                let mut result = 0;
                if raw::yvex_gguf_value_as_i64(value, &mut result) != 0 {
                    return Err(extent_error());
                }
                Ok(MetadataValue::Signed(result))
            }
            raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_FLOAT32
            | raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_FLOAT64 => {
                let mut result = 0.0;
                if raw::yvex_gguf_value_as_f64(value, &mut result) != 0 {
                    return Err(extent_error());
                }
                Ok(MetadataValue::Float(result))
            }
            raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_BOOL => {
                let mut result = 0;
                if raw::yvex_gguf_value_as_bool(value, &mut result) != 0 {
                    return Err(extent_error());
                }
                Ok(MetadataValue::Boolean(result != 0))
            }
            raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_STRING => {
                let mut data = std::ptr::null();
                let mut size = 0;
                if raw::yvex_gguf_value_as_string(value, &mut data, &mut size) != 0 {
                    return Err(extent_error());
                }
                Ok(MetadataValue::String(copy_view(data.cast::<u8>(), size)?))
            }
            raw::yvex_gguf_value_type_YVEX_GGUF_VALUE_ARRAY => {
                let mut result = raw::yvex_gguf_array_info::default();
                if raw::yvex_gguf_value_array_info(value, &mut result) != 0 {
                    return Err(extent_error());
                }
                Ok(MetadataValue::Array {
                    element_type: borrowed_text(raw::yvex_gguf_value_type_name(
                        result.element_type,
                    ))?,
                    count: result.count,
                })
            }
            _ => Err(extent_error()),
        }
    }
}

impl ModelView {
    pub(crate) fn role_counts(&self) -> Vec<(String, u64)> {
        (0..raw::yvex_tensor_role_YVEX_TENSOR_ROLE_COUNT)
            .map(|role| {
                (tensor_role(role), unsafe {
                    raw::yvex_model_role_count(self.native.model, role)
                })
            })
            .collect()
    }
    pub(crate) fn open(path: &str) -> Result<Self, Error> {
        let path = argument(Some(path))?.expect("required path");
        let mut view = Self {
            native: raw::yvex_model_context::default(),
        };
        let mut failure = raw::yvex_error::default();
        // The native owner authenticates the artifact and retains its read-only views.
        let status =
            unsafe { raw::yvex_model_context_open(path.as_ptr(), &mut view.native, &mut failure) };
        if status != 0 {
            return Err(error(status, &failure));
        }
        if view.native.model.is_null() || view.native.gguf.is_null() || view.native.table.is_null()
        {
            return Err(extent_error());
        }
        Ok(view)
    }

    pub(crate) fn tokenizer(path: &str) -> Result<Self, Error> {
        let mut view = Self::open(path)?;
        let mut failure = raw::yvex_error::default();
        let mut status = 0;
        if status == 0 {
            status = unsafe {
                raw::yvex_family_tokenizer_open(
                    &mut view.native.tokenizer,
                    view.native.gguf,
                    &mut failure,
                )
            };
            if status == raw::yvex_status_YVEX_ERR_UNSUPPORTED {
                status = unsafe {
                    raw::yvex_tokenizer_from_gguf(
                        &mut view.native.tokenizer,
                        view.native.gguf,
                        view.native.model,
                        &mut failure,
                    )
                };
            }
        }
        if status != 0 {
            return Err(error(status, &failure));
        }
        if view.native.tokenizer.is_null() || view.native.model.is_null() {
            return Err(extent_error());
        }
        Ok(view)
    }

    pub(crate) fn descriptor(&self) -> Result<DescriptorInfo, Error> {
        // The context retains an admitted immutable descriptor and container.
        unsafe {
            Ok(DescriptorInfo {
                header: copied(raw::yvex_gguf_header_view(self.native.gguf))?,
                architecture: borrowed_text(raw::yvex_arch_name(raw::yvex_model_arch(
                    self.native.model,
                )))?,
                name: borrowed_text(raw::yvex_model_name(self.native.model))?,
                alignment: raw::yvex_gguf_alignment(self.native.gguf),
                data_offset: raw::yvex_gguf_tensor_data_offset(self.native.gguf),
                known_bytes: raw::yvex_model_total_storage_bytes(self.native.model),
                unsupported_accounting: raw::yvex_model_unsupported_tensor_accounting_count(
                    self.native.model,
                ),
                context_length: raw::yvex_model_context_length(self.native.model),
            })
        }
    }

    pub(crate) fn metadata(&self) -> Result<Vec<MetadataEntry>, Error> {
        let mut entries = Vec::new();
        // Every getter borrows the live immutable container. Copy before closing.
        unsafe {
            for index in 0..raw::yvex_gguf_metadata_count(self.native.gguf) {
                entries.push(MetadataEntry {
                    key: borrowed_text(raw::yvex_gguf_metadata_key(self.native.gguf, index))?,
                    value: metadata_value(raw::yvex_gguf_metadata_value(self.native.gguf, index))?,
                });
            }
        }
        Ok(entries)
    }

    pub(crate) fn tensors(&self) -> Result<Vec<TensorInfo>, Error> {
        let mut entries = Vec::new();
        // Native table and range admission own role, dtype, geometry and validity.
        unsafe {
            for index in 0..raw::yvex_tensor_table_count(self.native.table) {
                let tensor = raw::yvex_tensor_table_at(self.native.table, index);
                let view = copied(tensor)?;
                if view.rank as usize > view.dims.len() {
                    return Err(extent_error());
                }
                let mut range = raw::yvex_tensor_range::default();
                let mut failure = raw::yvex_error::default();
                let status = raw::yvex_tensor_range_validate(
                    self.native.artifact,
                    self.native.gguf,
                    tensor,
                    &mut range,
                    &mut failure,
                );
                entries.push(TensorInfo {
                    name: borrowed_text(view.name)?,
                    role: borrowed_text(raw::yvex_tensor_role_name(view.role))?,
                    role_id: view.role,
                    dtype: borrowed_text(raw::yvex_dtype_name(view.dtype))?,
                    dimensions: view.dims[..view.rank as usize].to_vec(),
                    storage_bytes: view.storage_bytes,
                    relative_offset: view.relative_offset,
                    absolute_offset: view.absolute_offset,
                    range: (status == 0).then_some(range),
                });
            }
        }
        Ok(entries)
    }

    pub(crate) fn info(&self) -> TokenizerInfo {
        // All pointers refer to immutable data owned by this live model context.
        unsafe {
            let tokenizer = self.native.tokenizer;
            let mut specials = [None; 4];
            for (slot, function) in specials.iter_mut().zip([
                raw::yvex_tokenizer_bos_id,
                raw::yvex_tokenizer_eos_id,
                raw::yvex_tokenizer_pad_id,
                raw::yvex_tokenizer_unk_id,
            ]) {
                let mut id = 0;
                if function(tokenizer, &mut id) == 0 {
                    *slot = Some(id);
                }
            }
            let mut template = std::ptr::null();
            let mut bytes = 0;
            let template_status =
                raw::yvex_tokenizer_chat_template(tokenizer, &mut template, &mut bytes);
            TokenizerInfo {
                architecture: CStr::from_ptr(raw::yvex_arch_name(raw::yvex_model_arch(
                    self.native.model,
                )))
                .to_string_lossy()
                .into_owned(),
                name: CStr::from_ptr(raw::yvex_model_name(self.native.model))
                    .to_string_lossy()
                    .into_owned(),
                kind: CStr::from_ptr(raw::yvex_tokenizer_kind_name(raw::yvex_tokenizer_kind_of(
                    tokenizer,
                )))
                .to_string_lossy()
                .into_owned(),
                support: CStr::from_ptr(raw::yvex_tokenizer_support_name(
                    raw::yvex_tokenizer_support_of(tokenizer),
                ))
                .to_string_lossy()
                .into_owned(),
                vocabulary: raw::yvex_tokenizer_vocab_size(tokenizer),
                plan: raw::yvex_tokenizer_plan_summary_get(tokenizer)
                    .as_ref()
                    .copied(),
                specials,
                chat_template: template_status == 0 && !template.is_null() && bytes != 0,
            }
        }
    }

    pub(crate) fn piece(&self, id: u32) -> Result<Vec<u8>, Error> {
        // The token view is immutable and its byte storage shares this context lifetime.
        let token =
            unsafe { raw::yvex_tokenizer_token_at(self.native.tokenizer, id.into()).as_ref() }
                .ok_or_else(extent_error)?;
        unsafe { copy_view(token.text.cast::<u8>(), token.text_len) }
    }

    pub(crate) fn encode(
        &self,
        input: &[u8],
        bos: bool,
        eos: bool,
        pieces: bool,
    ) -> Result<Encoding, Error> {
        let mut encoded = raw::yvex_tokenizer_encode_result::default();
        let mut fixture = raw::yvex_tokens::default();
        let mut failure = raw::yvex_error::default();
        let runtime = self.info().plan.is_some();
        let options = raw::yvex_tokenizer_encode_options {
            add_bos: bos.into(),
            add_eos: eos.into(),
            allow_special_tokens: 1,
            maximum_tokens: u64::MAX,
        };
        let result = (|| {
            let status = if runtime {
                // Caller bytes and output record are synchronous bounded borrows.
                unsafe {
                    raw::yvex_tokenizer_encode(
                        self.native.tokenizer,
                        input.as_ptr(),
                        input.len() as _,
                        &options,
                        &mut encoded,
                        &mut failure,
                    )
                }
            } else if bos || eos {
                return Err(Error {
                    code: raw::yvex_status_YVEX_ERR_UNSUPPORTED,
                    owner: "tokenizer.special-policy".into(),
                    message: "fixture tokenizers do not admit explicit BOS/EOS insertion".into(),
                });
            } else {
                let input = CString::new(input).map_err(|_| extent_error())?;
                unsafe {
                    raw::yvex_tokenize_text(
                        self.native.tokenizer,
                        input.as_ptr(),
                        &mut fixture,
                        &mut failure,
                    )
                }
            };
            if status != 0 {
                return Err(error(status, &failure));
            }
            let tokens = if runtime { &encoded.tokens } else { &fixture };
            let ids = unsafe { copy_view(tokens.ids, tokens.len) }?;
            let pieces = if pieces {
                ids.iter()
                    .map(|id| self.piece(*id))
                    .collect::<Result<_, _>>()?
            } else {
                Vec::new()
            };
            Ok(Encoding {
                ids,
                pieces,
                runtime,
                identity: if runtime {
                    text(&encoded.encoding_identity)
                } else {
                    "unavailable".into()
                },
            })
        })();
        // Clear both owned native results on success and on every refusal/copy failure.
        unsafe {
            raw::yvex_tokenizer_encode_result_clear(&mut encoded);
            raw::yvex_tokens_clear(&mut fixture);
        }
        result
    }

    pub(crate) fn decode(&self, ids: &[u32]) -> Result<Decoding, Error> {
        let mut decoded = raw::yvex_tokenizer_decode_result::default();
        let mut failure = raw::yvex_error::default();
        let runtime = self.info().plan.is_some();
        let result = (|| {
            if runtime {
                let options = raw::yvex_tokenizer_decode_options::default();
                // This context owns the tokenizer; input IDs remain borrowed until return.
                let status = unsafe {
                    raw::yvex_tokenizer_decode(
                        self.native.tokenizer,
                        ids.as_ptr(),
                        ids.len() as _,
                        &options,
                        &mut decoded,
                        &mut failure,
                    )
                };
                if status != 0 {
                    return Err(error(status, &failure));
                }
                Ok(Decoding {
                    bytes: unsafe { copy_view(decoded.bytes, decoded.byte_count) }?,
                    identity: text(&decoded.decoder_identity),
                    runtime,
                })
            } else {
                let capacity = ids.iter().try_fold(1usize, |count, id| {
                    count
                        .checked_add(self.piece(*id)?.len())
                        .ok_or_else(extent_error)
                })?;
                let mut bytes = vec![0 as std::ffi::c_char; capacity];
                let status = unsafe {
                    raw::yvex_detokenize_ids(
                        self.native.tokenizer,
                        ids.as_ptr(),
                        ids.len() as _,
                        bytes.as_mut_ptr(),
                        bytes.len() as _,
                        &mut failure,
                    )
                };
                if status != 0 {
                    return Err(error(status, &failure));
                }
                let bytes = bytes
                    .into_iter()
                    .take_while(|byte| *byte != 0)
                    .map(|byte| byte.to_ne_bytes()[0])
                    .collect();
                Ok(Decoding {
                    bytes,
                    identity: "unavailable".into(),
                    runtime,
                })
            }
        })();
        unsafe {
            raw::yvex_tokenizer_decode_result_clear(&mut decoded);
        }
        result
    }

    pub(crate) fn prompt(
        &self,
        messages: &[(raw::yvex_prompt_role, String)],
        thinking: bool,
        generation: bool,
    ) -> Result<(Vec<u8>, String), Error> {
        if messages.is_empty() || messages.len() > 16 {
            return Err(extent_error());
        }
        let texts = messages
            .iter()
            .map(|(_, text)| CString::new(text.as_str()))
            .collect::<Result<Vec<_>, _>>()
            .map_err(|_| extent_error())?;
        let messages = messages
            .iter()
            .zip(&texts)
            .map(|((role, _), text)| raw::yvex_prompt_message {
                schema_version: raw::YVEX_PROMPT_MESSAGE_SCHEMA_V1,
                role: *role,
                content: text.as_ptr(),
                content_len: text.as_bytes().len() as _,
                ..Default::default()
            })
            .collect::<Vec<_>>();
        let options = raw::yvex_prompt_options {
            add_bos: 1,
            add_generation_prompt: generation.into(),
            drop_thinking: 1,
            mode: if thinking {
                raw::yvex_prompt_mode_YVEX_PROMPT_MODE_THINKING
            } else {
                raw::yvex_prompt_mode_YVEX_PROMPT_MODE_CHAT
            },
            ..Default::default()
        };
        let mut rendered = raw::yvex_rendered_prompt::default();
        let mut failure = raw::yvex_error::default();
        // Messages and all text pointers are immutable borrows alive through this call.
        let status = unsafe {
            raw::yvex_prompt_render(
                &mut rendered,
                self.native.tokenizer,
                messages.as_ptr(),
                messages.len() as _,
                &options,
                &mut failure,
            )
        };
        let result = if status == 0 {
            unsafe { copy_view(rendered.text.cast::<u8>(), rendered.len) }
                .map(|bytes| (bytes, text(&rendered.prompt_identity)))
        } else {
            Err(error(status, &failure))
        };
        unsafe {
            raw::yvex_rendered_prompt_free(&mut rendered);
        }
        result
    }
}

impl Drop for ModelView {
    fn drop(&mut self) {
        // Context teardown retires tokenizer/model/table/mapping in native ownership order.
        unsafe {
            raw::yvex_model_context_close(&mut self.native);
        }
    }
}

pub(crate) fn status_name(status: i32) -> String {
    // Canonical status spellings are immutable native data.
    unsafe { CStr::from_ptr(raw::yvex_status_name(status)) }
        .to_string_lossy()
        .into_owned()
}

pub(crate) fn backend_report(
    request: &raw::yvex_backend_report_request,
) -> Result<Box<raw::yvex_backend_report>, Error> {
    let mut report = Box::<raw::yvex_backend_report>::new_uninit();
    let mut failure = raw::yvex_error::default();
    // Synchronous report construction closes its temporary backend before return.
    let status =
        unsafe { raw::yvex_backend_report_build(request, report.as_mut_ptr(), &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    let report = unsafe { report.assume_init() };
    if report.variant_count as usize > report.variants.len()
        || report.bandwidth.sample_count as usize > report.bandwidth.stream_elapsed_ns.len()
    {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_FORMAT,
            owner: "backend.report".into(),
            message: "native report exceeds bounded extent".into(),
        });
    }
    Ok(report)
}

pub(crate) fn backend_status_name(status: raw::yvex_backend_status) -> String {
    unsafe { CStr::from_ptr(raw::yvex_backend_status_name(status)) }
        .to_string_lossy()
        .into_owned()
}

pub(crate) fn capability_name(capability: raw::yvex_backend_capability) -> String {
    unsafe { CStr::from_ptr(raw::yvex_backend_capability_name(capability)) }
        .to_string_lossy()
        .into_owned()
}

pub(crate) fn variant_name(variant: raw::yvex_backend_operation_variant) -> String {
    unsafe { CStr::from_ptr(raw::yvex_backend_operation_variant_name(variant)) }
        .to_string_lossy()
        .into_owned()
}

pub(crate) fn capability_state_name(state: raw::yvex_backend_capability_state) -> String {
    unsafe { CStr::from_ptr(raw::yvex_backend_capability_state_name(state)) }
        .to_string_lossy()
        .into_owned()
}

pub(crate) fn capability_reason_name(reason: raw::yvex_backend_capability_reason) -> String {
    unsafe { CStr::from_ptr(raw::yvex_backend_capability_reason_name(reason)) }
        .to_string_lossy()
        .into_owned()
}

pub(crate) fn bundle_admission_name(admission: raw::yvex_backend_bundle_admission) -> String {
    unsafe { CStr::from_ptr(raw::yvex_backend_bundle_admission_name(admission)) }
        .to_string_lossy()
        .into_owned()
}

pub(crate) fn put_text<const N: usize>(
    out: &mut [std::ffi::c_char; N],
    value: &str,
) -> Result<(), Error> {
    if value.len() >= N || value.as_bytes().contains(&0) {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_BOUNDS,
            owner: "client.text".into(),
            message: "text exceeds the native field bound or contains NUL".into(),
        });
    }
    out.fill(0);
    for (target, byte) in out.iter_mut().zip(value.as_bytes()) {
        *target = std::ffi::c_char::from_ne_bytes([*byte]);
    }
    Ok(())
}

pub(crate) fn session_state_name(state: raw::yvex_server_session_state) -> String {
    // The C owner returns a process-lifetime static spelling for every enum value.
    unsafe { CStr::from_ptr(raw::yvex_server_session_state_name(state)) }
        .to_string_lossy()
        .into_owned()
}

pub(crate) fn backend_name(backend: raw::yvex_backend_kind) -> String {
    // Unknown kinds remain explicit; no CUDA/CPU inference from an alias.
    unsafe { CStr::from_ptr(raw::yvex_backend_kind_name(backend)) }
        .to_string_lossy()
        .into_owned()
}

pub(crate) struct Client {
    native: NonNull<raw::yvex_client>,
    next_request: u64,
}

impl Client {
    pub(crate) fn connect(socket: Option<&str>) -> Result<Self, Error> {
        let socket = socket.map(CString::new).transpose().map_err(|_| Error {
            code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
            owner: "client.socket".into(),
            message: "socket path contains NUL".into(),
        })?;
        let mut native = std::ptr::null_mut();
        let mut report = raw::yvex_error::default();
        // connect copies the optional path and returns one independently owned handle.
        let status = unsafe {
            raw::yvex_client_connect(
                &mut native,
                socket
                    .as_ref()
                    .map_or(std::ptr::null(), |value| value.as_ptr()),
                &mut report,
            )
        };
        if status != 0 {
            return Err(error(status, &report));
        }
        let native = NonNull::new(native).ok_or_else(|| Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "client.connect".into(),
            message: "successful connect returned no handle".into(),
        })?;
        Ok(Self {
            native,
            next_request: 1,
        })
    }

    pub(crate) fn request(
        &mut self,
        operation: raw::yvex_client_operation,
    ) -> raw::yvex_client_request {
        let mut defaults = raw::yvex_provider_request::default();
        // The C provider owner, not the shell, defines the sampling defaults.
        unsafe { raw::yvex_provider_request_default(&mut defaults) };
        let request = raw::yvex_client_request {
            schema_version: LOCAL_PROTOCOL_VERSION,
            operation,
            request_number: self.next_request,
            maximum_new_tokens: defaults.maximum_output_tokens,
            stochastic: defaults.sampling.stochastic,
            seed_present: defaults.sampling.seed_present,
            seed: defaults.sampling.seed,
            temperature: defaults.sampling.temperature,
            top_k: defaults.sampling.top_k,
            top_p: defaults.sampling.top_p,
            min_p: defaults.sampling.min_p,
            typical_p: defaults.sampling.typical_p,
            reasoning_policy: raw::yvex_reasoning_policy_YVEX_REASONING_DISABLED,
            ..Default::default()
        };
        self.next_request = self
            .next_request
            .checked_add(1)
            .expect("request ID exhausted");
        request
    }

    pub(crate) fn send(&mut self, request: &raw::yvex_client_request) -> Result<(), Error> {
        let mut report = raw::yvex_error::default();
        // All borrowed request pointers remain owned by the caller for this call.
        let status = unsafe { raw::yvex_client_send(self.native.as_ptr(), request, &mut report) };
        if status == 0 {
            Ok(())
        } else {
            Err(error(status, &report))
        }
    }

    pub(crate) fn send_text(
        &mut self,
        request: &raw::yvex_client_request,
        prompt: &str,
        attachments: &[raw::yvex_content_part],
    ) -> Result<(), Error> {
        let mut request = *request;
        request.prompt = prompt.as_ptr();
        request.prompt_bytes = prompt.len() as u64;
        let mut parts = attachments.to_vec();
        if !parts.is_empty() {
            let mut text = raw::yvex_content_part {
                schema_version: raw::YVEX_CONTENT_PART_SCHEMA_V1,
                kind: raw::yvex_content_kind_YVEX_CONTENT_TEXT,
                storage: raw::yvex_content_storage_YVEX_CONTENT_INLINE,
                bytes: prompt.as_ptr(),
                byte_count: prompt.len() as u64,
                ..Default::default()
            };
            put_text(&mut text.media_type, "text/plain;charset=utf-8")?;
            seal_content(&mut text)?;
            parts.push(text);
            // Private wire admits exactly one payload form. Text is already
            // the inline content part; a second prompt form is invalid.
            request.prompt = std::ptr::null();
            request.prompt_bytes = 0;
            request.content_parts = parts.as_ptr();
            request.content_part_count = parts.len() as u64;
        }
        // send serializes the borrowed text before returning; no native borrower survives.
        self.send(&request)
    }

    pub(crate) fn host_identity(&self) -> Result<String, Error> {
        let mut out = [0; raw::YVEX_SHA256_HEX_CAP as usize];
        let mut report = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_client_host_identity(self.native.as_ptr(), out.as_mut_ptr(), &mut report)
        };
        if status != 0 {
            return Err(error(status, &report));
        }
        Ok(text(&out))
    }

    pub(crate) fn timeout(&mut self, milliseconds: u64) -> Result<(), Error> {
        let mut report = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_client_timeout_set(self.native.as_ptr(), milliseconds, &mut report)
        };
        if status == 0 {
            Ok(())
        } else {
            Err(error(status, &report))
        }
    }

    pub(crate) fn receive(&mut self) -> Result<Box<raw::yvex_client_message>, Error> {
        let mut message = Box::<raw::yvex_client_message>::new_uninit();
        let mut report = raw::yvex_error::default();
        // The C decoder fills and validates the complete record on success only.
        let status = unsafe {
            raw::yvex_client_receive(self.native.as_ptr(), message.as_mut_ptr(), &mut report)
        };
        if status != 0 {
            return Err(error(status, &report));
        }
        Ok(unsafe { message.assume_init() })
    }
}

pub(crate) fn seal_content(part: &mut raw::yvex_content_part) -> Result<(), Error> {
    let mut report = raw::yvex_error::default();
    // The C content owner validates and seals identity; Rust never hashes native layouts.
    let status = unsafe { raw::yvex_content_part_seal(part, &mut report) };
    if status == 0 {
        Ok(())
    } else {
        Err(error(status, &report))
    }
}

impl Drop for Client {
    fn drop(&mut self) {
        let mut native = self.native.as_ptr();
        // close destroys only this connection; it never cancels or destroys a session.
        unsafe { raw::yvex_client_close(&mut native) };
    }
}

pub(crate) fn event_name(kind: raw::yvex_server_event_kind) -> String {
    // Canonical enum spellings are immutable C-owned data, not shell semantics.
    unsafe { CStr::from_ptr(raw::yvex_server_event_kind_name(kind)) }
        .to_string_lossy()
        .into_owned()
}

pub(crate) fn default_socket() -> Result<String, Error> {
    let mut path = [0; raw::YVEX_SERVER_SOCKET_PATH_CAP as usize];
    let mut report = raw::yvex_error::default();
    let status = unsafe { raw::yvex_server_socket_path(path.as_mut_ptr(), &mut report) };
    if status == 0 {
        Ok(text(&path))
    } else {
        Err(error(status, &report))
    }
}

pub(crate) struct Host {
    server: NonNull<raw::yvex_server>,
    loader: NonNull<raw::yvex_server_registry_loader>,
    _socket: CString,
}

impl Host {
    pub(crate) fn create(
        mut options: raw::yvex_server_options,
        socket: &str,
    ) -> Result<Self, Error> {
        let socket = argument(Some(socket))?.unwrap();
        let mut loader = std::ptr::null_mut();
        let mut report = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_server_registry_loader_create(&mut loader, options.trace_level, &mut report)
        };
        if status != 0 {
            return Err(error(status, &report));
        }
        let loader = NonNull::new(loader).expect("C loader success owns a handle");
        options.socket_path = socket.as_ptr();
        options.model_loader = Some(raw::yvex_server_registry_model_load);
        options.model_loader_context = loader.as_ptr().cast();
        let mut server = std::ptr::null_mut();
        let status = unsafe { raw::yvex_server_create(&mut server, &options, &mut report) };
        if status != 0 {
            // No server was published. The loader has no possible worker borrower.
            let mut pointer = loader.as_ptr();
            unsafe { raw::yvex_server_registry_loader_close(&mut pointer) };
            return Err(error(status, &report));
        }
        Ok(Self {
            server: NonNull::new(server).expect("C host success owns a handle"),
            loader,
            _socket: socket,
        })
    }

    pub(crate) fn start(&mut self) -> Result<(), Error> {
        let mut report = raw::yvex_error::default();
        let status = unsafe { raw::yvex_server_start(self.server.as_ptr(), &mut report) };
        if status == 0 {
            Ok(())
        } else {
            Err(error(status, &report))
        }
    }

    pub(crate) fn serve(&self) -> Result<(), Error> {
        let mut report = raw::yvex_error::default();
        let status = unsafe { raw::yvex_server_serve(self.server.as_ptr(), &mut report) };
        if status == 0 {
            Ok(())
        } else {
            Err(error(status, &report))
        }
    }

    pub(crate) fn finish(&self) -> Result<(), Error> {
        let mut report = raw::yvex_error::default();
        let stop = unsafe { raw::yvex_server_stop(self.server.as_ptr(), &mut report) };
        if stop != 0 {
            return Err(error(stop, &report));
        }
        let status = unsafe { raw::yvex_server_finish(self.server.as_ptr(), &mut report) };
        if status == 0 {
            Ok(())
        } else {
            Err(error(status, &report))
        }
    }

    pub(crate) fn events(&self) -> HostEvents<'_> {
        HostEvents {
            server: self.server,
            _host: std::marker::PhantomData,
        }
    }
}

impl Drop for Host {
    fn drop(&mut self) {
        let mut server = self.server.as_ptr();
        // The C close owner retires workers and transport before freeing resources.
        unsafe { raw::yvex_server_close(&mut server) };
        let mut loader = self.loader.as_ptr();
        // This callback context cannot be destroyed while the server owns workers.
        unsafe { raw::yvex_server_registry_loader_close(&mut loader) };
    }
}

pub(crate) struct HostEvents<'host> {
    server: NonNull<raw::yvex_server>,
    _host: std::marker::PhantomData<&'host ()>,
}

// SAFETY: C event_next serializes telemetry access. Only this read-only subscriber
// can cross a scoped thread; its borrow keeps Host alive until the reader joins.
// Host and its lifecycle methods are deliberately neither Send nor Sync.
unsafe impl Send for HostEvents<'_> {}

impl HostEvents<'_> {
    pub(crate) fn summary(&self) -> Result<Box<raw::yvex_server_summary>, Error> {
        let mut summary = Box::<raw::yvex_server_summary>::default();
        let mut report = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_server_get_summary(self.server.as_ptr(), summary.as_mut(), &mut report)
        };
        if status == 0 {
            Ok(summary)
        } else {
            Err(error(status, &report))
        }
    }

    pub(crate) fn next(&self, after: u64) -> Result<raw::yvex_server_event, Error> {
        let mut event = raw::yvex_server_event::default();
        let mut report = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_server_event_next(self.server.as_ptr(), after, 0, &mut event, &mut report)
        };
        if status == 0 {
            Ok(event)
        } else {
            Err(error(status, &report))
        }
    }
}

pub(crate) struct ModelSnapshot {
    pub entry: raw::yvex_model_library_entry,
    pub working_set: bool,
    pub sources: Vec<raw::yvex_local_source_record>,
    pub artifacts: Vec<(raw::yvex_model_artifact_fact, bool)>,
    pub profiles: Vec<raw::yvex_model_runtime_profile_fact>,
    pub publications: Vec<raw::yvex_model_publication>,
}

pub(crate) struct Library {
    native: NonNull<raw::yvex_model_library>,
}

fn argument(value: Option<&str>) -> Result<Option<CString>, Error> {
    value.map(CString::new).transpose().map_err(|_| Error {
        code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
        owner: "native.argument".into(),
        message: "native string contains NUL".into(),
    })
}

fn pointer(value: &Option<CString>) -> *const std::ffi::c_char {
    value
        .as_ref()
        .map_or(std::ptr::null(), |value| value.as_ptr())
}

pub(crate) struct Account {
    pub provider: raw::yvex_account_provider,
    cli: Option<CString>,
    token_env: Option<CString>,
}

impl Account {
    pub(crate) fn open(
        provider: &str,
        cli: Option<&str>,
        token_env: Option<&str>,
    ) -> Result<Self, Error> {
        let name = argument(Some(provider))?.unwrap();
        let mut provider = raw::yvex_account_provider_YVEX_ACCOUNT_PROVIDER_UNKNOWN;
        if unsafe { raw::yvex_account_provider_from_name(name.as_ptr(), &mut provider) } == 0 {
            return Err(Error {
                code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
                owner: "accounts.provider".into(),
                message: "unknown provider".into(),
            });
        }
        Ok(Self {
            provider,
            cli: argument(cli)?,
            token_env: argument(token_env)?,
        })
    }

    pub(crate) fn token_environment(&self) -> String {
        if let Some(name) = &self.token_env {
            return name.to_string_lossy().into_owned();
        }
        // A provider's default environment identity is static native metadata.
        unsafe { CStr::from_ptr(raw::yvex_account_default_token_env(self.provider)) }
            .to_string_lossy()
            .into_owned()
    }

    pub(crate) fn observe(&self) -> Result<raw::yvex_account_observation, Error> {
        let options = raw::yvex_account_observe_options {
            provider: self.provider,
            cli_override: pointer(&self.cli),
            token_env_name: pointer(&self.token_env),
        };
        let mut report = raw::yvex_account_observation::default();
        let mut failure = raw::yvex_error::default();
        let status = unsafe { raw::yvex_account_observe(&options, &mut report, &mut failure) };
        if status == 0 {
            Ok(report)
        } else {
            Err(error(status, &failure))
        }
    }

    pub(crate) fn ensure(
        &self,
        interactive: raw::yvex_account_interactive_mode,
        required: bool,
    ) -> Result<raw::yvex_account_observation, Error> {
        let options = raw::yvex_account_ensure_options {
            provider: self.provider,
            cli_override: pointer(&self.cli),
            token_env_name: pointer(&self.token_env),
            interactive,
            required: i32::from(required),
        };
        let mut report = raw::yvex_account_observation::default();
        let mut failure = raw::yvex_error::default();
        let status = unsafe { raw::yvex_account_ensure(&options, &mut report, &mut failure) };
        if status == 0 {
            Ok(report)
        } else {
            Err(error(status, &failure))
        }
    }
}

pub(crate) fn account_state(observations: &[raw::yvex_account_observation]) -> Result<(), Error> {
    let mut failure = raw::yvex_error::default();
    // The native persistence owner copies the synchronous borrowed observations.
    let status = unsafe {
        raw::yvex_account_write_state(observations.as_ptr(), observations.len() as _, &mut failure)
    };
    if status == 0 {
        Ok(())
    } else {
        Err(error(status, &failure))
    }
}

pub(crate) fn run_provider(args: &[std::ffi::OsString], machine: bool) -> Result<i32, Error> {
    use std::os::unix::ffi::OsStrExt;
    if args.is_empty() || args.len() >= raw::YVEX_ACCOUNT_ARG_CAP as usize {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_BOUNDS,
            owner: "accounts.command".into(),
            message: "provider argument extent is invalid".into(),
        });
    }
    let arguments = args
        .iter()
        .map(|arg| CString::new(arg.as_bytes()))
        .collect::<Result<Vec<_>, _>>()
        .map_err(|_| Error {
            code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
            owner: "accounts.command".into(),
            message: "provider argument contains NUL".into(),
        })?;
    if machine {
        // JSON owns stdout. Provider output is untrusted (and may contain secrets),
        // so drain both pipes into bounded caller-owned buffers without publishing it.
        // The native process owner preserves stdin and credentials as for foreground use.
        let mut stdout = [0 as std::ffi::c_char; 4096];
        let mut stderr = [0 as std::ffi::c_char; 4096];
        let mut options = raw::yvex_account_capture_options {
            stdout_bytes: stdout.as_mut_ptr(),
            stdout_capacity: stdout.len(),
            stderr_bytes: stderr.as_mut_ptr(),
            stderr_capacity: stderr.len(),
            ..Default::default()
        };
        for (slot, argument) in options.args.iter_mut().zip(&arguments) {
            *slot = argument.as_ptr();
        }
        let mut failure = raw::yvex_error::default();
        // All pointer extents are bounded and borrowed until the child is reaped.
        let status =
            unsafe { raw::yvex_accounts_capture_provider_command(&mut options, &mut failure) };
        return if status == 0 {
            Ok(options.exit_code)
        } else {
            Err(error(status, &failure))
        };
    }
    let mut options = raw::yvex_account_command_options::default();
    for (slot, argument) in options.args.iter_mut().zip(&arguments) {
        *slot = argument.as_ptr();
    }
    let mut failure = raw::yvex_error::default();
    // Borrowed argv stays alive until the C process owner finishes foreground execution.
    let status = unsafe { raw::yvex_accounts_run_provider_command(&options, &mut failure) };
    if status >= 0 {
        Ok(status)
    } else {
        Err(error(status, &failure))
    }
}

impl Library {
    pub(crate) fn open(models_root: Option<&str>, registry: Option<&str>) -> Result<Self, Error> {
        let models_root = argument(models_root)?;
        let registry = argument(registry)?;
        let options = raw::yvex_local_catalog_options {
            models_root: pointer(&models_root),
            registry_path: pointer(&registry),
        };
        let mut native = std::ptr::null_mut();
        let mut report = raw::yvex_error::default();
        // The catalog opens a copied metadata snapshot and retains no option pointers.
        let status = unsafe { raw::yvex_model_library_open(&mut native, &options, &mut report) };
        if status != 0 {
            return Err(error(status, &report));
        }
        NonNull::new(native)
            .map(|native| Self { native })
            .ok_or_else(|| Error {
                code: raw::yvex_status_YVEX_ERR_STATE,
                owner: "model.library".into(),
                message: "successful library open returned no handle".into(),
            })
    }

    pub(crate) fn count(&self) -> u64 {
        unsafe { raw::yvex_model_library_count(self.native.as_ptr()) }
    }

    pub(crate) fn matches(&self, index: u64, selector: &str) -> Result<bool, Error> {
        let selector = argument(Some(selector))?;
        Ok(unsafe {
            raw::yvex_model_library_matches(self.native.as_ptr(), index, pointer(&selector))
        } != 0)
    }

    pub(crate) fn snapshot(&self, index: u64) -> Result<ModelSnapshot, Error> {
        // Every borrowed record comes from this live immutable library. Copy it
        // before close; no native pointer or interior lifetime leaves this seam.
        let native = self.native.as_ptr();
        let entry = copied(unsafe { raw::yvex_model_library_at(native, index) })?;
        let working_set = unsafe { raw::yvex_model_library_is_working_set(native, index) } != 0;
        let sources = (0..unsafe { raw::yvex_model_library_source_count(native, index) })
            .map(|row| copied(unsafe { raw::yvex_model_library_source_at(native, index, row) }))
            .collect::<Result<Vec<_>, _>>()?;
        let artifacts = (0..unsafe { raw::yvex_model_library_artifact_count(native, index) })
            .map(|row| {
                Ok((
                    copied(unsafe { raw::yvex_model_library_artifact_at(native, index, row) })?,
                    unsafe { raw::yvex_model_library_artifact_is_local(native, index, row) } != 0,
                ))
            })
            .collect::<Result<Vec<_>, Error>>()?;
        let profiles = (0..unsafe { raw::yvex_model_library_profile_count(native, index) })
            .map(|row| copied(unsafe { raw::yvex_model_library_profile_at(native, index, row) }))
            .collect::<Result<Vec<_>, _>>()?;
        let publications = (0..unsafe { raw::yvex_model_library_publication_count(native, index) })
            .map(|row| {
                copied(unsafe { raw::yvex_model_library_publication_at(native, index, row) })
            })
            .collect::<Result<Vec<_>, _>>()?;
        Ok(ModelSnapshot {
            entry,
            working_set,
            sources,
            artifacts,
            profiles,
            publications,
        })
    }
}

fn copied<T: Copy>(value: *const T) -> Result<T, Error> {
    NonNull::new(value.cast_mut())
        .map(|value| {
            // Callers provide pointers from the current live C owner, not user addresses.
            unsafe { *value.as_ptr() }
        })
        .ok_or_else(|| Error {
            code: raw::yvex_status_YVEX_ERR_BOUNDS,
            owner: "native.snapshot".into(),
            message: "native snapshot index is unavailable".into(),
        })
}

impl Drop for Library {
    fn drop(&mut self) {
        unsafe { raw::yvex_model_library_close(self.native.as_ptr()) };
    }
}

pub(crate) fn media_component_paths(target: &str) -> Result<[String; 4], Error> {
    let target = argument(Some(target))?;
    let mut profile = raw::yvex_media_target_profile::default();
    let mut report = raw::yvex_error::default();
    let status =
        unsafe { raw::yvex_component_target_profile(pointer(&target), &mut profile, &mut report) };
    if status != 0 {
        return Err(error(status, &report));
    }
    Ok([
        profile.text_artifact,
        profile.transformer_artifact,
        profile.video_artifact,
        profile.audio_artifact,
    ]
    .map(|value| {
        if value.is_null() {
            String::new()
        } else {
            // Profile strings belong to the immutable registered family descriptor.
            unsafe { CStr::from_ptr(value) }
                .to_string_lossy()
                .into_owned()
        }
    }))
}

pub(crate) struct Gguf {
    native: NonNull<raw::yvex_gguf>,
}

impl Gguf {
    pub(crate) fn open(path: &str) -> Result<Self, Error> {
        let path = argument(Some(path))?;
        let options = raw::yvex_artifact_options {
            path: pointer(&path),
            readonly: 1,
            map: 0,
        };
        let mut artifact = std::ptr::null_mut();
        let mut native = std::ptr::null_mut();
        let mut report = raw::yvex_error::default();
        let status = unsafe { raw::yvex_artifact_open(&mut artifact, &options, &mut report) };
        if status != 0 {
            return Err(error(status, &report));
        }
        // The parsed view owns copied metadata; close the unmapped artifact even
        // when parsing refuses, before returning a native owner or error.
        let status = unsafe { raw::yvex_gguf_open(&mut native, artifact, &mut report) };
        unsafe { raw::yvex_artifact_close(artifact) };
        if status != 0 {
            return Err(error(status, &report));
        }
        NonNull::new(native)
            .map(|native| Self { native })
            .ok_or_else(|| Error {
                code: raw::yvex_status_YVEX_ERR_STATE,
                owner: "gguf.open".into(),
                message: "successful metadata open returned no handle".into(),
            })
    }

    pub(crate) fn precision(&self) -> Result<String, Error> {
        let mut types = Vec::new();
        for index in 0..unsafe { raw::yvex_gguf_tensor_count(self.native.as_ptr()) } {
            let tensor = copied(unsafe { raw::yvex_gguf_tensor_at(self.native.as_ptr(), index) })?;
            if tensor.ggml_type_name.is_null() {
                continue;
            }
            let name = unsafe { CStr::from_ptr(tensor.ggml_type_name) }
                .to_string_lossy()
                .into_owned();
            if types.len() < 8 && !types.contains(&name) {
                types.push(name);
            }
        }
        Ok(if types.is_empty() {
            "unknown".into()
        } else {
            types.join("/")
        })
    }
}

impl Drop for Gguf {
    fn drop(&mut self) {
        unsafe { raw::yvex_gguf_close(self.native.as_ptr()) };
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn exact_leaf_contract() {
        assert_eq!(version(), "0.1.0");
        assert_eq!(LOCAL_PROTOCOL_VERSION, 25);
        assert_eq!(text(&[65, 0, 66]), "A");
    }
    #[test]
    fn missing_socket_is_typed_and_owned() {
        let result = Client::connect(Some("/nonexistent-yvex-shell-test/socket"));
        assert!(matches!(result, Err(Error { code: -3, .. })));
        assert!(Client::connect(Some("bad\0path")).is_err());
    }
}
