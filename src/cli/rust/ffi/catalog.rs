// Provider classification stays native; all records are copied before catalog retirement.
use super::{Account, Error, Library, argument, copied, error, extent_error, pointer, raw};

#[derive(serde::Serialize)]
pub(crate) struct ArtifactDiscovery {
    pub target_id: String,
    pub family: String,
    pub artifact_class: String,
    pub artifact_status: String,
    pub source_status: String,
    pub prepare_status: String,
    pub top_blocker: String,
    pub detail: String,
    pub tensor_map_status: String,
    pub output_head_map_status: String,
    pub tokenizer_map_status: String,
    pub prepare_blocker_count: u32,
    pub path: String,
    pub expected_path: String,
    pub display_path: String,
    pub registry_path: String,
    pub download_report_path: String,
    pub source_manifest_path: String,
    pub native_inventory_path: String,
    pub tensor_map_path: String,
    pub output_head_map_path: String,
    pub tokenizer_map_path: String,
    pub size_bytes: u64,
    pub dynamic_source: bool,
    pub next: String,
}
impl ArtifactDiscovery {
    fn copy(native: &raw::yvex_artifact_catalog_row) -> Result<Self, Error> {
        macro_rules! copy {
            ($($name:ident),*) => {
                Self { $($name: super::text(&native.$name),)*
                    size_bytes: native.size_bytes,
                    prepare_blocker_count: native.prepare_blocker_count,
                    dynamic_source: native.dynamic_source != 0,
                    next: unsafe { catalog_text(raw::yvex_artifact_catalog_next(native))? },
                }
            }
        }
        Ok(copy!(
            target_id,
            family,
            artifact_class,
            artifact_status,
            source_status,
            prepare_status,
            top_blocker,
            detail,
            tensor_map_status,
            output_head_map_status,
            tokenizer_map_status,
            path,
            expected_path,
            display_path,
            registry_path,
            download_report_path,
            source_manifest_path,
            native_inventory_path,
            tensor_map_path,
            output_head_map_path,
            tokenizer_map_path
        ))
    }
}

pub(crate) fn artifact_inventory(
    root: Option<&str>,
    family: Option<&str>,
) -> Result<Vec<ArtifactDiscovery>, Error> {
    struct Catalog(*mut raw::yvex_artifact_catalog);
    impl Drop for Catalog {
        fn drop(&mut self) {
            unsafe { raw::yvex_artifact_catalog_close(self.0) };
        }
    }
    let paths = super::Paths::with_models_root(root)?;
    let family = argument(family)?;
    let mut catalog = Catalog(std::ptr::null_mut());
    let mut failure = raw::yvex_error::default();
    super::execution::checked(
        unsafe {
            raw::yvex_artifact_catalog_open(
                &mut catalog.0,
                &paths.operator,
                pointer(&family),
                &mut failure,
            )
        },
        &failure,
    )?;
    if catalog.0.is_null() {
        return Err(extent_error());
    }
    (0..unsafe { raw::yvex_artifact_catalog_count(catalog.0) })
        .map(|index| {
            ArtifactDiscovery::copy(&copied(unsafe {
                raw::yvex_artifact_catalog_at(catalog.0, index)
            })?)
        })
        .collect()
}

pub(crate) fn acquired_target(
    root: Option<&str>,
    selector: &str,
) -> Result<Option<Box<raw::yvex_source_acquisition_provenance>>, Error> {
    let paths = super::Paths::with_models_root(root)?;
    let selector = argument(Some(selector))?.expect("required source selection");
    let mut record = Box::<raw::yvex_source_acquisition_provenance>::default();
    let mut failure = raw::yvex_error::default();
    let found = unsafe {
        raw::yvex_source_acquisition_provenance_resolve(
            selector.as_ptr(),
            &paths.operator,
            record.as_mut(),
            &mut failure,
        )
    };
    if found < 0 {
        return Err(error(failure.code, &failure));
    }
    Ok((found != 0).then_some(record))
}

pub(crate) enum RemoteRequest<'a> {
    Search {
        query: Option<&'a str>,
        author: Option<&'a str>,
        filter: Option<&'a str>,
        page: u32,
        limit: u32,
    },
    Inspect {
        repository: &'a str,
        revision: Option<&'a str>,
    },
}
pub(crate) struct RemoteModel {
    pub facts: raw::yvex_remote_model,
    pub representations: Vec<raw::yvex_model_representation>,
    pub files: Vec<raw::yvex_remote_file>,
}
pub(crate) struct RemoteSnapshot {
    pub query: String,
    pub provider_count: u64,
    pub models: Vec<RemoteModel>,
    pub sources: Vec<raw::yvex_local_source_record>,
    pub packages: Vec<raw::yvex_local_package_record>,
}
struct Remote(*mut raw::yvex_remote_catalog);
impl Drop for Remote {
    fn drop(&mut self) {
        unsafe { raw::yvex_remote_catalog_close(self.0) };
    }
}
struct Local(*mut raw::yvex_local_catalog);
impl Drop for Local {
    fn drop(&mut self) {
        unsafe { raw::yvex_local_catalog_close(self.0) };
    }
}

