#ifndef VM_SWAP_H
#define VM_SWAP_H


#include <stdio.h>
#include "threads/vaddr.h"
#include "lib/kernel/bitmap.h"

/* swap init */ 
void init_swap (void); 

/* Swap a page in pm */
void swap_in (int location, void *page);

/* Swap a page out pm */
int swap_out (void *page);


#endif