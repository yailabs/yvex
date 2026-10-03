// Registry lifetimes, filename interpretation, file identity and admission stay C-owned.
use super::{Error, argument, borrowed_text, error, extent_error, pointer, raw, text};
use std::ffi::CString;

pub(crate) fn default_path() -> Result<String, Error> {
    let mut path = [0; raw::YVEX_PATH_CAP as usize];
    let mut failure = raw::yvex_error::default();
    let status = unsafe {
        raw::yvex_model_registry_default_path(path.as_mut_ptr(), path.len() as u64, &mut failure)
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(text(&path))
}

pub(crate) struct Reference {
    native: raw::yvex_model_ref,
}
pub(crate) struct ReferenceMetadata {
    pub pairs: Vec<(String, String)>,
    pub support: String,
    pub ready: bool,
    pub hidden: u64,
    pub vocab: u64,
    pub output: u64,
    pub slice: u64,
}
pub(crate) struct ReferenceIntegrity {
    pub passed: bool,
    pub identity: String,
    pub metadata: String,
    pub readiness: String,
    pub current: Option<ReferenceMetadata>,
    pub issues: Vec<raw::yvex_model_metadata_issue>,
}
impl Drop for Reference {
    fn drop(&mut self) {
        unsafe {
            raw::yvex_model_ref_clear(&mut self.native);
        }
    }
}
impl Reference {
    pub(crate) fn resolve(input: &str) -> Result<Self, Error> {
        Self::resolve_in(input, None)
    }
    pub(crate) fn resolve_in(input: &str, registry: Option<&str>) -> Result<Self, Error> {
        let input = argument(Some(input))?.expect("required reference");
        let registry = argument(registry)?;
        let options = raw::yvex_model_ref_options {
            allow_registry: 1,
            registry_path: pointer(&registry),
        };
        let mut reference = Self {
            native: raw::yvex_model_ref::default(),
        };
        let mut failure = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_model_ref_resolve(
                &mut reference.native,
                input.as_ptr(),
                &options,
                &mut failure,
            )
        };
        if status != 0 {
            return Err(error(status, &failure));
        }
        Ok(reference)
    }
    pub(crate) fn path(&self) -> Result<String, Error> {
        unsafe { borrowed_text(self.native.path) }
    }
    pub(crate) fn is_alias(&self) -> bool {
        self.native.kind == raw::yvex_model_ref_kind_YVEX_MODEL_REF_ALIAS
    }
    pub(crate) fn registered_digest(&self) -> Result<Option<String>, Error> {
        if !self.is_alias() || self.native.sha256.is_null() {
            return Ok(None);
        }
        let digest = unsafe { borrowed_text(self.native.sha256) }?;
        Ok((!digest.is_empty()).then_some(digest))
    }
    pub(crate) fn integrity(
        &self,
        expected: Option<&str>,
        required: bool,
        token: u32,
    ) -> Result<(raw::yvex_artifact_integrity_report, i32), Error> {
        let expected = argument(expected)?;
        let options = raw::yvex_artifact_integrity_options {
            expect_sha256: pointer(&expected),
            require_token_embedding: required.into(),
            token_id: token,
            registered_sha256: if self.is_alias() {
                self.native.sha256
            } else {
                std::ptr::null()
            },
        };
        let mut report = raw::yvex_artifact_integrity_report::default();
        let mut failure = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_artifact_integrity_check_path(
                self.native.path,
                &options,
                &mut report,
                &mut failure,
            )
        };
        if report.issue_count as usize > report.issues.len() {
            return Err(extent_error());
        }
        Ok((report, status))
    }
    pub(crate) fn integrity_verification(
        &self,
        integrity: &raw::yvex_artifact_integrity_report,
    ) -> Result<ReferenceIntegrity, Error> {
        let mut report = Box::<raw::yvex_model_registry_verification>::default();
        let mut failure = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_model_ref_verify_integrity(
                &self.native,
                integrity,
                report.as_mut(),
                &mut failure,
            )
        };
        if status != 0 && status != raw::yvex_status_YVEX_ERR_STATE {
            return Err(error(status, &failure));
        }
        if report.drift.issue_count as usize > report.drift.issues.len() {
            return Err(extent_error());
        }
        Ok(ReferenceIntegrity {
            passed: report.passed != 0,
            identity: text(&report.identity_status),
            metadata: text(&report.metadata_status),
            readiness: text(&report.readiness_status),
            current: if report.metadata_checked != 0 {
                Some(reference_metadata(&report.current.entry)?)
            } else {
                None
            },
            issues: report.drift.issues[..report.drift.issue_count as usize].to_vec(),
        })
    }
}

