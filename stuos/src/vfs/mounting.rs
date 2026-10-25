use alloc::vec::Vec;
use spin::Mutex;

use crate::{fs2::VNode, kern_std::path::AbsolutePath};

static MOUNTS: Mutex<Vec<(AbsolutePath, VNode)>> = Mutex::new(Vec::new());

pub fn rs_vfs_add_mount(mount_against: AbsolutePath, filesystem_root: VNode) {
    MOUNTS.try_lock().unwrap()
    .push((mount_against, filesystem_root));
}

pub fn rs_vfs_resolve_mount(path: &AbsolutePath) -> Option<(AbsolutePath, VNode)> {
    MOUNTS.try_lock().unwrap()
    .iter()
    .filter(|(p,_)| path.is_subpath_of(p))//get only parent paths e.g get /a/b for /a/b/c/d
    .max_by_key(|(p, _)| p.into_iter().count())//get the longest parent, since that is the mount point 
    .map(|(p,v)| (p.clone(),v.clone()))
}