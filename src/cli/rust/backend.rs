// Diagnostic presentation consumes native capability/evidence reports, never infers model support.
use crate::{
    Output,
    ffi::{self, raw},
    presentation,
    registry::Invocation,
};
use replai::Alignment;

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let operation = invocation.operation.operation_id.as_str();
    let backend = match invocation.positionals.first().map(String::as_str) {
        Some(name) if operation == "system.backend" => ffi::backend_kind(name)?,
        Some("cuda") | None => raw::yvex_backend_kind_YVEX_BACKEND_KIND_CUDA,
        _ => return Err("backend selector must name an admitted registry value".into()),
    };
    let kind = match operation {
        "system.backend" => raw::yvex_backend_report_kind_YVEX_BACKEND_REPORT_CAPABILITIES,
        "system.cuda" => raw::yvex_backend_report_kind_YVEX_BACKEND_REPORT_CUDA_INFO,
        "system.cuda.bandwidth" => raw::yvex_backend_report_kind_YVEX_BACKEND_REPORT_CUDA_BANDWIDTH,
        _ => return Err("unprojected backend operation".into()),
    };
    let report = ffi::backend_report(&raw::yvex_backend_report_request {
        kind,
        backend_kind: backend,
    })?;
    let text = render(&report, width, styled)?;
    Ok(Output::standard(text, u8::try_from(report.exit_code)?))
}

fn render(report: &raw::yvex_backend_report, width: usize, styled: bool) -> Result<String> {
    let backend = ffi::backend_name(report.backend_kind);
    if report.available == 0 {
        return Ok(presentation::record(
            &format!("BACKEND  {backend} · unavailable"),
            &[("reason", &ffi::text(&report.reason))],
            width,
            styled,
        )?);
    }
    let device = &report.device_info;
    let mut lines = vec![
        format!(
            "BACKEND  {backend} · {}",
            ffi::backend_status_name(report.backend_status)
        ),
        format!("DEVICE  {}", ffi::text(&report.device_name)),
        format!(
            "MEMORY  {} B allocated · {} allocations · {} B peak",
            report.memory.allocated_bytes,
            report.memory.allocation_count,
            report.memory.peak_allocated_bytes
        ),
    ];
    if report.has_device_info != 0 {
        lines[1] = format!(
            "DEVICE  {} · index {}",
            ffi::text(&report.device_name),
            device.device_index
        );
    }
    if report.backend_kind == raw::yvex_backend_kind_YVEX_BACKEND_KIND_CUDA {
        let architecture = ffi::text(&report.kernel_bundle_architecture);
        let identity = ffi::text(&report.kernel_bundle_identity);
        lines[1] = format!(
            "DEVICE  {} · SM {}.{} · {:.2} GiB free / {:.2} GiB total",
            ffi::text(&report.device_name),
            device.compute_capability_major,
            device.compute_capability_minor,
            device.free_memory_bytes as f64 / 1073741824.0,
            device.total_memory_bytes as f64 / 1073741824.0
        );
        lines.push(format!(
            "CUDA  context {} · bundle {} · {} · {}",
            if report.context_available != 0 {
                "available"
            } else {
                "unavailable"
            },
            ffi::bundle_admission_name(report.bundle_admission),
            if architecture.is_empty() {
                "unavailable"
            } else {
                &architecture
            },
            if report.bundle_admission
                != raw::yvex_backend_bundle_admission_YVEX_BACKEND_BUNDLE_ADMITTED
            {
                "no executable image"
            } else if report.kernel_bundle_native != 0 {
                "native"
            } else {
                "PTX"
            }
        ));
        lines.push(format!(
            "BUNDLE  {} · {}",
            if identity.is_empty() {
                "unavailable"
            } else {
                &identity
            },
            ffi::capability_reason_name(report.bundle_reason)
        ));
    }
    let mut output = presentation::lines(&lines, width, styled)?;
    if report.resources.schema != 0 {
        output.push_str(&resources(&report.resources, width, styled)?);
    }
    if report.kind == raw::yvex_backend_report_kind_YVEX_BACKEND_REPORT_CAPABILITIES {
        let rows = report
            .capabilities
            .iter()
            .enumerate()
            .map(|(index, supported)| {
                vec![
                    ffi::capability_name(index as raw::yvex_backend_capability),
                    if *supported != 0 {
                        "supported"
                    } else {
                        "unsupported"
                    }
                    .into(),
                ]
            })
            .collect::<Vec<_>>();
        output.push_str(&presentation::table(
            &[("PRIMITIVE", Alignment::Left), ("STATE", Alignment::Left)],
            &rows,
            width,
            styled,
        )?);
        if report.variant_count != 0 {
            let rows = report.variants[..report.variant_count as usize]
                .iter()
                .map(|variant| {
                    vec![
                        ffi::variant_name(variant.variant),
                        ffi::capability_state_name(variant.state),
                        ffi::capability_reason_name(variant.reason),
                    ]
                })
                .collect::<Vec<_>>();
            output.push_str(&presentation::table(
                &[
                    ("VARIANT", Alignment::Left),
                    ("STATE", Alignment::Left),
                    ("REASON", Alignment::Left),
                ],
                &rows,
                width,
                styled,
            )?);
        }
    } else if report.kind == raw::yvex_backend_report_kind_YVEX_BACKEND_REPORT_CUDA_BANDWIDTH {
        output.push_str(&bandwidth(&report.bandwidth, width, styled)?);
    }
    output.push_str("Primitive capability is not model execution qualification.\n");
    Ok(output)
}

