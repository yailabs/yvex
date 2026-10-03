// Native quantization documents retain semantic/validation ownership in libyvex.
use super::{Error, argument, borrowed_text, error, extent_error, pointer, raw, text};
use std::sync::Mutex;

fn checked(status: i32, failure: &raw::yvex_error) -> Result<(), Error> {
    if status == 0 {
        Ok(())
    } else {
        Err(error(status, failure))
    }
}

unsafe fn optional(value: *const std::ffi::c_char) -> Result<String, Error> {
    if value.is_null() {
        Ok(String::new())
    } else {
        unsafe { borrowed_text(value) }
    }
}

pub(crate) enum PolicyInput<'a> {
    File {
        path: &'a str,
        validate: bool,
        template: Option<&'a str>,
    },
    Template {
        path: &'a str,
        architecture: &'a str,
        out: &'a str,
    },
    Preset {
        name: &'a str,
        out: Option<&'a str>,
    },
}

#[derive(serde::Serialize)]
pub(crate) struct PolicyRule {
    pub selector_kind: String,
    pub selector: String,
    pub qtype: String,
    pub storage_supported: bool,
    pub compute_supported: bool,
    pub requires_imatrix: bool,
    pub requires_cpu: bool,
    pub requires_cuda: bool,
    pub priority: u32,
    pub label: String,
    pub match_mask: u64,
}

#[derive(serde::Serialize)]
pub(crate) struct PolicyFacts {
    pub name: String,
    pub architecture: String,
    pub preset: String,
    pub schema_version: u32,
    pub identity: String,
    pub status: String,
    pub rules: u64,
    pub issues: u64,
    pub requires_imatrix: u64,
    pub storage_supported: u64,
    pub compute_supported: u64,
    #[serde(skip)]
    pub rows: Vec<PolicyRule>,
}

struct Policy(*mut raw::yvex_quant_policy);
impl Drop for Policy {
    fn drop(&mut self) {
        unsafe { raw::yvex_quant_policy_close(self.0) };
    }
}

pub(crate) fn policy(input: PolicyInput<'_>) -> Result<PolicyFacts, Error> {
    let mut policy = Policy(std::ptr::null_mut());
    let mut failure = raw::yvex_error::default();
    match input {
        PolicyInput::File {
            path,
            validate,
            template,
        } => {
            let path = argument(Some(path))?.expect("required policy");
            let template = argument(template)?;
            checked(
                unsafe { raw::yvex_quant_policy_open(&mut policy.0, path.as_ptr(), &mut failure) },
                &failure,
            )?;
            if validate {
                checked(
                    unsafe {
                        raw::yvex_quant_policy_validate(policy.0, pointer(&template), &mut failure)
                    },
                    &failure,
                )?;
            }
        }
        PolicyInput::Template {
            path,
            architecture,
            out,
        } => {
            let path = argument(Some(path))?.expect("required template");
            let architecture = argument(Some(architecture))?.expect("required architecture");
            let out = argument(Some(out))?.expect("required policy destination");
            checked(
                unsafe {
                    raw::yvex_quant_policy_create_from_template(
                        &mut policy.0,
                        path.as_ptr(),
                        architecture.as_ptr(),
                        &mut failure,
                    )
                },
                &failure,
            )?;
            checked(
                unsafe { raw::yvex_quant_policy_write_json(out.as_ptr(), policy.0, &mut failure) },
                &failure,
            )?;
        }
        PolicyInput::Preset { name, out } => {
            let name = argument(Some(name))?.expect("required preset");
            let out = argument(out)?;
            checked(
                unsafe {
                    raw::yvex_quant_policy_preset_open(&mut policy.0, name.as_ptr(), &mut failure)
                },
                &failure,
            )?;
            if let Some(out) = out {
                checked(
                    unsafe {
                        raw::yvex_quant_policy_write_json(out.as_ptr(), policy.0, &mut failure)
                    },
                    &failure,
                )?;
            }
        }
    }
    let mut summary = raw::yvex_quant_policy_summary::default();
    checked(
        unsafe { raw::yvex_quant_policy_get_summary(policy.0, &mut summary, &mut failure) },
        &failure,
    )?;
    let count = unsafe { raw::yvex_quant_policy_rule_count(policy.0) };
    if count != summary.rule_count {
        return Err(extent_error());
    }
    let mut rows = Vec::new();
    for index in 0..count {
        let row = unsafe { raw::yvex_quant_policy_rule_at(policy.0, index).as_ref() }
            .ok_or_else(extent_error)?;
        unsafe {
            rows.push(PolicyRule {
                selector_kind: borrowed_text(raw::yvex_quant_selector_kind_name(
                    row.selector_kind,
                ))?,
                selector: optional(row.selector)?,
                qtype: borrowed_text(raw::yvex_quant_qtype_name(row.qtype))?,
                storage_supported: row.storage_supported != 0,
                compute_supported: row.compute_supported != 0,
                requires_imatrix: row.requires_imatrix != 0,
                requires_cpu: row.requires_cpu_compute != 0,
                requires_cuda: row.requires_cuda_compute != 0,
                priority: row.priority,
                label: optional(row.label)?,
                match_mask: row.match_mask,
            });
        }
    }
    unsafe {
        Ok(PolicyFacts {
            name: optional(summary.name)?,
            architecture: optional(summary.architecture)?,
            preset: optional(summary.preset_name)?,
            schema_version: summary.schema_version,
            identity: text(&summary.policy_identity),
            status: borrowed_text(raw::yvex_quant_policy_status_name(summary.status))?,
            rules: summary.rule_count,
            issues: summary.issue_count,
            requires_imatrix: summary.requires_imatrix_count,
            storage_supported: summary.storage_supported_count,
            compute_supported: summary.compute_supported_count,
            rows,
        })
    }
}

