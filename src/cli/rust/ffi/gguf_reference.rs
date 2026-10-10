//! Pinned external parser projection. Artifact admission remains native-owned.
use super::{Error, raw};
use std::{ffi::CStr, os::unix::fs::MetadataExt, path::Path};

#[allow(
    non_camel_case_types,
    non_snake_case,
    non_upper_case_globals,
    dead_code
)]
mod external {
    include!(concat!(env!("OUT_DIR"), "/gguf_reference.rs"));
}

struct Reader {
    gguf: *mut external::gguf_context,
    tensors: *mut external::ggml_context,
}
impl Drop for Reader {
    fn drop(&mut self) {
        unsafe {
            if !self.gguf.is_null() {
                external::gguf_free(self.gguf);
            }
            if !self.tensors.is_null() {
                external::ggml_free(self.tensors);
            }
        }
    }
}

fn require(ok: bool, reason: &str) -> Result<(), Error> {
    if ok {
        Ok(())
    } else {
        Err(Error {
            code: raw::yvex_status_YVEX_ERR_FORMAT,
            owner: "artifact.independent-reader".into(),
            message: reason.into(),
        })
    }
}

fn same_snapshot(a: &std::fs::Metadata, b: &std::fs::Metadata) -> bool {
    a.is_file()
        && b.is_file()
        && a.dev() == b.dev()
        && a.ino() == b.ino()
        && a.len() == b.len()
        && a.mtime() == b.mtime()
        && a.mtime_nsec() == b.mtime_nsec()
        && a.ctime() == b.ctime()
        && a.ctime_nsec() == b.ctime_nsec()
}

impl Reader {
    fn array(&self, name: &CStr, kind: external::gguf_type, count: u64) -> bool {
        unsafe {
            let key = external::gguf_find_key(self.gguf, name.as_ptr());
            key >= 0
                && external::gguf_get_kv_type(self.gguf, key) == external::gguf_type_GGUF_TYPE_ARRAY
                && external::gguf_get_arr_type(self.gguf, key) == kind
                && external::gguf_get_arr_n(self.gguf, key) as u64 == count
        }
    }
    fn string(&self, name: &CStr) -> Option<&CStr> {
        unsafe {
            let key = external::gguf_find_key(self.gguf, name.as_ptr());
            if key < 0
                || external::gguf_get_kv_type(self.gguf, key)
                    != external::gguf_type_GGUF_TYPE_STRING
            {
                return None;
            }
            let value = external::gguf_get_val_str(self.gguf, key);
            (!value.is_null()).then(|| CStr::from_ptr(value))
        }
    }
    fn tokenizer(&self, w: &raw::yvex_gguf_writer_plan_summary) -> bool {
        if w.tokenizer_token_count != 0 || w.tokenizer_merge_count != 0 {
            self.array(
                c"tokenizer.ggml.tokens",
                external::gguf_type_GGUF_TYPE_STRING,
                w.tokenizer_token_count,
            ) && self.array(
                c"tokenizer.ggml.token_type",
                external::gguf_type_GGUF_TYPE_INT32,
                w.tokenizer_token_count,
            ) && self.array(
                c"tokenizer.ggml.merges",
                external::gguf_type_GGUF_TYPE_STRING,
                w.tokenizer_merge_count,
            ) && self.string(c"tokenizer.huggingface.json").is_some()
                && self.string(c"yvex.tokenizer.config.json").is_some()
        } else {
            self.string(c"yvex.logical.target").is_some()
                && self.string(c"yvex.logical.component").is_some()
                && self
                    .string(c"yvex.logical.component.identity")
                    .is_some_and(|s| s.to_bytes().len() == 64)
        }
    }
    fn structure(&self, w: &raw::yvex_gguf_writer_plan_summary) -> Result<(), Error> {
        unsafe {
            require(
                external::gguf_get_version(self.gguf) == 3
                    && external::gguf_get_n_kv(self.gguf) as u64 == w.metadata_count
                    && external::gguf_get_n_tensors(self.gguf) as u64 == w.tensor_count
                    && external::gguf_get_alignment(self.gguf) == 32
                    && self.tokenizer(w),
                "official structure/tokenizer differs from the sealed writer",
            )?;
            let mut end = 0u64;
            for index in 0..w.tensor_count {
                let name = external::gguf_get_tensor_name(self.gguf, index as i64);
                require(
                    !name.is_null() && !CStr::from_ptr(name).to_bytes().is_empty(),
                    "missing tensor name",
                )?;
                let tensor = external::ggml_get_tensor(self.tensors, name);
                require(!tensor.is_null(), "missing official tensor")?;
                let offset = external::gguf_get_tensor_offset(self.gguf, index as i64) as u64;
                let size = external::gguf_get_tensor_size(self.gguf, index as i64) as u64;
                require(
                    (*tensor).type_ == external::gguf_get_tensor_type(self.gguf, index as i64)
                        && (1..=4).contains(&external::ggml_n_dims(tensor))
                        && size == external::ggml_nbytes(tensor) as u64
                        && offset == end,
                    "official tensor extent/type mismatch",
                )?;
                end = offset
                    .checked_add(size)
                    .and_then(|n| n.checked_add(31))
                    .ok_or_else(super::extent_error)?
                    & !31;
            }
            require(
                (external::gguf_get_data_offset(self.gguf) as u64).checked_add(end)
                    == Some(w.final_file_bytes),
                "official file extent mismatch",
            )
        }
    }
}

