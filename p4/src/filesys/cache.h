#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H
#include <list.h>
#include <string.h>
#include "devices/timer.h"
#include "devices/block.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/filesys.h"

/* For read ahead function */
struct read_ahead
{
    int id;
    struct list_elem elem; 
    struct list read_ahead_list;
    struct lock read_ahead_lock;
};

/* Update hand */
int update_hand();

/* Initialization function */
void cache_init();

/* Get free entry in cache */
int c_fetch();

/* Assess whether a entry has been initialized */
int assess_entry(int id);

/* Flush all block to disk */
void flush_all();
/* Thread function */
void flushall();

/* Cache read and write function */
void c_read(int id,void *addre);
void c_write(int id,void *addre);

#endif