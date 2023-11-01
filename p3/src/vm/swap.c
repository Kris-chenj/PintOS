#include "vm/swap.h"
#include "devices/block.h"

static struct block* swap_block;
struct bitmap* swap_bitmap;

/* swap init */ 
void init_swap(void)
{
    /* get the block */
    swap_block = block_get_role (BLOCK_SWAP);
    /* compute the size of swap */
    int size = block_size(swap_block);
    size = size / (PGSIZE / BLOCK_SECTOR_SIZE);
    /* create the bitmap */
    swap_bitmap = bitmap_create(size);

    /* initialize bitmap to 1 */
    bitmap_set_all(swap_bitmap, 1);
}

/* Swap a page in pm */
void swap_in (int location, void *page)
{
    for (int i = 0; i < 8; i++)
    {
        block_read(swap_block, i+location*8, i*BLOCK_SECTOR_SIZE+page);
    }

    bitmap_set(swap_bitmap, location, 1);
}

/* Swap a page out pm */
int swap_out (void *page)
{
    int index;
    index = bitmap_scan(swap_bitmap, 0, 1, 1);

    for (int i = 0; i < 8; i++)
    {
        block_write(swap_block, i+index*8,i*BLOCK_SECTOR_SIZE+page);
    }
    
    bitmap_set(swap_bitmap, index, 0);
    return index;
}