pub(super) fn verify(
    path: &CStr,
    writer: &raw::yvex_gguf_writer_plan_summary,
) -> Result<raw::yvex_artifact_official_reader_fact, Error> {
    use std::os::unix::ffi::OsStrExt;
    let location = Path::new(std::ffi::OsStr::from_bytes(path.to_bytes()));
    let metadata = |path: &Path| {
        std::fs::metadata(path).map_err(|e| Error {
            code: raw::yvex_status_YVEX_ERR_IO,
            owner: "artifact.independent-reader".into(),
            message: e.to_string(),
        })
    };
    let before = metadata(location)?;
    require(
        before.is_file() && before.len() == writer.final_file_bytes,
        "official file size mismatch",
    )?;
    let pin: serde_json::Value =
        serde_json::from_str(include_str!("../../../../config/gguf_reference.json"))
            .map_err(|_| super::extent_error())?;
    let revision = CStr::from_bytes_with_nul(raw::YVEX_GGUF_OFFICIAL_READER_REVISION)
        .map_err(|_| super::extent_error())?;
    require(
        pin["revision"].as_str() == revision.to_str().ok(),
        "reader pin differs from native admission contract",
    )?;
    let mut reader = Reader {
        gguf: std::ptr::null_mut(),
        tensors: std::ptr::null_mut(),
    };
    let params = external::gguf_init_params {
        no_alloc: true,
        ctx: &mut reader.tensors,
    };
    reader.gguf = unsafe { external::gguf_init_from_file(path.as_ptr(), params) };
    require(
        !reader.gguf.is_null() && !reader.tensors.is_null(),
        "official reader refused artifact",
    )?;
    reader.structure(writer)?;
    let after = metadata(location)?;
    require(
        same_snapshot(&before, &after),
        "artifact changed during independent verification",
    )?;
    let mut fact = raw::yvex_artifact_official_reader_fact {
        metadata_count: writer.metadata_count,
        tensor_count: writer.tensor_count,
        file_bytes: after.len(),
        file_device: after.dev(),
        file_inode: after.ino(),
        file_mtime_seconds: after.mtime(),
        file_mtime_nanoseconds: after.mtime_nsec(),
        file_ctime_seconds: after.ctime(),
        file_ctime_nanoseconds: after.ctime_nsec(),
        accepted: 1,
        ..Default::default()
    };
    for (out, byte) in fact.revision.iter_mut().zip(revision.to_bytes_with_nul()) {
        *out = *byte as _;
    }
    Ok(fact)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use std::sync::atomic::{AtomicU64, Ordering};

    struct Fixture(std::path::PathBuf);
    impl Drop for Fixture {
        fn drop(&mut self) {
            let _ = std::fs::remove_file(&self.0);
        }
    }
    fn fixture() -> (Fixture, raw::yvex_gguf_writer_plan_summary) {
        static ID: AtomicU64 = AtomicU64::new(0);
        let path = std::env::temp_dir().join(format!(
            "yvex-reader-{}-{}.gguf",
            std::process::id(),
            ID.fetch_add(1, Ordering::Relaxed)
        ));
        let mut bytes = Vec::new();
        bytes.extend(b"GGUF");
        bytes.extend(3u32.to_le_bytes());
        bytes.extend(1u64.to_le_bytes());
        bytes.extend(3u64.to_le_bytes());
        let string = |bytes: &mut Vec<u8>, value: &str| {
            bytes.extend((value.len() as u64).to_le_bytes());
            bytes.extend(value.as_bytes());
        };
        for (key, value) in [
            ("yvex.logical.target", "fixture"),
            ("yvex.logical.component", "weights"),
            (
                "yvex.logical.component.identity",
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            ),
        ] {
            string(&mut bytes, key);
            bytes.extend(8u32.to_le_bytes());
            string(&mut bytes, value);
        }
        string(&mut bytes, "weight");
        bytes.extend(1u32.to_le_bytes());
        bytes.extend(2u64.to_le_bytes());
        bytes.extend(0u32.to_le_bytes());
        bytes.extend(0u64.to_le_bytes());
        bytes.resize(bytes.len().next_multiple_of(32), 0);
        bytes.extend(1f32.to_le_bytes());
        bytes.extend(2f32.to_le_bytes());
        bytes.resize(bytes.len().next_multiple_of(32), 0);
        let mut file = std::fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&path)
            .unwrap();
        file.write_all(&bytes).unwrap();
        (
            Fixture(path),
            raw::yvex_gguf_writer_plan_summary {
                final_file_bytes: bytes.len() as u64,
                metadata_count: 3,
                tensor_count: 1,
                ..Default::default()
            },
        )
    }

    #[test]
    fn independent_reader_accepts_structure_and_rejects_wrong_facts() {
        let (fixture, writer) = fixture();
        let path = super::super::argument(fixture.0.to_str()).unwrap().unwrap();
        let fact = verify(&path, &writer).unwrap();
        assert_eq!(fact.accepted, 1);
        assert_eq!(fact.file_bytes, writer.final_file_bytes);
        assert_eq!(fact.tensor_count, 1);
        assert_ne!(fact.file_inode, 0);
        let mut wrong = writer;
        wrong.metadata_count += 1;
        assert!(verify(&path, &wrong).is_err());
        wrong = writer;
        wrong.tokenizer_token_count = 1;
        assert!(verify(&path, &wrong).is_err());
        wrong = writer;
        wrong.final_file_bytes += 1;
        assert!(verify(&path, &wrong).is_err());
    }

    #[test]
    fn reader_snapshot_and_invalid_container_refuse() {
        let (fixture, writer) = fixture();
        let before = std::fs::metadata(&fixture.0).unwrap();
        let mut file = std::fs::OpenOptions::new()
            .write(true)
            .open(&fixture.0)
            .unwrap();
        file.write_all(b"NOPE").unwrap();
        let path = super::super::argument(fixture.0.to_str()).unwrap().unwrap();
        assert!(verify(&path, &writer).is_err());
        file.set_len(1).unwrap();
        assert!(!same_snapshot(
            &before,
            &std::fs::metadata(&fixture.0).unwrap()
        ));
    }
}