pub(crate) fn policy_presets() -> Result<Vec<String>, Error> {
    let count = unsafe { raw::yvex_quant_policy_preset_count() };
    (0..count)
        .map(|index| unsafe { borrowed_text(raw::yvex_quant_policy_preset_name(index)) })
        .collect()
}

pub(crate) struct ImatrixOptions<'a> {
    pub name: &'a str,
    pub architecture: &'a str,
    pub source_manifest: Option<&'a str>,
    pub policy: Option<&'a str>,
    pub path: &'a str,
    pub dataset: Option<&'a str>,
    pub command: Option<&'a str>,
    pub producer: Option<&'a str>,
    pub format: &'a str,
    pub status: &'a str,
    pub out: &'a str,
}

pub(crate) enum ImatrixInput<'a> {
    Create(ImatrixOptions<'a>),
    File { path: &'a str, validate: bool },
}

#[derive(serde::Serialize)]
pub(crate) struct ImatrixFacts {
    pub name: String,
    pub architecture: String,
    pub imatrix: String,
    pub source_manifest: String,
    pub quant_policy: String,
    pub format: String,
    pub status: String,
    pub issues: u64,
    pub covered_rules: u64,
    pub uncovered_rules: u64,
    pub requires_imatrix_rules: u64,
    pub file_exists: bool,
}

struct Imatrix(*mut raw::yvex_imatrix_manifest);
impl Drop for Imatrix {
    fn drop(&mut self) {
        unsafe { raw::yvex_imatrix_manifest_close(self.0) };
    }
}

fn imatrix_create(options: ImatrixOptions<'_>) -> Result<Imatrix, Error> {
    let name = argument(Some(options.name))?.expect("required imatrix name");
    let architecture = argument(Some(options.architecture))?.expect("required architecture");
    let source = argument(options.source_manifest)?;
    let policy = argument(options.policy)?;
    let path = argument(Some(options.path))?.expect("required calibration path");
    let dataset = argument(options.dataset)?;
    let command = argument(options.command)?;
    let producer = argument(options.producer)?;
    let format = argument(Some(options.format))?.expect("required format");
    let status = argument(Some(options.status))?.expect("required status");
    let out = argument(Some(options.out))?.expect("required manifest destination");
    let native = raw::yvex_imatrix_manifest_options {
        name: name.as_ptr(),
        architecture: architecture.as_ptr(),
        source_manifest_path: pointer(&source),
        quant_policy_path: pointer(&policy),
        imatrix_path: path.as_ptr(),
        calibration_dataset: pointer(&dataset),
        calibration_command: pointer(&command),
        producer: pointer(&producer),
        format: unsafe { raw::yvex_imatrix_format_from_name(format.as_ptr()) },
        status: unsafe { raw::yvex_imatrix_status_from_name(status.as_ptr()) },
    };
    if native.format == raw::yvex_imatrix_format_YVEX_IMATRIX_FORMAT_UNKNOWN
        || native.status == raw::yvex_imatrix_status_YVEX_IMATRIX_STATUS_UNKNOWN
    {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
            owner: "imatrix.manifest".into(),
            message: "unknown calibration format or status".into(),
        });
    }
    let mut manifest = Imatrix(std::ptr::null_mut());
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe { raw::yvex_imatrix_manifest_create(&mut manifest.0, &native, &mut failure) },
        &failure,
    )?;
    checked(
        unsafe { raw::yvex_imatrix_manifest_validate(manifest.0, &mut failure) },
        &failure,
    )?;
    checked(
        unsafe { raw::yvex_imatrix_manifest_write_json(out.as_ptr(), manifest.0, &mut failure) },
        &failure,
    )?;
    Ok(manifest)
}

