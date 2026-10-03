// Exact remote selection and adoption stay Model/Source-owned, not name heuristics.
use super::{Error, argument, copied, execution::checked, pointer, raw};
pub(crate) type Selection = raw::yvex_model_remote_selection;
pub(crate) fn retained(
    root: &str,
    repository: &str,
    revision: Option<&str>,
    variant: Option<&str>,
    format: Option<&str>,
) -> Result<Selection, Error> {
    let values = [Some(root), Some(repository), revision, variant, format]
        .into_iter()
        .map(argument)
        .collect::<Result<Vec<_>, _>>()?;
    let mut out = Selection::default();
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_model_remote_selection_resolve(
                pointer(&values[0]),
                pointer(&values[1]),
                pointer(&values[2]),
                pointer(&values[3]),
                pointer(&values[4]),
                &mut out,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok(out)
}
pub(crate) fn retained_source(
    root: &str,
    repository: &str,
    revision: Option<&str>,
    variant: Option<&str>,
    format: Option<&str>,
) -> Result<Option<raw::yvex_local_source_record>, Error> {
    let values = [Some(root), Some(repository), revision, variant, format]
        .into_iter()
        .map(argument)
        .collect::<Result<Vec<_>, _>>()?;
    let mut out = raw::yvex_local_source_record::default();
    let mut found = 0;
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_model_remote_source_resolve(
                pointer(&values[0]),
                pointer(&values[1]),
                pointer(&values[2]),
                pointer(&values[3]),
                pointer(&values[4]),
                &mut out,
                &mut found,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok((found != 0).then_some(out))
}
pub(crate) fn retained_revision(root: &str, repository: &str) -> Result<Option<String>, Error> {
    let root = argument(Some(root))?.expect("root");
    let repository = argument(Some(repository))?.expect("repository");
    let mut revision = [0; raw::YVEX_REMOTE_REVISION_CAP as usize];
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_model_remote_revision_resolve(
                root.as_ptr(),
                repository.as_ptr(),
                revision.as_mut_ptr(),
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok((revision[0] != 0).then(|| super::text(&revision)))
}
pub(crate) fn materialize(
    selection: &mut Selection,
    root: &str,
    anonymous: bool,
) -> Result<raw::yvex_model_storage_result, Error> {
    let root = argument(Some(root))?.expect("root");
    let mut out = raw::yvex_model_storage_result::default();
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_model_remote_materialize(
                selection,
                root.as_ptr(),
                anonymous.into(),
                &mut out,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok(out)
}
pub(crate) fn materialize_source(
    source: &raw::yvex_local_source_record,
    root: &str,
    anonymous: bool,
) -> Result<raw::yvex_model_storage_result, Error> {
    let root = argument(Some(root))?.expect("root");
    let mut out = raw::yvex_model_storage_result::default();
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_model_source_file_materialize(
                source,
                root.as_ptr(),
                anonymous.into(),
                &mut out,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok(out)
}

pub(crate) struct Inspection {
    native: *mut raw::yvex_remote_catalog,
    pub model: super::catalog::RemoteModel,
}
impl Drop for Inspection {
    fn drop(&mut self) {
        unsafe { raw::yvex_remote_catalog_close(self.native) };
    }
}
impl Inspection {
    pub(crate) fn open(
        repository: &str,
        revision: Option<&str>,
        anonymous: bool,
    ) -> Result<Self, Error> {
        let repository = argument(Some(repository))?.expect("repository");
        let revision = argument(revision)?;
        let options = raw::yvex_remote_inspect_options {
            provider: raw::yvex_account_provider_YVEX_ACCOUNT_PROVIDER_HUGGINGFACE,
            repository: repository.as_ptr(),
            revision: pointer(&revision),
        };
        let mut native = std::ptr::null_mut();
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe {
                raw::yvex_model_remote_inspect_policy(
                    &mut native,
                    &options,
                    anonymous.into(),
                    &mut failure,
                )
            },
            &failure,
        )?;
        // Establish RAII before copying any fallible native record.
        let mut inspection = Self {
            native,
            model: super::catalog::RemoteModel {
                facts: raw::yvex_remote_model::default(),
                representations: Vec::new(),
                files: Vec::new(),
            },
        };
        inspection.model.facts = copied(unsafe { raw::yvex_remote_catalog_at(native, 0) })?;
        let facts = &inspection.model.facts;
        if facts.representation_count > raw::YVEX_REMOTE_MAX_REPRESENTATIONS {
            return Err(super::extent_error());
        }
        inspection.model.representations = (0..facts.representation_count)
            .map(|index| {
                copied(unsafe { raw::yvex_remote_catalog_representation_at(native, 0, index) })
            })
            .collect::<Result<_, _>>()?;
        inspection.model.files = (0..facts.available_file_count)
            .map(|index| copied(unsafe { raw::yvex_remote_catalog_file_at(native, 0, index) }))
            .collect::<Result<_, _>>()?;
        Ok(inspection)
    }
    pub(crate) fn adopt(&self, index: usize, root: &str, dry: bool) -> Result<Selection, Error> {
        let root = argument(Some(root))?.expect("root");
        if index >= self.model.representations.len() {
            return Err(super::extent_error());
        }
        let representation = unsafe {
            raw::yvex_remote_catalog_representation_at(
                self.native,
                0,
                index.try_into().map_err(|_| super::extent_error())?,
            )
        };
        let mut out = Selection::default();
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe {
                raw::yvex_model_remote_adopt_existing(
                    self.native,
                    representation,
                    root.as_ptr(),
                    dry.into(),
                    &mut out,
                    &mut failure,
                )
            },
            &failure,
        )?;
        Ok(out)
    }
}

pub(crate) fn reference(
    locator: &raw::yvex_source_locator,
    root: &str,
    name: &str,
    family: &str,
    revision: &str,
    representation: &raw::yvex_model_representation,
) -> Result<raw::yvex_source_reference_result, Error> {
    let values = [
        root,
        name,
        family,
        revision,
        &super::text(&representation.format),
        &super::text(&representation.precision),
    ]
    .into_iter()
    .map(|value| argument(Some(value)))
    .collect::<Result<Vec<_>, _>>()?;
    let options = raw::yvex_source_reference_options {
        locator,
        models_root: pointer(&values[0]),
        name: pointer(&values[1]),
        family: pointer(&values[2]),
        resolved_revision: pointer(&values[3]),
        format: pointer(&values[4]),
        precision: pointer(&values[5]),
        size_bytes: representation.size_bytes,
        size_known: representation.size_known,
        ..Default::default()
    };
    let mut out = raw::yvex_source_reference_result::default();
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe { raw::yvex_source_register_reference(&options, &mut out, &mut failure) },
        &failure,
    )?;
    Ok(out)
}
pub(crate) fn prepare_selector(
    repository: &str,
    revision: &str,
    default: &str,
) -> Result<String, Error> {
    let repository = argument(Some(repository))?.expect("repository");
    let target =
        unsafe { raw::yvex_source_target_identity_find_repository(repository.as_ptr()).as_ref() };
    if let Some(target) = target
        && unsafe { super::borrowed_text(target.upstream_revision)? } == revision
    {
        return unsafe { super::borrowed_text(target.target_id) };
    }
    Ok(default.into())
}
