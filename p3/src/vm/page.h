#ifndef VM_PAGE_H
#define VM_PAGE_H


#include <hash.h>
#include <stdint.h>
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "filesys/off_t.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"




struct vm_entry
{
    /* User virtual address */
    void *upage;

    /* Kernel address could map to physical address */
    void *kpage;

    /* Whether able to write */
    bool write;

    /* location about swap position */
    int location_swap;

    /* Page status */
    /* 0 means marked with zero, 1 means on frame ,2 means on swap disk, 3 means on filesystem, 4 means on mmp */
    int page_status;

    /* Original page status */
    /* 0 means marked with zero, 1 means on frame ,2 means on swap disk, 3 means on filesystem, 4 means on mmp */
    int origin_status;

    /* Belongs to this thread */
    struct thread *own;

    /* Filesys */
    struct file *file;
    
    /* File page offset */
    off_t file_offset;

    /* If in a sysfile/mmf , record corresponding byte it should read */
    off_t b_read;

    /* Hash list index */
    struct hash_elem hash_elem;
};


/* Supplmental table entry hash function */
unsigned s_hash(const struct hash_elem *e, void *aux);

/* Supplmental table entry hash compare function */
bool s_compare 
    (const struct hash_elem *a, const struct hash_elem *b, void *aux);

/* Set a new element to supplmental table */
struct vm_entry *set_swap_to_frame(struct hash *t, void *upage);

/* Look up a element in the supplmental table */
struct vm_entry *s_lookup(struct hash *t, void *address);

/* Load a kind of page to pm */
bool s_load(struct hash *t, uint32_t *pd, void *upage);



#endif