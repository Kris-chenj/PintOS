#include "filesys/cache.h"

struct cache_entry
{
    /* Block representation */
    char buffer[BLOCK_SECTOR_SIZE];
    /* Dirty bit*/
    bool dirt;
    /* Assess whether touched this entry */
    bool touch;
    /* Second chance bit */
    int second_chance;
    /* Access lock */
    struct lock ele_lock;
    /* Entry size */
    int size;
    /* Record entry_id in cache */
    int id;
};

/* Clock hand */
int clock_hand;

/* Lock for cache */
static struct lock cache_lock;

/* Cache array */
static struct cache_entry cache[64];

/* Update hand */
int update_hand()
{
    int ans = clock_hand;
    clock_hand = (clock_hand + 1) % 64;
    return ans;
}

/* Initialization function */
void cache_init()
{
    lock_init(&cache_lock);
    for (int i = 0; i < 64; i ++)
    {
        cache[i].id = 0;
        cache[i].dirt = false;
        cache[i].touch = false;
        cache[i].second_chance = 0;
        lock_init (&cache[i].ele_lock);
        memset(cache[i].buffer,0,BLOCK_SECTOR_SIZE);   
    }
    /* Set a thread to flush cache to disk peroidly */
    thread_create("flush_all",PRI_DEFAULT,flushall,NULL);
}

/* Assess whether a entry has been initialized */
int assess_entry(int id)
{
    for (int i=0;i<64;i++)
    {
        /* If it has existed */
        if (cache[i].touch && cache[i].id == id){return i;}
    }
    /* If it hasn't existed */
    return -1;
}

/* Get free entry in cache */
int c_fetch()
{
    int ans;
    for(;;)
    {
        if(cache[clock_hand].second_chance == 0)
        {
            if(cache[clock_hand].dirt){block_write(fs_device,cache[clock_hand].id,cache[clock_hand].buffer);}
            ans = update_hand();
            break;
        }
        else
        {
            cache[clock_hand].second_chance = 0;
            update_hand();
        }
    }
    return ans;
}

/* Thread function */
void flushall()
{
    for(;;)
    {
        /* Flush with period 1s */
        timer_msleep(1000);
        flush_all();
    }
}

/* Flush all block to disk */
void flush_all()
{
    lock_acquire(&cache_lock);
    for (int i = 0; i < 64; i++) 
    {
        if (cache[i].touch && cache[i].dirt)
        {
            block_write(fs_device,cache[i].id,cache[i].buffer);
            cache[i].touch = false;
            cache[i].dirt = false;
        }
    }
    lock_release(&cache_lock);
}

/* Cache read and write function */
void c_read(int id,void *addre)
{
    lock_acquire(&cache_lock);
    int target = assess_entry(id);
    /* If it not existed, build a new entry and read to cache from disk and then read it out from cache to addre */
    if (target == -1)
    {
        /* Find a new entry and initialize it */
        int new_entry = c_fetch();
        cache[new_entry].id = id;
        cache[new_entry].touch = true;
        cache[new_entry].dirt == false;
        cache[new_entry].second_chance = 1;
        /* Read to cache from disk */
        block_read (fs_device,id,cache[new_entry].buffer);
        /* Read it out from cache to addre */
        memcpy(addre,cache[new_entry].buffer,BLOCK_SECTOR_SIZE);
    }
    /* If it has existed, read it out from cache directly */
    else{memcpy(addre,cache[target].buffer,BLOCK_SECTOR_SIZE);}
    lock_release(&cache_lock);
}
void c_write(int id,void *addre)
{
    lock_acquire(&cache_lock);
    int target = assess_entry(id);
    /* If it not existed, build a new entry and read to cache from disk and then read it out from cache to addre */
    if (target == -1)
    {
        /* Find a new entry and initialize it */
        int new_entry = c_fetch();
        cache[new_entry].id = id;
        cache[new_entry].touch = true;
        cache[new_entry].dirt == true;
        cache[new_entry].second_chance = 1;
        /* Write address's content to cache */
        memcpy(cache[new_entry].buffer, addre, BLOCK_SECTOR_SIZE);
    }
    else 
    {
        /* Update attributes */
        cache[target].dirt = true;
        cache[target].touch = true;
        cache[target].second_chance = 0;
        /* Write address's content to cache */
        memcpy(cache[target].buffer, addre, BLOCK_SECTOR_SIZE);
    }
    lock_release(&cache_lock);block_write(fs_device,id,addre);
}