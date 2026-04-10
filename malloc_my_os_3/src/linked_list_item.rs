use core::ptr::null_mut;
use core::marker::PhantomData;
use core::ptr;

use memory_size::MemorySize;

use crate::debugging::{print_hex, print_str};

pub(crate) const LINKED_LIST_ITEM_SIZE: MemorySize = MemorySize::from_bytes(size_of::<LinkedListItem>() as u64);
const LL_CANARY: u32 = 0xDEADBEEF;

/// Must be aligned to 16 byte, so that all mallocs are
/// 
/// cbindgen:no-export
#[derive(Clone)]
#[repr(C, align(16))]
pub(crate) struct LinkedListItem {
    next: *mut LinkedListItem,
    is_free: bool,
    canary: u32,
}
pub(crate) struct LinkedListCursor<'a> {
    /// Pointer to the `next` field that points at the next item
    next_pointer: *mut *mut LinkedListItem,
    /// Takes ownership of the whole linked list whilst iterating
    _marker: PhantomData<&'a mut LinkedListItem>
}

impl LinkedListItem {
    /// Generates a `LinkedListItem`, that is free and represents all the memory after `self`, up to `next`, or the end of the heap if `next` is null
    pub fn new(next: *mut LinkedListItem, output: *mut Self) {
        let result = Self { next, canary: LL_CANARY, is_free: true};
        unsafe{(*output) = result}
    }

    pub fn destroy(&mut self) {
        self.canary = 0xABABABAB;
        self.next = 0xCAFECAFECAFECAFE as *mut LinkedListItem;
    }

    pub fn is_free(&self) -> bool{
        self.assert_invariant();
        self.is_free
    }
    pub fn mark_as(&mut self, is_free: bool) {
        self.assert_invariant();
        self.is_free = is_free;
    }

    pub fn region_start(&self) -> *const () {
        self.assert_invariant();
        unsafe {(self as *const LinkedListItem).add(1) as *const ()}
    }

    pub fn region_size(&self, heap_end: *const ()) -> MemorySize {
        self.assert_invariant();
        //start inclusive
        let region_start_ptr = self.region_start();
        //end exclusive
        let region_end_ptr = if self.next.is_null() {heap_end} else {self.next as *const ()};

        let region_size_bytes = unsafe {region_end_ptr.byte_offset_from(region_start_ptr)};
        MemorySize::from_bytes(region_size_bytes.try_into().unwrap())
    }

    /// Initialises and inserts a blank segment after self at address `to_insert_addr`
    /// 
    /// Also updates `last_item_ptr` if required
    pub fn insert_after(&mut self, to_insert_addr: *mut (), last_item_ptr: &mut *mut Self) {
        // let's call the nodes self -> to_insert -> after
        let after_addr = self.get_next();
        let to_insert_addr = to_insert_addr as *mut Self;
        assert!(!to_insert_addr.is_null());
        assert!(to_insert_addr.is_aligned());
        //inserted item must be after me, either in my region (as I am being split), or at the start of a newly brk'd region
        assert!(self.region_start() <= to_insert_addr as *const ());

        //insert
        self.next = to_insert_addr;
        if after_addr.is_null() {
            //no item after, just create on end, and link
            Self::new(null_mut(), to_insert_addr);
            //as I added a new last item, update
            *last_item_ptr = to_insert_addr;
        } else {
            Self::new(after_addr, to_insert_addr);//make the item to be inserted
            //next item must be after inserted item
            assert!(unsafe {(*to_insert_addr).region_start()} <= after_addr as *const ());
        }
    }

    /// Removes the item after myself, if both myself and next are free
    /// 
    /// ### Returns
    /// True if a merge was successful
    pub fn try_merge_after(&mut self, last_item_ptr: &mut *mut Self) -> bool {
        //find the next item after self
        let to_destroy_addr = self.get_next();
        let to_destroy = match unsafe{to_destroy_addr.as_mut()} {
            None => return false,//I am last item, can't possible remove after
            Some(x) => x
        };
        if !self.is_free() || !to_destroy.is_free() {return false;}//can only merge free regions
        
        //make my pointer skip the deleted item
        let after_removed_addr = to_destroy.get_next();
        self.next = after_removed_addr;

        //if I removed the last item, update
        if ptr::eq(to_destroy, *last_item_ptr) {
            *last_item_ptr = self as *mut LinkedListItem;
        }

        //destroy the item to be removed
        to_destroy.destroy();

        true
    }

    pub fn get_next(&self) -> *mut Self {
        self.assert_invariant();
        assert!(self.next.is_aligned());
        self.next
    }

    pub fn assert_invariant(&self) {
        if self.canary != LL_CANARY {
            print_str("ERROR WITH RUST MALLOC CANARY!\ncanary: \0");
            print_hex(self.canary.into());
            panic!()
        }
    }
}

impl<'a> LinkedListCursor<'a> {
    pub(crate) fn new(head: &'a mut *mut LinkedListItem) -> LinkedListCursor<'a> {
        LinkedListCursor { next_pointer: head, _marker: PhantomData }
    }

    pub(crate) fn move_next(&mut self) -> Option<&mut LinkedListItem> {

        unsafe {
            let node = (*self.next_pointer).as_mut()?;
            self.next_pointer = &mut node.next;
            Some(node)
        }
    }
}