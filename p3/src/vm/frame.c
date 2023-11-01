#include "vm/frame.h"

/* Init function */
void init_frame()
{
    list_init(&frame_table);
}

/* Set value to the frame */
struct frame_entry *set_frame(void *kpage, void *upage, frame *fra)
{
    /* If NULL, malloc */
    if (fra == NULL){fra = malloc(sizeof(struct frame_entry));}
    /* Set value */
    fra->kpage = kpage;
    fra->upage = upage;
    fra->own = thread_current();
    /* Insert it */
    list_push_front (&frame_table, &fra->elem);

    return fra;
}

/* Get a frame to store upage content */
void *get_frame(void *upage)
{
    /* Get current thread */
    struct thread *cur = thread_current();
    /* Try to get a page directly */
    void *kpage = palloc_get_page(PAL_ZERO | PAL_USER);
    /* Latent victim frame */
    struct frame_entry *frame = NULL;
    /* Latent corresponding supplmental table */
    struct vm_entry *vmt;

    /* If no enough space */
    if(kpage == NULL)
    {
        /* Find a victimal frame */
        frame = list_entry(list_back(&frame_table),struct frame_entry, elem);
        kpage = frame -> kpage;
        /* Find corresponding supplmental table */
        vmt = s_lookup(&frame -> own -> vm,frame -> upage);

        /* Assess page original status */
        /* Original on swap space or on sysfile */
        if(vmt -> origin_status == 2 || vmt -> origin_status == 3)
        {
            /* Change supplmental table info */
            int swap_index = swap_out(frame->kpage);
            /* Update inde of swap page */
            vmt -> location_swap = swap_index;
            vmt -> kpage = NULL;
            /* Make both status to swap status */
            vmt -> page_status = 2;
            vmt -> origin_status = 2;
        }
        /* On memory mapped files */
        else if(vmt -> origin_status == 4)
        {
            /* Assess whether it's a dirty page */
            if (pagedir_is_dirty(frame -> own -> pagedir,vmt -> upage))
            {
                /* Write back */
                file_seek(vmt -> file,vmt -> file_offset);
                file_write(vmt -> file,vmt -> upage,vmt -> b_read);
            }
            vmt -> page_status = 4;
        }

        /* Clear kpage */
        pagedir_clear_page(frame -> own -> pagedir,frame -> upage);
        palloc_free_page(kpage);
        list_remove(&frame -> elem);

        /* Reset a new page */
        kpage = palloc_get_page(PAL_ZERO | PAL_USER);
    }

    /* If can't find it, reset it */
    if (s_lookup(&cur->vm, upage) == NULL){set_swap_to_frame(&cur->vm, upage);}
    /* Set entry to table */
    set_frame(kpage,upage,frame);
    
    return kpage;
}