fn reference_metadata(entry: &raw::yvex_model_registry_entry) -> Result<ReferenceMetadata, Error> {
    let mut pairs = Vec::new();
    for (name, value) in [
        ("primary_tensor", entry.primary_tensor_name),
        ("primary_tensor_role", entry.primary_tensor_role),
        ("primary_tensor_dtype", entry.primary_tensor_dtype),
        ("primary_tensor_dims", entry.primary_tensor_dims),
    ] {
        pairs.push((
            name.into(),
            if value.is_null() {
                String::new()
            } else {
                unsafe { borrowed_text(value)? }
            },
        ));
    }
    pairs.push((
        "primary_tensor_rank".into(),
        entry.primary_tensor_rank.to_string(),
    ));
    pairs.push((
        "primary_tensor_bytes".into(),
        entry.primary_tensor_bytes.to_string(),
    ));
    Ok(ReferenceMetadata {
        pairs,
        support: if entry.support_level.is_null() {
            "not-checked".into()
        } else {
            unsafe { borrowed_text(entry.support_level)? }
        },
        ready: entry.selected_embedding_ready != 0,
        hidden: entry.selected_embedding_hidden_size,
        vocab: entry.selected_embedding_vocab_size,
        output: entry.selected_embedding_output_count,
        slice: entry.selected_embedding_slice_bytes,
    })
}

pub(crate) enum Deployment<'a> {
    Inspection,
    Single {
        binding: &'a str,
        target: &'a str,
        backend: &'a str,
        strategy: &'a str,
        context: u64,
    },
    Composite {
        root: &'a str,
        target: &'a str,
        backend: &'a str,
    },
}

pub(crate) struct ProfileRequest<'a> {
    pub path: &'a str,
    pub alias: Option<&'a str>,
    pub family: Option<&'a str>,
    pub model: Option<&'a str>,
    pub scope: Option<&'a str>,
    pub class: Option<&'a str>,
    pub qprofile: Option<&'a str>,
    pub calibration: Option<&'a str>,
    pub support: Option<&'a str>,
    pub expected_sha256: Option<&'a str>,
    pub deployment: Deployment<'a>,
}

pub(crate) fn profile_create(
    request: &ProfileRequest<'_>,
    registry: Option<&str>,
) -> Result<raw::yvex_model_registry_creation, Error> {
    profile_publish(request, registry, false)
}

pub(crate) fn profile_replace(
    request: &ProfileRequest<'_>,
    registry: Option<&str>,
) -> Result<raw::yvex_model_registry_creation, Error> {
    profile_publish(request, registry, true)
}