unsafe fn catalog_text(value: *const std::ffi::c_char) -> Result<String, Error> {
    if value.is_null() {
        Ok(String::new())
    } else {
        unsafe { super::borrowed_text(value) }
    }
}

macro_rules! catalog_projection {
    ($owner:ident, $native:ty, [$($field:ident),* $(,)?]) => {
        #[derive(serde::Serialize)]
        pub(crate) struct $owner { $(pub $field: String),* }
        impl $owner {
            fn copy(native: &$native) -> Result<Self, Error> {
                Ok(Self { $($field: unsafe { catalog_text(native.$field)? }),* })
            }
        }
    };
}
catalog_projection!(
    Target,
    raw::yvex_model_target_record,
    [
        target_id,
        family,
        model,
        target_class,
        source_artifact_class,
        target_artifact_class,
        pressure_purpose,
        tensor_set,
        local_path_class,
        source_footprint_class,
        runtime_boundary,
        runtime_execution,
        generation,
        external_reference,
    ]
);
catalog_projection!(
    TargetClass,
    raw::yvex_model_target_class_record,
    [
        class_id,
        capability_claim,
        runtime_execution,
        generation,
        description,
    ]
);

// Native strings have process lifetime; publish owned projections rather than
// lending their pointers or a human report row to ordinary product logic.
pub(crate) fn target(selector: &str) -> Result<Option<Target>, Error> {
    let selector = argument(Some(selector))?.expect("target selector");
    let record = unsafe { raw::yvex_model_target_find(selector.as_ptr()) };
    if record.is_null() {
        Ok(None)
    } else {
        Target::copy(&copied(record)?).map(Some)
    }
}
pub(crate) fn target_catalog() -> Result<Vec<Target>, Error> {
    (0..unsafe { raw::yvex_model_target_catalog_count() })
        .map(|index| {
            Target::copy(&copied(unsafe {
                raw::yvex_model_target_catalog_at(index)
            })?)
        })
        .collect()
}
pub(crate) fn target_classes() -> Result<Vec<TargetClass>, Error> {
    (0..unsafe { raw::yvex_model_target_class_count() })
        .map(|index| TargetClass::copy(&copied(unsafe { raw::yvex_model_target_class_at(index) })?))
        .collect()
}

#[derive(serde::Serialize)]
pub(crate) struct TargetSummary {
    pub source_status: String,
    pub artifact_status: String,
    pub runtime: String,
    pub next: String,
    pub boundary: String,
    pub release_selected: bool,
    pub upstream_repository: Option<String>,
    pub source_revision: Option<String>,
}
pub(crate) fn target_summary(selector: &str) -> Result<TargetSummary, Error> {
    let selector = argument(Some(selector))?.expect("target selector");
    let mut summary = raw::yvex_model_target_summary::default();
    let mut failure = raw::yvex_error::default();
    super::execution::checked(
        unsafe {
            raw::yvex_model_target_summary_get(selector.as_ptr(), &mut summary, &mut failure)
        },
        &failure,
    )?;
    let identity = if summary.release_identity.is_null() {
        None
    } else {
        Some(copied(summary.release_identity)?)
    };
    Ok(TargetSummary {
        source_status: unsafe { catalog_text(summary.source_status)? },
        artifact_status: unsafe { catalog_text(summary.artifact_status)? },
        runtime: unsafe { catalog_text(summary.runtime_status)? },
        next: unsafe { catalog_text(summary.next)? },
        boundary: unsafe { catalog_text(summary.boundary)? },
        release_selected: summary.release_selected != 0,
        upstream_repository: identity
            .as_ref()
            .map(|record| unsafe { catalog_text(record.upstream_repo_id) })
            .transpose()?,
        source_revision: identity
            .as_ref()
            .map(|record| unsafe { catalog_text(record.upstream_revision) })
            .transpose()?,
    })
}

