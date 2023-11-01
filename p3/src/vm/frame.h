#ifndef VM_FRAME_H
#define VM_FRAME_H


#include <list.h>
#include <stdint.h>
#include "vm/page.h"
#include "threads/palloc.h"
#include "threads/thread.h"

struct list frame_table;             /* A sumable frame table list */

typedef struct frame_entry{
    void * upage;                    /* Virtual user address */
    void * kpage;                    /* Physical user address */
    struct thread * own;             /* Occupy */
    struct list_elem elem;           /* Index list */
} frame;

void init_frame();                   /* Init function */
void *get_frame(void *upage);        /* Get a frame to store upage content */
struct frame_entry *set_frame(void *kpage, void *upage, struct frame_entry *frame);


#endif