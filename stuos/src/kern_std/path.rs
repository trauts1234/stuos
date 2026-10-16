use core::str::Split;

use alloc::{string::{String, ToString}, vec::Vec};

#[derive(Debug, Clone)]
pub struct AbsolutePath {
    /// leading / is implicit and not stored
    /// there is never a trailing /
    data: String
}

#[derive(Debug)]
pub enum PathWithCwdErr {
    /// path contains \0
    ContainsNullByte,
    /// path contains //
    ContainsEmptySegment,
    /// path tries to .. on the root directory
    TooManyDotDot
}
#[derive(Debug)]
pub enum MakeCwdErr {
    /// path contains \0
    ContainsNullByte,
    /// path contains //
    ContainsEmptySegment,
    /// path contains . or ..
    Dot,
    /// Path doesn't start with /
    IsRelative
}

impl AbsolutePath {
    pub fn root() -> Self {
        Self { data: String::new() }
    }
    pub fn make_cwd(cwd: &str) -> Result<Self, MakeCwdErr> {
        let cwd = cwd.strip_prefix('/').ok_or(MakeCwdErr::IsRelative)?;
        let cwd = cwd.strip_suffix('/').unwrap_or(cwd);
        if cwd.contains('\0') {
            return Err(MakeCwdErr::ContainsNullByte);
        }
        for segment in cwd.split('/') {
            match segment {
                "" => return Err(MakeCwdErr::ContainsEmptySegment),
                "." | ".." => return Err(MakeCwdErr::Dot),
                _ => {}
            }
        }

        Ok(Self { data: cwd.to_string() })
    }

    pub fn with_cwd(cwd: &Self, path: &str) -> Result<Self, PathWithCwdErr> {
        if path.contains('\0') {
            return Err(PathWithCwdErr::ContainsNullByte);
        }

        let mut stack = Vec::new();

        //prepend the cwd if path is relative
        if !path.starts_with('/') {
            stack.extend(cwd.into_iter());
        }

        //remove / prefix if applicable
        let path = path.strip_prefix('/').unwrap_or(path).strip_suffix('/').unwrap_or(path);

        for segment in path.split('/') {
            match segment {
                //blank string - error
                "" => {
                    return Err(PathWithCwdErr::ContainsEmptySegment)
                },
                //current directory - do nothing
                "." => {},
                //parent - go up one
                ".." => {
                    if stack.pop().is_none() {
                        return Err(PathWithCwdErr::TooManyDotDot)
                    }
                },
                //normal path segment
                x => {
                    stack.push(x);
                }
            }
        }

        Ok(AbsolutePath {
            data: stack.join("/"),
        })
    }

    pub fn is_subpath_of(&self, potential_parent: &Self) -> bool {
        return self.data.starts_with(&potential_parent.data)
    }
}

impl<'a> IntoIterator for &'a AbsolutePath {
    type Item = &'a str;

    type IntoIter = Split<'a, char>;

    fn into_iter(self) -> Self::IntoIter {
        self.data.split('/')
    }
}