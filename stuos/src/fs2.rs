use core::{ffi::CStr, hash::Hash, mem::MaybeUninit};

use alloc::vec::Vec;
use foldhash::fast::FixedState;
use hashbrown::HashMap;
use spin::Mutex;
use thiserror::Error;

use crate::{kern_std::path::{AbsolutePath, PathWithCwdErr}, rs_uapi::syscalls::{O_APPEND, O_CREAT, O_DIRECTORY, O_NONBLOCK, O_TRUNC, S_IFDIR, S_IFREG, dirent, mode_t, stat}, vfs::mounting::{rs_vfs_add_mount, rs_vfs_resolve_mount}};

unsafe extern "C" {
    fn vfs_get_root() -> VNode;
}

#[doc = " Represents an abstract node in the filesystem, which can be a file or directory"]
#[repr(C)]
// TODO just hash the VNodeData?
#[derive(Debug, Copy, Clone, Eq, Default)]
pub struct VNode {
    pub id: VNodeData,
    pub stat_file: ::core::option::Option<unsafe extern "C" fn(id: VNodeData) -> stat>,
    #[doc = " This function reads directory entries, and fills dirent_buf and vnode_buf accordingly\n\n dirent_buf and/or vnode_buf can be NULL\n\n Returns the number of items put into dirent_buf and vnode_buf"]
    pub read_dirents: ::core::option::Option<
        unsafe extern "C" fn(
            inode_num: VNodeData,
            dirent_index: usize,
            dirent_buf: *mut dirent,
            vnode_buf: *mut VNode,
            dirent_count: usize,
        ) -> usize,
    >,
    #[doc = " This function should check that file is a valid file, then try to write data from `input_buf` to index `offset`\n\n Returns:\n number of bytes written"]
    pub write_file: ::core::option::Option<
        unsafe extern "C" fn(
            inode_num: VNodeData,
            offset: usize,
            input_buf: *const (),
            num_bytes: usize,
        ) -> usize,
    >,
    #[doc = " This function should check that file is a valid file, then try to read `num_bytes` bytes into `out` at index in the file `offset`\n\n This operation is also valid for open directories, where the directory pretends to be a list of struct dirent. may crash if offset or num_bytes are not a multiple of sizeof(struct dirent)\n\n Returns:\n number of bytes read"]
    pub read_file: ::core::option::Option<
        unsafe extern "C" fn(
            inode_num: VNodeData,
            offset: usize,
            output_buf: *mut (),
            num_bytes: usize,
        ) -> usize,
    >,
    #[doc = " This function should check that `parent` is a directory node, then create a new inode of type `new_inode_type`, returning the VNode that was created into out.\n parent.inode_number == 0 means that there is no parent, and the inode that is created is the root node\n `name` is cloned\n\n Returns:\n 0 on success"]
    pub create_inode: ::core::option::Option<
        unsafe extern "C" fn(
            parent_inode_num: VNodeData,
            new_inode_type: mode_t,
            name: *const ::core::ffi::c_char,
        ) -> ::core::ffi::c_int,
    >,
    #[doc = " This function should check that file is a valid file, then either truncate data or zero-extend to len bytes"]
    pub ftruncate:
        ::core::option::Option<unsafe extern "C" fn(inode_num: VNodeData, len: usize)>,
}

impl Hash for VNode {
    fn hash<H: core::hash::Hasher>(&self, state: &mut H) {
        self.id.hash(state);
        self.stat_file.hash(state);
        self.read_dirents.hash(state);
        self.write_file.hash(state);
        self.read_file.hash(state);
        self.create_inode.hash(state);
        self.ftruncate.hash(state);
    }
}
impl PartialEq for VNode {
    #[allow(unpredictable_function_pointer_comparisons)]
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id &&
        self.stat_file == other.stat_file && self.read_dirents == other.read_dirents && self.write_file == other.write_file && self.read_file == other.read_file && self.create_inode == other.create_inode && self.ftruncate == other.ftruncate
    }
}

impl VNode {
    pub fn is_null(&self) -> bool {
        return true &&
            self.id.inode == 0 &&
            self.id.mount_id == 0 &&
            self.read_dirents.is_none() &&
            self.write_file.is_none() &&
            self.read_file.is_none() &&
            self.create_inode.is_none() &&
            self.stat_file.is_none();
    }
}