fn resources(
    facts: &raw::yvex_backend_resource_facts,
    width: usize,
    styled: bool,
) -> Result<String> {
    let mut output = presentation::lines(
        &[
            format!(
                "RESOURCES  schema {} · known mask {}",
                facts.schema, facts.known
            ),
            format!(
                "STORAGE  {}",
                if facts.shared_system_memory != 0 {
                    "shared system memory"
                } else {
                    "separate memory domains"
                }
            ),
            format!(
                "COPIES  host write {} B · host read {} B · device {} B",
                facts.host_write_copy_bytes, facts.host_read_copy_bytes, facts.device_copy_bytes
            ),
        ],
        width,
        styled,
    )?;
    let rows = [
        (
            "addressable bytes",
            raw::yvex_backend_resource_fact_YVEX_BACKEND_RESOURCE_ADDRESSABLE,
            facts.addressable_bytes,
        ),
        (
            "mapped bytes",
            raw::yvex_backend_resource_fact_YVEX_BACKEND_RESOURCE_MAPPED,
            facts.mapped_bytes,
        ),
        (
            "allocated bytes",
            raw::yvex_backend_resource_fact_YVEX_BACKEND_RESOURCE_ALLOCATED,
            facts.allocated_bytes,
        ),
        (
            "device API allocated bytes",
            raw::yvex_backend_resource_fact_YVEX_BACKEND_RESOURCE_DEVICE_ALLOCATED,
            facts.device_allocated_bytes,
        ),
        (
            "resident bytes",
            raw::yvex_backend_resource_fact_YVEX_BACKEND_RESOURCE_RESIDENT,
            facts.resident_bytes,
        ),
        (
            "working set bytes",
            raw::yvex_backend_resource_fact_YVEX_BACKEND_RESOURCE_WORKING_SET,
            facts.working_set_bytes,
        ),
        (
            "recommended working set bytes",
            raw::yvex_backend_resource_fact_YVEX_BACKEND_RESOURCE_RECOMMENDED_WORKING_SET,
            facts.recommended_working_set_bytes,
        ),
        (
            "maximum buffer bytes",
            raw::yvex_backend_resource_fact_YVEX_BACKEND_RESOURCE_MAX_BUFFER,
            facts.max_buffer_bytes,
        ),
        (
            "temporary bytes",
            raw::yvex_backend_resource_fact_YVEX_BACKEND_RESOURCE_TEMPORARY,
            facts.temporary_bytes,
        ),
    ]
    .into_iter()
    .map(|(name, bit, value)| {
        vec![
            name.into(),
            if facts.known & bit != 0 {
                value.to_string()
            } else {
                "unmeasured".into()
            },
        ]
    })
    .collect::<Vec<_>>();
    output.push_str(&presentation::table(
        &[("RESOURCE", Alignment::Left), ("BYTES", Alignment::Right)],
        &rows,
        width,
        styled,
    )?);
    Ok(output)
}

