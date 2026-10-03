// Native preparation leases authenticate sealed plans; Rust never borrows C presentation.
use super::{Error, Library, Paths, argument, borrowed_text, error, extent_error, pointer, raw};
use std::{
    ffi::CString,
    os::fd::{FromRawFd, OwnedFd},
};

pub(crate) struct Request<'a> {
    pub root: Option<&'a str>,
    pub registry: Option<&'a str>,
    pub quant: Option<&'a str>,
    pub imatrix: Option<&'a str>,
    pub dry: bool,
}
struct Inputs {
    strings: Vec<Option<CString>>,
    dry: bool,
}
impl Inputs {
    fn new(request: Request<'_>) -> Result<Self, Error> {
        Ok(Self {
            strings: [
                request.root,
                request.registry,
                request.quant,
                request.imatrix,
            ]
            .into_iter()
            .map(argument)
            .collect::<Result<_, _>>()?,
            dry: request.dry,
        })
    }
    fn native(&self) -> raw::yvex_model_preparation_request {
        raw::yvex_model_preparation_request {
            models_root: pointer(&self.strings[0]),
            registry_path: pointer(&self.strings[1]),
            quant: pointer(&self.strings[2]),
            imatrix: pointer(&self.strings[3]),
            dry_run: self.dry.into(),
        }
    }
}
fn checked(status: i32, failure: &raw::yvex_error) -> Result<(), Error> {
    if status == 0 {
        Ok(())
    } else {
        Err(error(status, failure))
    }
}

pub(crate) fn lock(root: Option<&str>, selector: &str) -> Result<OwnedFd, Error> {
    let paths = Paths::with_models_root(root)?;
    let root = argument(Some(&super::text(&paths.operator.models_root)))?;
    let scope = argument(Some("model.prepare"))?;
    let selector = argument(Some(selector))?;
    let mut descriptor = -1;
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_source_acquisition_lock(
                pointer(&root),
                pointer(&scope),
                pointer(&selector),
                &mut descriptor,
                &mut failure,
            )
        },
        &failure,
    )?;
    if descriptor < 0 {
        return Err(extent_error());
    }
    // The native acquisition owner transfers one descriptor; its RAII lifetime
    // spans catalog selection and final registry publication, including refusal.
    Ok(unsafe { OwnedFd::from_raw_fd(descriptor) })
}

pub(crate) fn verify_ready(
    library: &Library,
    index: u64,
    request: Request<'_>,
) -> Result<(), Error> {
    let inputs = Inputs::new(request)?;
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_model_preparation_ready_verify(
                library.native.as_ptr(),
                index,
                &inputs.native(),
                &mut failure,
            )
        },
        &failure,
    )
}