fn directory_lookup(dir: &VNode, name: &str) -> Result<VNode, ()> {
    let mut vnode_buf = VNode::default();
    let mut dirent_buf = dirent { d_ino: 0, d_type: 0, d_name: [0;_] };

    let mut offset = 0usize;
    loop {
        let count = unsafe { dir.read_dirents.unwrap()(dir.id, offset, &raw mut dirent_buf, &raw mut vnode_buf, 1) };
        if count == 0 {
            return Err(());//not found
        }
        assert!(count == 1);
        let item_name = unsafe { CStr::from_ptr(dirent_buf.d_name.as_ptr()) };
        if item_name.to_str().unwrap() == name {
            return Ok(vnode_buf)
        }
        offset += count;
    }
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, Default)]
pub struct VNodeData {
    #[doc = " Unique ID per file under a mount"]
    pub inode: u64,
    pub mount_id: u64,
}

#[unsafe(no_mangle)]
pub extern "C" fn vfs_add_mount(mount_against: *const i8, filesystem_root: VNode) {
    let mount_against = unsafe {CStr::from_ptr(mount_against)}.to_str().unwrap();
    let mount_against = AbsolutePath::make_cwd(mount_against).unwrap();
    rs_vfs_add_mount(mount_against, filesystem_root);
}

#[unsafe(no_mangle)]
pub extern "C" fn c_vfs_get(cwd_path: *const i8, path: *const i8, open_flags: i32) -> VNode {
    let cwd_path = AbsolutePath::make_cwd(unsafe {CStr::from_ptr(cwd_path)}.to_str().unwrap()).unwrap();
    let path = unsafe {CStr::from_ptr(path)}.to_str().unwrap();
    rs_vfs_get(cwd_path, path, open_flags.into()).unwrap()
}

#[derive(Error, Debug)]
enum VfsGetError {
    #[error(transparent)]
    PathWithCwdErr(#[from] PathWithCwdErr),
    #[error("ENOTDIR")]
    ENotDir,
    #[error("ENOENT")]
    ENoEnt,
}

struct OpenOptions {
    o_creat: bool,
    o_append: bool,
    o_directory: bool,
    o_trunc: bool,

}
impl From<i32> for OpenOptions {
    fn from(value: i32) -> Self {
        assert!(value & !(O_CREAT | O_APPEND | O_DIRECTORY | O_TRUNC | O_NONBLOCK) == 0);//must only be flags I recognise
        Self {
            o_creat: (value & O_CREAT) != 0,
            o_append: (value & O_APPEND) != 0,
            o_directory: (value & O_DIRECTORY) != 0,
            o_trunc: (value & O_TRUNC) != 0,
            //ignore O_NONBLOCK
        }
    }
}
fn rs_vfs_get(cwd: AbsolutePath, path: &str, open_flags: OpenOptions) -> Result<VNode, VfsGetError> {
    //resolve .. and .
    let full_path = AbsolutePath::with_cwd(&cwd, path)?;
    //find the mount point and strip its prefix
    let (full_path, mut curr) = rs_vfs_resolve_mount(&full_path).expect("not even / is mounted!");
    //loop path segments
    let mut segments = full_path.into_iter().peekable();
    while let Some(segment) = segments.next() {
        let is_last = segments.peek().is_none();
        match directory_lookup(&curr, segment) {
            Ok(vnode) => {
                curr = vnode;
            },
            Err(()) => {
                if is_last && open_flags.o_creat {
                    let mode = if open_flags.o_directory {S_IFDIR} else {S_IFREG};
                    let return_value = unsafe { curr.create_inode.unwrap()(curr.id, mode.try_into().unwrap(), segment.as_ptr() as *const i8) };
                    assert!(return_value == 0);//TODO error handling
                    todo!("find the newly created file and return it")
                } else {
                    return Err(VfsGetError::ENoEnt)
                }
            }
        }
    }

    let stat_data = unsafe { curr.stat_file.unwrap()(curr.id) };
    if (stat_data.st_mode & S_IFDIR as u32 != 0) != open_flags.o_directory {
        return Err(VfsGetError::ENotDir)
    }

    Ok(curr)
}