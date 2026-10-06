// Public same-user management socket. The computational Host socket is unrelated.
use crate::management_pairing::Result;
use rustix::fs::{FlockOperation, OFlags, flock};
use std::{
    fs,
    os::unix::{
        fs::{DirBuilderExt, FileTypeExt, MetadataExt, OpenOptionsExt, PermissionsExt},
        net::{UnixListener, UnixStream},
    },
    path::PathBuf,
};
pub(crate) struct LocalListener {
    pub listener: UnixListener,
    path: PathBuf,
    inode: u64,
    _lock: fs::File,
}
fn private_dir(path: &std::path::Path) -> Result<()> {
    if !path.exists() {
        fs::DirBuilder::new()
            .mode(0o700)
            .create(path)
            .map_err(|_| "local_management_directory_unavailable")?;
    }
    let m = fs::symlink_metadata(path).map_err(|_| "local_management_directory_unavailable")?;
    if !m.is_dir() || m.uid() != rustix::process::geteuid().as_raw() || m.mode() & 0o777 != 0o700 {
        return Err("unsafe_local_management_directory");
    }
    Ok(())
}
pub(crate) fn path() -> Result<PathBuf> {
    let parent = if let Some(root) = std::env::var_os("XDG_RUNTIME_DIR") {
        let root = PathBuf::from(root);
        if !root.is_absolute() {
            return Err("invalid_runtime_directory");
        }
        private_dir(&root)?;
        root.join("yvex")
    } else {
        PathBuf::from(format!("/tmp/yvex-{}", rustix::process::geteuid().as_raw()))
    };
    private_dir(&parent)?;
    let socket = parent.join("product-management.sock");
    if socket.as_os_str().len() >= 100 {
        return Err("local_management_path_bound");
    }
    Ok(socket)
}
pub(crate) fn bind() -> Result<LocalListener> {
    let path = path()?;
    let lock = fs::OpenOptions::new()
        .read(true)
        .write(true)
        .create(true)
        .truncate(false)
        .mode(0o600)
        .custom_flags(OFlags::NOFOLLOW.bits() as i32)
        .open(path.with_extension("lock"))
        .map_err(|_| "local_management_lock_unavailable")?;
    let m = lock
        .metadata()
        .map_err(|_| "unsafe_local_management_lock")?;
    if !m.is_file()
        || m.nlink() != 1
        || m.uid() != rustix::process::geteuid().as_raw()
        || m.mode() & 0o077 != 0
    {
        return Err("unsafe_local_management_lock");
    }
    flock(&lock, FlockOperation::NonBlockingLockExclusive)
        .map_err(|_| "local_management_already_running")?;
    if let Ok(m) = fs::symlink_metadata(&path) {
        if !m.file_type().is_socket()
            || m.uid() != rustix::process::geteuid().as_raw()
            || m.mode() & 0o077 != 0
        {
            return Err("unsafe_local_management_socket");
        }
        match UnixStream::connect(&path) {
            Ok(_) => return Err("local_management_already_running"),
            Err(error) if error.kind() == std::io::ErrorKind::ConnectionRefused => {
                fs::remove_file(&path).map_err(|_| "local_management_recovery_failed")?;
            }
            Err(_) => return Err("local_management_state_uncertain"),
        }
    }
    let listener = UnixListener::bind(&path).map_err(|_| "local_management_bind_failed")?;
    fs::set_permissions(&path, fs::Permissions::from_mode(0o600))
        .map_err(|_| "local_management_permissions_failed")?;
    listener
        .set_nonblocking(true)
        .map_err(|_| "local_management_bind_failed")?;
    let inode = fs::symlink_metadata(&path)
        .map_err(|_| "local_management_bind_failed")?
        .ino();
    Ok(LocalListener {
        listener,
        path,
        inode,
        _lock: lock,
    })
}
pub(crate) fn same_user(stream: &UnixStream) -> Result<u32> {
    #[cfg(target_os = "linux")]
    let uid = rustix::net::sockopt::socket_peercred(stream)
        .map_err(|_| "local_peer_unavailable")?
        .uid
        .as_raw();
    #[cfg(target_os = "macos")]
    let uid = nix::unistd::getpeereid(stream)
        .map_err(|_| "local_peer_unavailable")?
        .0
        .as_raw();
    #[cfg(not(any(target_os = "linux", target_os = "macos")))]
    return Err("local_peer_platform_unsupported");
    #[cfg(any(target_os = "linux", target_os = "macos"))]
    if uid != rustix::process::geteuid().as_raw() {
        Err("local_peer_not_owner")
    } else {
        Ok(uid)
    }
}
impl Drop for LocalListener {
    fn drop(&mut self) {
        if fs::symlink_metadata(&self.path)
            .is_ok_and(|m| m.ino() == self.inode && m.uid() == rustix::process::geteuid().as_raw())
        {
            let _ = fs::remove_file(&self.path);
        }
    }
}