pub(crate) fn imatrix(input: ImatrixInput<'_>) -> Result<ImatrixFacts, Error> {
    let mut failure = raw::yvex_error::default();
    let manifest = match input {
        ImatrixInput::Create(options) => imatrix_create(options)?,
        ImatrixInput::File { path, validate } => {
            let path = argument(Some(path))?.expect("required imatrix manifest");
            let mut manifest = Imatrix(std::ptr::null_mut());
            checked(
                unsafe {
                    raw::yvex_imatrix_manifest_open(&mut manifest.0, path.as_ptr(), &mut failure)
                },
                &failure,
            )?;
            if validate {
                checked(
                    unsafe { raw::yvex_imatrix_manifest_validate(manifest.0, &mut failure) },
                    &failure,
                )?;
            }
            manifest
        }
    };
    let mut summary = raw::yvex_imatrix_summary::default();
    checked(
        unsafe { raw::yvex_imatrix_manifest_get_summary(manifest.0, &mut summary, &mut failure) },
        &failure,
    )?;
    unsafe {
        Ok(ImatrixFacts {
            name: optional(summary.name)?,
            architecture: optional(summary.architecture)?,
            imatrix: optional(summary.imatrix_path)?,
            source_manifest: optional(summary.source_manifest_path)?,
            quant_policy: optional(summary.quant_policy_path)?,
            format: borrowed_text(raw::yvex_imatrix_format_name(summary.format))?,
            status: borrowed_text(raw::yvex_imatrix_status_name(summary.status))?,
            issues: summary.issue_count,
            covered_rules: summary.covered_rule_count,
            uncovered_rules: summary.uncovered_rule_count,
            requires_imatrix_rules: summary.requires_imatrix_rule_count,
            file_exists: summary.file_exists != 0,
        })
    }
}

pub(crate) struct JobOptions<'a> {
    pub name: &'a str,
    pub architecture: &'a str,
    pub tool: &'a str,
    pub tool_path: &'a str,
    pub source_manifest: Option<&'a str>,
    pub source: &'a str,
    pub template: &'a str,
    pub policy: Option<&'a str>,
    pub imatrix_manifest: Option<&'a str>,
    pub imatrix: Option<&'a str>,
    pub output: &'a str,
    pub log: &'a str,
    pub command: &'a str,
    pub status: &'a str,
    pub out: &'a str,
}

