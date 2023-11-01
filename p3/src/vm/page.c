#include "vm/page.h"
#include "vm/frame.h"

/* Supplmental table entry hash function */
unsigned
s_hash (const struct hash_elem *e, void *aux)
{
  const struct vm_entry *dummy = hash_entry (e, struct vm_entry, hash_elem);
  return hash_bytes (&dummy->upage, sizeof(dummy->upage));
}

/* Returns true if a front of b */
bool
s_compare (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  return (hash_entry(a, struct vm_entry, hash_elem) -> upage < hash_entry (b, struct vm_entry, hash_elem) -> upage);
}

/* Set a new element to supplmental table */
struct vm_entry *set_swap_to_frame(struct hash *t, void *upage)
{
    struct vm_entry * ans = malloc(sizeof(struct vm_entry));
    struct hash_elem *elem;
  
    ans->page_status = 1;      /* It's on frame */
    ans->origin_status = 2;    /* Origin on swap disk */
    ans->upage = upage;
    elem = hash_insert(t, &ans->hash_elem);
    if (elem != NULL){return NULL;}

    return ans;
}

/* Look up a element in the supplmental table */
struct vm_entry *s_lookup(struct hash *t, void *address)
{
    /* Create a dummy to show the new entry */
    struct vm_entry dummy;
    dummy.upage = address;

    /* Find elem in the table */
    struct hash_elem *elem = hash_find(t, &dummy.hash_elem);

    if (elem == NULL){return NULL;}
    return hash_entry(elem, struct vm_entry, hash_elem);
}

/* Load a kind of page to pm */
bool s_load(struct hash *t, uint32_t *pd, void *upage)
{
    /* Find position of upage in spt */
    struct vm_entry *target = s_lookup(t,upage);
    if(target == NULL){return false;}

    /* Set a kernel virtual page map to upage */
    void *kpage = get_frame(upage);

    /* Assess status of page */
    bool write = true;

    /* Marked by 0 */
    if(target->page_status == 0){memset(kpage,0,PGSIZE);}
    /* On swap space */
    else if(target->page_status == 2){swap_in(target -> location_swap, kpage);}
    /* On file system or On memory mapped files */
    else if(target->page_status == 3 || target->page_status == 4)
    {
        /* Locate file */
        file_seek(target -> file,target -> file_offset);
        off_t read = file_read(target -> file,kpage,target -> b_read);
        memset(kpage + read,0,PGSIZE - (target -> b_read));
    }

    /* Set whether writable when On file system*/
    if(target->page_status == 3){write = target -> write;}
    

    bool whether_set = pagedir_set_page(pd,upage,kpage,write);
    if(!whether_set){return false;}

    /* Set the parameter to vm_entry */
    target -> kpage = kpage;
    target -> page_status = 1;
    return true;
}