pub(crate) struct Preparation<'a> {
    native: *mut raw::yvex_model_preparation,
    library: &'a Library,
    inputs: Inputs,
}
impl Drop for Preparation<'_> {
    fn drop(&mut self) {
        unsafe { raw::yvex_model_preparation_close(self.native) };
    }
}
#[derive(serde::Serialize)]
pub(crate) struct View {
    pub source: String,
    pub revision: String,
    pub target: String,
    pub quant: String,
    pub backend: String,
    pub strategy: String,
    pub family: String,
    pub model: String,
    pub models_root: String,
    pub registry: String,
    pub manifest: String,
    pub plan: String,
    pub artifact: String,
    pub binding: String,
    pub profile: String,
    pub rebind: bool,
}
#[derive(serde::Serialize)]
pub(crate) struct Recipe {
    pub target: String,
    pub family: String,
    pub architecture: String,
    pub tensor: String,
    pub qtype: String,
    pub artifact_leaf: String,
    pub plan_leaf: String,
    pub repository: String,
    pub revision: String,
    pub reason: String,
    pub artifact_tensor: String,
    pub gate_label: String,
    pub gate_dims: [u64; 2],
    pub gate_bytes: u64,
    pub implemented: bool,
}
pub(crate) fn recipe(target: &str) -> Result<Recipe, Error> {
    let target = argument(Some(target))?.expect("required target");
    let mut record = raw::yvex_model_preparation_recipe::default();
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_model_preparation_recipe_get(target.as_ptr(), &mut record, &mut failure)
        },
        &failure,
    )?;
    // Catalog pointers have process lifetime; expose only owned, bounded copies.
    let copy = |text: *const std::ffi::c_char| {
        if text.is_null() {
            Ok(String::new())
        } else {
            unsafe { borrowed_text(text) }
        }
    };
    Ok(Recipe {
        target: copy(record.target)?,
        family: copy(record.family)?,
        architecture: copy(record.architecture)?,
        tensor: copy(record.tensor)?,
        qtype: copy(record.qtype)?,
        artifact_leaf: copy(record.artifact_leaf)?,
        plan_leaf: copy(record.plan_leaf)?,
        repository: copy(record.repository)?,
        revision: copy(record.revision)?,
        reason: copy(record.reason)?,
        artifact_tensor: copy(record.artifact_tensor)?,
        gate_label: copy(record.gate_label)?,
        gate_dims: record.gate_dims,
        gate_bytes: record.gate_bytes,
        implemented: record.implemented != 0,
    })
}

impl<'a> Preparation<'a> {
    pub(crate) fn open(
        library: &'a Library,
        index: u64,
        request: Request<'_>,
    ) -> Result<Self, Error> {
        let mut context = Self {
            native: std::ptr::null_mut(),
            library,
            inputs: Inputs::new(request)?,
        };
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe {
                raw::yvex_model_preparation_open(
                    &mut context.native,
                    library.native.as_ptr(),
                    index,
                    &context.inputs.native(),
                    &mut failure,
                )
            },
            &failure,
        )?;
        if context.native.is_null() {
            return Err(extent_error());
        }
        Ok(context)
    }
    pub(crate) fn view(&self) -> Result<View, Error> {
        let mut view = raw::yvex_model_preparation_view::default();
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe { raw::yvex_model_preparation_view_get(self.native, &mut view, &mut failure) },
            &failure,
        )?;
        // No interior pointer or borrowed catalog record leaves this seam.
        macro_rules! copy {
            ($($name:ident),*) => {
                Ok(View { $($name: unsafe { borrowed_text(view.$name)? },)* rebind: view.rebind != 0 })
            };
        }
        copy!(
            source,
            revision,
            target,
            quant,
            backend,
            strategy,
            family,
            model,
            models_root,
            registry,
            manifest,
            plan,
            artifact,
            binding,
            profile
        )
    }
    pub(crate) fn verify(&mut self) -> Result<(), Error> {
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe { raw::yvex_model_preparation_verify(self.native, &mut failure) },
            &failure,
        )
    }
    pub(crate) fn store_plan(&mut self) -> Result<(), Error> {
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe { raw::yvex_model_preparation_store_plan(self.native, &mut failure) },
            &failure,
        )
    }
    pub(crate) fn cached(&mut self, index: u64) -> Result<bool, Error> {
        let mut cached = 0;
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe {
                raw::yvex_model_preparation_cached(
                    self.native,
                    self.library.native.as_ptr(),
                    index,
                    &mut cached,
                    &mut failure,
                )
            },
            &failure,
        )?;
        Ok(cached != 0)
    }
    pub(crate) fn verify_artifact(&mut self) -> Result<(), Error> {
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe { raw::yvex_model_preparation_artifact_verify(self.native, &mut failure) },
            &failure,
        )
    }
    pub(crate) fn binding(&mut self) -> Result<bool, Error> {
        let mut published = 0;
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe {
                raw::yvex_model_preparation_binding_publish(
                    self.native,
                    &mut published,
                    &mut failure,
                )
            },
            &failure,
        )?;
        Ok(published != 0)
    }
}
