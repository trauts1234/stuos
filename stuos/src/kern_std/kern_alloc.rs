use core::alloc::GlobalAlloc;

unsafe extern "C" {
    fn malloc(size: usize) -> *mut ();
    fn free(ptr: *mut ());
}

#[global_allocator]
static GLOBAL: MyAllocator = MyAllocator;

struct MyAllocator;

unsafe impl GlobalAlloc for MyAllocator {
    unsafe fn alloc(&self, layout: core::alloc::Layout) -> *mut u8 {
        assert!(layout.align() <= 16);
        unsafe {malloc(layout.size()) as *mut u8}
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: core::alloc::Layout) {
        assert!(layout.align() <= 16);
        unsafe {free(ptr as *mut())}
    }
}