fn bandwidth(
    evidence: &raw::yvex_backend_bandwidth_evidence,
    width: usize,
    styled: bool,
) -> Result<String> {
    let mut output = presentation::lines(
        &[
            format!(
                "BANDWIDTH  {} B working set · {} iterations · {} samples",
                evidence.working_set_bytes, evidence.iterations, evidence.sample_count
            ),
            format!(
                "RATE  read {:.3} GB/s · copy {:.3} GB/s · coherent host {:.3} GB/s",
                evidence.sustainable_read_bytes_per_second as f64 / 1e9,
                evidence.sustainable_copy_bytes_per_second as f64 / 1e9,
                evidence.sustainable_coherent_host_bytes_per_second as f64 / 1e9
            ),
            format!(
                "EVIDENCE  schema {} · {}",
                evidence.schema_version,
                ffi::text(&evidence.identity)
            ),
            format!("BUNDLE  {}", ffi::text(&evidence.kernel_bundle_identity)),
        ],
        width,
        styled,
    )?;
    let rows = (0..evidence.sample_count as usize)
        .map(|index| {
            vec![
                (index + 1).to_string(),
                evidence.stream_elapsed_ns[index].to_string(),
                evidence.copy_elapsed_ns[index].to_string(),
                evidence.coherent_host_elapsed_ns[index].to_string(),
            ]
        })
        .collect::<Vec<_>>();
    output.push_str(&presentation::table(
        &[
            ("SAMPLE", Alignment::Right),
            ("READ ns", Alignment::Right),
            ("COPY ns", Alignment::Right),
            ("HOST ns", Alignment::Right),
        ],
        &rows,
        width,
        styled,
    )?);
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn shared_memory_report_preserves_known_zero_and_unknown_residency() {
        let facts = raw::yvex_backend_resource_facts {
            schema: raw::YVEX_BACKEND_RESOURCE_SCHEMA,
            known: raw::yvex_backend_resource_fact_YVEX_BACKEND_RESOURCE_ALLOCATED,
            shared_system_memory: 1,
            resident_bytes: 123,
            ..Default::default()
        };
        let rendered = resources(&facts, 140, false).unwrap();
        assert!(rendered.contains("shared system memory"));
        let allocated = rendered
            .lines()
            .find(|line| line.contains("allocated bytes") && !line.contains("device API"))
            .unwrap();
        assert!(allocated.contains('0') && !allocated.contains("unmeasured"));
        let resident = rendered
            .lines()
            .find(|line| line.contains("resident bytes"))
            .unwrap();
        assert!(resident.contains("unmeasured") && !resident.contains("123"));
    }

    #[test]
    fn canonical_backend_selector_includes_metal_and_rocm() {
        assert_eq!(
            ffi::backend_kind("metal").unwrap(),
            raw::yvex_backend_kind_YVEX_BACKEND_KIND_METAL
        );
        assert_eq!(
            ffi::backend_kind("rocm").unwrap(),
            raw::yvex_backend_kind_YVEX_BACKEND_KIND_ROCM
        );
        assert!(ffi::backend_kind("unknown").is_err());
    }

    #[test]
    fn absent_bundle_is_not_presented_as_an_executable_ptx_image() {
        let report = raw::yvex_backend_report {
            backend_kind: raw::yvex_backend_kind_YVEX_BACKEND_KIND_CUDA,
            available: 1,
            context_available: 1,
            bundle_admission: raw::yvex_backend_bundle_admission_YVEX_BACKEND_BUNDLE_ABSENT,
            bundle_reason: raw::yvex_backend_capability_reason_YVEX_BACKEND_CAPABILITY_REASON_KERNEL_BUNDLE_ABSENT,
            ..Default::default()
        };
        let rendered = render(&report, 180, false).unwrap();
        assert!(rendered.contains("bundle absent · unavailable · no executable image"));
        assert!(rendered.contains("BUNDLE  unavailable · kernel-bundle-absent"));
        assert!(!rendered.contains("PTX"));
    }

    #[test]
    fn unsupported_report_preserves_reason_at_narrow_width_without_style() {
        let mut report = raw::yvex_backend_report {
            backend_kind: raw::yvex_backend_kind_YVEX_BACKEND_KIND_CUDA,
            ..Default::default()
        };
        ffi::put_text(&mut report.reason, "CUDA driver unavailable").unwrap();
        let rendered = render(&report, 24, false).unwrap();
        assert!(!rendered.contains('\x1b'));
        assert!(
            rendered
                .chars()
                .filter(|c| !c.is_whitespace())
                .collect::<String>()
                .contains("CUDAdriverunavailable"),
            "{rendered}"
        );
    }

    #[test]
    fn real_cpu_report_projects_native_primitive_facts_without_cuda_claims() {
        let report = ffi::backend_report(&raw::yvex_backend_report_request {
            kind: raw::yvex_backend_report_kind_YVEX_BACKEND_REPORT_CAPABILITIES,
            backend_kind: raw::yvex_backend_kind_YVEX_BACKEND_KIND_CPU,
        })
        .unwrap();
        assert_ne!(report.available, 0);
        assert_eq!(report.exit_code, 0);
        let rendered = render(&report, 100, false).unwrap();
        assert!(!rendered.contains('\x1b'));
        for index in 0..report.capabilities.len() {
            assert!(
                rendered.contains(&ffi::capability_name(index as raw::yvex_backend_capability))
            );
        }
        assert!(!rendered.contains("CUDA  context"));
        assert!(!rendered.contains("SM 0.0"));
        assert!(!rendered.contains("GiB free"));
    }
}