fn open(request: RemoteRequest<'_>, provider: raw::yvex_account_provider) -> Result<Remote, Error> {
    let mut catalog = Remote(std::ptr::null_mut());
    let mut failure = raw::yvex_error::default();
    let status = match request {
        RemoteRequest::Search {
            query,
            author,
            filter,
            page,
            limit,
        } => {
            let query = argument(query)?;
            let author = argument(author)?;
            let filter = argument(filter)?;
            let options = raw::yvex_remote_search_options {
                provider,
                query: pointer(&query),
                author: pointer(&author),
                filter: pointer(&filter),
                page,
                page_size: limit,
            };
            unsafe { raw::yvex_remote_model_search(&mut catalog.0, &options, &mut failure) }
        }
        RemoteRequest::Inspect {
            repository,
            revision,
        } => {
            let repository = argument(Some(repository))?;
            let revision = argument(revision)?;
            let options = raw::yvex_remote_inspect_options {
                provider,
                repository: pointer(&repository),
                revision: pointer(&revision),
            };
            unsafe { raw::yvex_remote_model_inspect(&mut catalog.0, &options, &mut failure) }
        }
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    if catalog.0.is_null() {
        return Err(extent_error());
    }
    Ok(catalog)
}

pub(crate) fn remote_catalog(
    request: RemoteRequest<'_>,
    models_root: Option<&str>,
) -> Result<RemoteSnapshot, Error> {
    let account = Account::open("huggingface", None, None)?;
    let remote = open(request, account.provider)?;
    let root = argument(models_root)?;
    let options = raw::yvex_local_catalog_options {
        models_root: pointer(&root),
        registry_path: std::ptr::null(),
    };
    let mut local = Local(std::ptr::null_mut());
    let mut failure = raw::yvex_error::default();
    let status = unsafe { raw::yvex_local_catalog_open(&mut local.0, &options, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    if local.0.is_null() {
        return Err(extent_error());
    }
    let query = unsafe { super::borrowed_text(raw::yvex_remote_catalog_query(remote.0)) }?;
    let provider_count = unsafe { raw::yvex_remote_catalog_provider_count(remote.0) };
    let mut models = Vec::new();
    for index in 0..unsafe { raw::yvex_remote_catalog_count(remote.0) } {
        let facts = copied(unsafe { raw::yvex_remote_catalog_at(remote.0, index) })?;
        if facts.representation_count > raw::YVEX_REMOTE_MAX_REPRESENTATIONS {
            return Err(extent_error());
        }
        let representations = (0..facts.representation_count)
            .map(|ordinal| {
                copied(unsafe {
                    raw::yvex_remote_catalog_representation_at(remote.0, index, ordinal)
                })
            })
            .collect::<Result<Vec<_>, _>>()?;
        let files = (0..facts.available_file_count)
            .map(|ordinal| {
                copied(unsafe { raw::yvex_remote_catalog_file_at(remote.0, index, ordinal) })
            })
            .collect::<Result<Vec<_>, _>>()?;
        models.push(RemoteModel {
            facts,
            representations,
            files,
        });
    }
    let sources = (0..unsafe { raw::yvex_local_catalog_source_count(local.0) })
        .map(|index| copied(unsafe { raw::yvex_local_catalog_source_at(local.0, index) }))
        .collect::<Result<Vec<_>, _>>()?;
    let packages = (0..unsafe { raw::yvex_local_catalog_package_count(local.0) })
        .map(|index| copied(unsafe { raw::yvex_local_catalog_package_at(local.0, index) }))
        .collect::<Result<Vec<_>, _>>()?;
    Ok(RemoteSnapshot {
        query,
        provider_count,
        models,
        sources,
        packages,
    })
}

pub(crate) fn remote_kind(kind: raw::yvex_remote_model_kind) -> Result<String, Error> {
    unsafe { super::borrowed_text(raw::yvex_remote_model_kind_name(kind)) }
}
pub(crate) fn support_stage(stage: raw::yvex_model_support_stage) -> Result<String, Error> {
    unsafe { super::borrowed_text(raw::yvex_model_support_stage_name(stage)) }
}
pub(crate) fn remote_file_kind(kind: raw::yvex_remote_file_kind) -> Result<String, Error> {
    unsafe { super::borrowed_text(raw::yvex_remote_file_kind_name(kind)) }
}

pub(crate) struct StorageFacts {
    pub rows: Vec<raw::yvex_model_storage_row>,
    pub logical_bytes: u64,
    pub allocated_bytes: u64,
}
struct Storage(raw::yvex_model_storage_report);
impl Drop for Storage {
    fn drop(&mut self) {
        unsafe { raw::yvex_model_storage_report_free(&mut self.0) };
    }
}
impl Library {
    pub(crate) fn storage(
        &self,
        index: u64,
        root: &str,
        caches: bool,
    ) -> Result<StorageFacts, Error> {
        let root = argument(Some(root))?.expect("resolved model root");
        let mut native = Storage(raw::yvex_model_storage_report::default());
        let mut failure = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_model_storage_inspect(
                self.native.as_ptr(),
                index,
                root.as_ptr(),
                caches.into(),
                &mut native.0,
                &mut failure,
            )
        };
        if status != 0 {
            return Err(error(status, &failure));
        }
        let count = u64::try_from(native.0.count).map_err(|_| extent_error())?;
        let rows = unsafe { super::copy_view(native.0.rows, count) }?;
        Ok(StorageFacts {
            rows,
            logical_bytes: native.0.logical_bytes,
            allocated_bytes: native.0.allocated_bytes,
        })
    }

    pub(crate) fn evict(
        &self,
        index: u64,
        representation: u64,
        source: bool,
        root: &str,
        dry_run: bool,
    ) -> Result<raw::yvex_model_storage_result, Error> {
        let root = argument(Some(root))?.expect("resolved model root");
        let mut result = raw::yvex_model_storage_result::default();
        let mut failure = raw::yvex_error::default();
        let operation = if source {
            raw::yvex_model_source_evict
        } else {
            raw::yvex_model_local_evict
        };
        let status = unsafe {
            operation(
                self.native.as_ptr(),
                index,
                representation,
                root.as_ptr(),
                dry_run.into(),
                &mut result,
                &mut failure,
            )
        };
        if status != 0 {
            return Err(error(status, &failure));
        }
        Ok(result)
    }
}

pub(crate) fn export(
    source: &str,
    destination: &str,
    digest: &str,
) -> Result<raw::yvex_source_export_result, Error> {
    let source = argument(Some(source))?.expect("selected representation");
    let destination = argument(Some(destination))?.expect("selected destination");
    let digest = argument(Some(digest))?.expect("selected identity");
    let options = raw::yvex_source_export_options {
        source_path: source.as_ptr(),
        destination_path: destination.as_ptr(),
        expected_digest: digest.as_ptr(),
    };
    let mut result = raw::yvex_source_export_result::default();
    let mut failure = raw::yvex_error::default();
    let status = unsafe { raw::yvex_source_export_local(&options, &mut result, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(result)
}

pub(crate) fn locator(value: &str) -> Result<raw::yvex_source_locator, Error> {
    let value = argument(Some(value))?.expect("required source locator");
    let mut result = raw::yvex_source_locator::default();
    let mut failure = raw::yvex_error::default();
    let status =
        unsafe { raw::yvex_source_locator_parse(value.as_ptr(), &mut result, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(result)
}

pub(crate) fn locator_kind(kind: raw::yvex_source_locator_kind) -> Result<String, Error> {
    unsafe { super::borrowed_text(raw::yvex_source_locator_kind_name(kind)) }
}

pub(crate) fn local_representation(
    locator: &raw::yvex_source_locator,
    root: &str,
) -> Result<raw::yvex_source_representation_fact, Error> {
    let root = argument(Some(root))?.expect("resolved model root");
    let mut result = raw::yvex_source_representation_fact::default();
    let mut failure = raw::yvex_error::default();
    let status = unsafe {
        raw::yvex_source_representation_resolve_local(
            locator,
            root.as_ptr(),
            &mut result,
            &mut failure,
        )
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(result)
}

pub(crate) fn existing_content(
    root: &str,
    digest: &str,
) -> Result<raw::yvex_model_remote_selection, Error> {
    let root = argument(Some(root))?.expect("resolved model root");
    let digest = argument(Some(digest))?.expect("exact content identity");
    let mut result = raw::yvex_model_remote_selection::default();
    let mut failure = raw::yvex_error::default();
    let status = unsafe {
        raw::yvex_model_local_content_resolve(
            root.as_ptr(),
            digest.as_ptr(),
            &mut result,
            &mut failure,
        )
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(result)
}

pub(crate) struct ImportRequest<'a> {
    pub locator: &'a raw::yvex_source_locator,
    pub inspected: &'a raw::yvex_source_representation_fact,
    pub root: &'a str,
    pub name: Option<&'a str>,
    pub family: Option<&'a str>,
    pub storage: raw::yvex_source_storage_kind,
}
pub(crate) fn import(request: &ImportRequest<'_>) -> Result<raw::yvex_source_import_result, Error> {
    let root = argument(Some(request.root))?;
    let name = argument(request.name)?;
    let family = argument(request.family)?;
    let options = raw::yvex_source_import_options {
        locator: request.locator,
        inspected: request.inspected,
        models_root: pointer(&root),
        name: pointer(&name),
        family: pointer(&family),
        storage: request.storage,
    };
    let mut result = raw::yvex_source_import_result::default();
    let mut failure = raw::yvex_error::default();
    let status = unsafe { raw::yvex_source_import_local(&options, &mut result, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(result)
}
