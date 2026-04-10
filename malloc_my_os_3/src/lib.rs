// #![no_std]
#![cfg_attr(not(test), no_std)]
#![crate_type = "staticlib"]


mod linked_list_item;
/// cbindgen:ignore
mod debugging;
// mod test;

use core::panic::PanicInfo;
use core::ptr::null_mut;
use memory_size::MemorySize;
use crate::{debugging::{print_int, print_str}, linked_list_item::{LINKED_LIST_ITEM_SIZE, LinkedListCursor, LinkedListItem}};

/// params (extra_to_allocate) -> extra that was allocated
/// 
/// input doesn't have to be aligned, and the output will either be 0 or >= the input
type ExpandHeap = extern "C" fn(*mut (), u64) -> u64;
const MALLOC_ALIGNMENT: MemorySize = MemorySize::from_bytes(16);

#[repr(C)]
pub struct MemoryAllocator {
    /// Also the start of the heap
    pub(crate) head: *mut LinkedListItem,
    pub(crate) tail: *mut LinkedListItem,
    /// end of heap, exclusive
    heap_end: *mut (),

    allocate_more_memory: ExpandHeap,
}

impl MemoryAllocator {
    #[unsafe(no_mangle)]
    pub extern "C" fn init_memory_allocator(
        allocate_more_memory: ExpandHeap, 
        heap_start: *mut (),
        output: *mut Self) -> i32
    {
        let allocated_initial_memory = MemorySize::from_bytes(allocate_more_memory(heap_start, LINKED_LIST_ITEM_SIZE.size_bytes()));
        let heap_end = unsafe {heap_start.byte_add(allocated_initial_memory.size_bytes() as usize)};
        if allocated_initial_memory < LINKED_LIST_ITEM_SIZE {return -1;}

        let first_ll_item_ptr = heap_start as *mut LinkedListItem;
        LinkedListItem::new(null_mut(), first_ll_item_ptr);

        unsafe {
            //set up the output, to point at the memory and linked list item
            (*output) = Self {
                heap_end,
                allocate_more_memory: allocate_more_memory,
                head: first_ll_item_ptr,
                tail: first_ll_item_ptr,
            };
        }

        return 0;
    }

    #[unsafe(no_mangle)]
    pub extern "C" fn allocate(&mut self, size: u64) -> *mut () {
        if size == 0 {
            return null_mut();
        }
        let size = MemorySize::from_bytes(size).align_up(MALLOC_ALIGNMENT);//sometimes give more RAM than needed, to maintain alignment
        let reserved_metadata_ptr = self.find_or_make_free(size);
        //mark area as used
        unsafe {(*reserved_metadata_ptr).mark_as(false);};
        //get size and data pointer
        let reserved_size = unsafe{(*reserved_metadata_ptr).region_size(self.heap_end)};
        let reserved_data = unsafe {(*reserved_metadata_ptr).region_start()} as *mut u8;

        if reserved_size >= size + LINKED_LIST_ITEM_SIZE {
            //enough memory for the allocated region, plus a new free region after
            unsafe {
                let next_region_ptr = reserved_data.byte_add(size.size_bytes() as usize) as *mut ();
                (*reserved_metadata_ptr).insert_after(next_region_ptr, &mut self.tail);
            }
        } else {
            //only just enough memory for the allocation
        }

        return reserved_data as *mut ();
    }

    #[unsafe(no_mangle)]
    pub extern "C" fn deallocate(&mut self, ptr: *mut ()) {
        if ptr.is_null() {return;}
        let ptr_to_ll = unsafe {(ptr as *mut LinkedListItem).sub(1)};
        unsafe {
            (*ptr_to_ll).assert_invariant();
            (*ptr_to_ll).mark_as(true);
        }
    }

    /// Finds a free segment at least as big as `size`, or expands the heap to make a segment
    fn find_or_make_free(&mut self, size: MemorySize) -> *mut LinkedListItem {
        let mut cursor = LinkedListCursor::new(&mut self.head);
        while let Some(curr_ptr) = cursor.move_next() {
            //coalesce free regions
            while curr_ptr.try_merge_after(&mut self.tail) {}

            curr_ptr.assert_invariant();

            if curr_ptr.region_size(self.heap_end) >= size && curr_ptr.is_free() {
                return curr_ptr;//found a big enough free region
            }

        }

        //broke out of loop as didn't find region
        unsafe {
            let old_last_item_ptr = self.tail;
            (*old_last_item_ptr).assert_invariant();
            let last_size = if (*old_last_item_ptr).is_free() {(*old_last_item_ptr).region_size(self.heap_end)} else {MemorySize::new()};//hope that there is some free at the end

            let remaining_to_request = size - last_size;//only allocate the extra needed
            self.expand_heap(remaining_to_request).unwrap();//panic on OOM
            (*old_last_item_ptr).try_merge_after(&mut self.tail);
        }
        assert!(unsafe {
            (*self.tail).is_free() && (*self.tail).region_size(self.heap_end) >= size
        });

        self.tail//expanded heap, so last item is the one to use
    }

    fn expand_heap(&mut self, requested_bytes: MemorySize) -> Result<(), ()> {
        let actual_needed_bytes = requested_bytes + LINKED_LIST_ITEM_SIZE;//since an extra linked list item has to be allocated

        //try and allocate the memory
        let allocated = MemorySize::from_bytes((self.allocate_more_memory)(self.heap_end, actual_needed_bytes.size_bytes()));
        let old_heap_end = self.heap_end;
        self.heap_end = unsafe {self.heap_end.byte_add(allocated.size_bytes() as usize)};
        if allocated < actual_needed_bytes {return Err(());}//failed

        unsafe {
            (*self.tail).insert_after(old_heap_end, &mut self.tail);
        }

        Ok(())
    }
}



// unsafe extern "C" {fn crash() -> !;}
// #[cfg(not(test))]
#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    print_str("rust memory allocator panic!\n\0");

    let mut buf = [0u8; 100];

    if let Some(x) = info.location() {
        for (i, c) in x.file().bytes().enumerate() {
            buf[i] = c;
        }
        buf[x.file().len()] = 0;//horrible
        print_int(x.line().into());
        print_str(unsafe{&str::from_utf8_unchecked(&buf)});
        
    }

    loop {}
    // unsafe {crash()}
}
