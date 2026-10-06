pub type RLim = u64;

pub const RLIMIT_DATA: i32 = 0;

#[derive(Clone,Copy, Debug)]
#[repr(C)]
pub struct RLimit {
    pub rlim_cur: RLim,
    pub rlim_max: RLim
}

impl Default for RLimit {
    fn default() -> Self {
        Self { rlim_cur: RLim::MAX, rlim_max: RLim::MAX }
    }
}