pub(crate) enum JobInput<'a> {
    Create(Box<JobOptions<'a>>),
    File(&'a str),
}

#[derive(serde::Serialize)]
pub(crate) struct JobFacts {
    pub name: String,
    pub architecture: String,
    pub tool: String,
    pub tool_path: String,
    pub native_source: String,
    pub template: String,
    pub out_gguf: String,
    pub log: String,
    pub status: String,
    pub tool_exists: bool,
    pub source_exists: bool,
    pub template_exists: bool,
    pub imatrix_exists: bool,
    pub output_exists: bool,
}

// The native job API retains one process-lifetime summary. Copy it before the
// next job call, and serialize this consumer's access rather than escaping borrows.
static JOB_SUMMARY: Mutex<()> = Mutex::new(());

fn job_create(
    options: JobOptions<'_>,
    summary: &mut raw::yvex_quant_job_summary,
) -> Result<(), Error> {
    let name = argument(Some(options.name))?.expect("required job name");
    let architecture = argument(Some(options.architecture))?.expect("required architecture");
    let tool = argument(Some(options.tool))?.expect("required tool");
    let tool_path = argument(Some(options.tool_path))?.expect("required tool path");
    let source_manifest = argument(options.source_manifest)?;
    let source = argument(Some(options.source))?.expect("required native source");
    let template = argument(Some(options.template))?.expect("required template");
    let policy = argument(options.policy)?;
    let imatrix_manifest = argument(options.imatrix_manifest)?;
    let imatrix = argument(options.imatrix)?;
    let output = argument(Some(options.output))?.expect("required output GGUF path");
    let log = argument(Some(options.log))?.expect("required log path");
    let command = argument(Some(options.command))?.expect("required command");
    let status = argument(Some(options.status))?.expect("required status");
    let out = argument(Some(options.out))?.expect("required manifest destination");
    let native = raw::yvex_quant_job_options {
        name: name.as_ptr(),
        architecture: architecture.as_ptr(),
        tool_path: tool_path.as_ptr(),
        source_manifest_path: pointer(&source_manifest),
        native_source_dir: source.as_ptr(),
        template_path: template.as_ptr(),
        quant_policy_path: pointer(&policy),
        imatrix_manifest_path: pointer(&imatrix_manifest),
        imatrix_path: pointer(&imatrix),
        out_gguf_path: output.as_ptr(),
        log_path: log.as_ptr(),
        command: command.as_ptr(),
        tool: unsafe { raw::yvex_quant_job_tool_from_name(tool.as_ptr()) },
        status: unsafe { raw::yvex_quant_job_status_from_name(status.as_ptr()) },
    };
    if native.status == raw::yvex_quant_job_status_YVEX_QUANT_JOB_STATUS_UNKNOWN {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
            owner: "quant.job".into(),
            message: "unknown job status".into(),
        });
    }
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe { raw::yvex_quant_job_write_json(out.as_ptr(), &native, summary, &mut failure) },
        &failure,
    )
}

pub(crate) fn quant_job(input: JobInput<'_>) -> Result<JobFacts, Error> {
    let _guard = JOB_SUMMARY.lock().map_err(|_| Error {
        code: raw::yvex_status_YVEX_ERR_STATE,
        owner: "quant.job".into(),
        message: "job summary owner unavailable".into(),
    })?;
    let mut summary = raw::yvex_quant_job_summary::default();
    match input {
        JobInput::Create(options) => job_create(*options, &mut summary)?,
        JobInput::File(path) => {
            let path = argument(Some(path))?.expect("required job manifest");
            let mut failure = raw::yvex_error::default();
            checked(
                unsafe { raw::yvex_quant_job_validate(path.as_ptr(), &mut summary, &mut failure) },
                &failure,
            )?;
        }
    }
    unsafe {
        Ok(JobFacts {
            name: optional(summary.name)?,
            architecture: optional(summary.architecture)?,
            tool: borrowed_text(raw::yvex_quant_job_tool_name(summary.tool))?,
            tool_path: optional(summary.tool_path)?,
            native_source: optional(summary.native_source_dir)?,
            template: optional(summary.template_path)?,
            out_gguf: optional(summary.out_gguf_path)?,
            log: optional(summary.log_path)?,
            status: borrowed_text(raw::yvex_quant_job_status_name(summary.status))?,
            tool_exists: summary.tool_exists != 0,
            source_exists: summary.source_exists != 0,
            template_exists: summary.template_exists != 0,
            imatrix_exists: summary.imatrix_exists != 0,
            output_exists: summary.output_exists != 0,
        })
    }
}