fn profile_publish(
    request: &ProfileRequest<'_>,
    registry: Option<&str>,
    replace: bool,
) -> Result<raw::yvex_model_registry_creation, Error> {
    // Keep every CString alive until native code has copied the admitted entry.
    let mut storage = Vec::<CString>::new();
    let mut string = |value: Option<&str>| -> Result<*const std::ffi::c_char, Error> {
        if let Some(value) = argument(value)? {
            storage.push(value);
            Ok(storage.last().expect("retained string").as_ptr())
        } else {
            Ok(std::ptr::null())
        }
    };
    let mut requested = raw::yvex_model_registry_entry {
        schema_version: raw::YVEX_MODEL_REGISTRY_ENTRY_SCHEMA_CURRENT,
        path: string(Some(request.path))?,
        alias: string(request.alias)?,
        family: string(request.family)?,
        model: string(request.model)?,
        scope: string(request.scope)?,
        artifact_class: string(request.class)?,
        qprofile: string(request.qprofile)?,
        calibration: string(request.calibration)?,
        support_level: string(request.support)?,
        ..Default::default()
    };
    match request.deployment {
        Deployment::Inspection => {}
        Deployment::Single {
            binding,
            target,
            backend,
            strategy,
            context,
        } => {
            requested.runtime_profile = string(Some("single-artifact"))?;
            requested.runtime_binding = string(Some(binding))?;
            requested.runtime_target = string(Some(target))?;
            requested.runtime_backend = string(Some(backend))?;
            requested.runtime_execution_strategy = string(Some(strategy))?;
            requested.runtime_context = context;
            requested.runtime_engine_kind = string(Some("text"))?;
        }
        Deployment::Composite {
            root,
            target,
            backend,
        } => {
            requested.runtime_profile = string(Some("composite"))?;
            requested.runtime_installation = string(Some(root))?;
            requested.runtime_target = string(Some(target))?;
            requested.runtime_backend = string(Some(backend))?;
            requested.runtime_engine_kind = string(Some("media"))?;
            requested.runtime_execution_strategy = string(Some("not-applicable"))?;
        }
    }
    let registry = argument(registry)?;
    let expected = argument(request.expected_sha256)?;
    let mut receipt = raw::yvex_model_registry_creation::default();
    let mut failure = raw::yvex_error::default();
    // The facade derives missing names, verifies file facts and saves atomically
    // only after the same native startup-profile admission used by C consumers.
    let status = unsafe {
        raw::yvex_model_registry_create(
            &requested,
            pointer(&expected),
            pointer(&registry),
            replace.into(),
            &mut receipt,
            &mut failure,
        )
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(receipt)
}

pub(crate) struct ProfileVerification {
    pub passed: bool,
    pub identity_status: String,
    pub metadata_status: String,
    pub readiness_status: String,
    pub status: String,
    pub reason: String,
    pub pairs: Vec<(String, String, String)>,
    pub issues: Vec<raw::yvex_model_metadata_issue>,
    pub alias: String,
    pub path: String,
    pub registered_sha256: String,
    pub registered_size: u64,
    pub current: raw::yvex_artifact_file_identity,
}

struct Registry(*mut raw::yvex_model_registry);
impl Drop for Registry {
    fn drop(&mut self) {
        unsafe { raw::yvex_model_registry_close(self.0) };
    }
}

pub(crate) fn profile_verify(
    alias: &str,
    registry: Option<&str>,
) -> Result<ProfileVerification, Error> {
    let alias = argument(Some(alias))?.expect("required alias");
    let registry = argument(registry)?;
    let options = raw::yvex_model_registry_options {
        registry_path: pointer(&registry),
        create_if_missing: 1,
    };
    let mut native = Registry(std::ptr::null_mut());
    let mut failure = raw::yvex_error::default();
    let status = unsafe { raw::yvex_model_registry_open(&mut native.0, &options, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    let entry = unsafe { raw::yvex_model_registry_find(native.0, alias.as_ptr()).as_ref() }
        .ok_or_else(|| Error {
            code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
            owner: "model_registry_verify".into(),
            message: "model alias not found".into(),
        })?;
    verification(entry)
}

fn verification(entry: &raw::yvex_model_registry_entry) -> Result<ProfileVerification, Error> {
    let mut failure = raw::yvex_error::default();
    // The result's metadata pointers borrow its own buffers. Allocate before
    // calling and copy all selected facts before that fixed-address box retires.
    let mut result = Box::<raw::yvex_model_registry_verification>::default();
    let status = unsafe { raw::yvex_model_registry_verify(entry, result.as_mut(), &mut failure) };
    if status != 0 && status != raw::yvex_status_YVEX_ERR_STATE {
        return Err(error(status, &failure));
    }
    let issue_count = result.drift.issue_count as usize;
    if issue_count > result.drift.issues.len() {
        return Err(extent_error());
    }
    Ok(ProfileVerification {
        passed: result.passed != 0,
        identity_status: text(&result.identity_status),
        metadata_status: text(&result.metadata_status),
        readiness_status: text(&result.readiness_status),
        status: text(&result.status),
        reason: text(&result.reason),
        pairs: verification_pairs(entry, &result.current.entry, result.metadata_checked != 0)?,
        issues: result.drift.issues[..issue_count].to_vec(),
        alias: unsafe { borrowed_text(entry.alias)? },
        path: unsafe { borrowed_text(entry.path)? },
        registered_sha256: if entry.sha256.is_null() {
            String::new()
        } else {
            unsafe { borrowed_text(entry.sha256)? }
        },
        registered_size: entry.file_size,
        current: result.identity,
    })
}

fn verification_pairs(
    registered: &raw::yvex_model_registry_entry,
    current: &raw::yvex_model_registry_entry,
    checked: bool,
) -> Result<Vec<(String, String, String)>, Error> {
    let mut pairs = Vec::new();
    for (key, before, after) in [
        (
            "artifact_support_level",
            registered.support_level,
            current.support_level,
        ),
        (
            "architecture",
            registered.architecture,
            current.architecture,
        ),
        (
            "primary_tensor",
            registered.primary_tensor_name,
            current.primary_tensor_name,
        ),
        (
            "primary_role",
            registered.primary_tensor_role,
            current.primary_tensor_role,
        ),
        (
            "primary_dtype",
            registered.primary_tensor_dtype,
            current.primary_tensor_dtype,
        ),
        (
            "primary_dims",
            registered.primary_tensor_dims,
            current.primary_tensor_dims,
        ),
    ] {
        let before = if before.is_null() {
            String::new()
        } else {
            unsafe { borrowed_text(before)? }
        };
        let after = if !checked {
            "not-checked".into()
        } else if after.is_null() {
            String::new()
        } else {
            unsafe { borrowed_text(after)? }
        };
        pairs.push((key.into(), before, after));
    }
    for (key, before, after) in [
        (
            "tensor_count",
            registered.tensor_count,
            current.tensor_count,
        ),
        (
            "known_tensor_bytes",
            registered.known_tensor_bytes,
            current.known_tensor_bytes,
        ),
        (
            "primary_rank",
            registered.primary_tensor_rank.into(),
            current.primary_tensor_rank.into(),
        ),
        (
            "primary_bytes",
            registered.primary_tensor_bytes,
            current.primary_tensor_bytes,
        ),
        (
            "selected_embedding_hidden_size",
            registered.selected_embedding_hidden_size,
            current.selected_embedding_hidden_size,
        ),
        (
            "selected_embedding_vocab_size",
            registered.selected_embedding_vocab_size,
            current.selected_embedding_vocab_size,
        ),
        (
            "selected_embedding_output_count",
            registered.selected_embedding_output_count,
            current.selected_embedding_output_count,
        ),
        (
            "selected_embedding_slice_bytes",
            registered.selected_embedding_slice_bytes,
            current.selected_embedding_slice_bytes,
        ),
    ] {
        pairs.push((
            key.into(),
            before.to_string(),
            if checked {
                after.to_string()
            } else {
                "0".into()
            },
        ));
    }
    pairs.push((
        "selected_embedding_ready".into(),
        (registered.selected_embedding_ready != 0).to_string(),
        (checked && current.selected_embedding_ready != 0).to_string(),
    ));
    Ok(pairs)
}
