<center><font size=5>+-----------------------------------+</font></center>
<center><font size=5>|-------------- CS-130 --------------|</font></center>
<center><font size=5>|--PROJECT 3: VIRTUAL MEMORY--|</font></center>
<center><font size=5>|-------DESIGN-DOCUMENT-------|</font></center>
<center><font size=5>+-----------------------------------+</font></center>

### GROUP

> Fill in the names and email addresses of your group members.

Yifan Zhu <zhuyf1@shanghaitech.edu.cn>  
Zongze Li <lizz@shanghaitech.edu.cn>

### PRELIMINARIES

> If you have any preliminary comments on your submission, notes for the
> TAs, or extra credit, please give them here.

> Please cite any offline or online sources you consulted while
> preparing your submission, other than the Pintos documentation, course
> text, lecture notes, and course staff.

> * Page fault
>
>   https://www.cnblogs.com/shengs/p/13290500.html
>   
> * Memory management
>
>   https://blog.csdn.net/u014426028/article/details/120826515
>
> * Os Guide
>
>   https://static1.squarespace.com/static/5b18aa0955b02c1de94e4412/t/5b85fad2f950b7b16b7a2ed6/1535507195196/Pintos+Guide


<center><font size=5>PAGE TABLE MANAGEMENT</font></center>

### DATA STRUCTURES

> A1: Copy here the declaration of each new or changed `struct' or
> `struct' member, global or static variable, `typedef', or
> enumeration.  Identify the purpose of each in 25 words or less.

```
struct thread
{
    /* Store the information about vm entry set */
    struct hash vm;        
    /* Store the stack pointer of the thread */                
    void *esp;                            
}
```
frame.h:
```
/* A sumable frame table list corresponding to each process */
struct list frame_table;
/* Each frame entry's necessary variable */
typedef struct frame_entry{
    /* Virtual user address */
    void * upage;          
    /* Physical user address */          
    void * kpage;             
    /* Occupy */       
    struct thread * own;     
    /* Index list */        
    struct list_elem elem;           
} frame;
```
page.h:
```
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
```


### ALGORITHMS

> A2: In a few paragraphs, describe your code for accessing the data
> stored in the SPT about a given page.

At first, we use the hash table structure to maintain SPT table, of course, each SPT table correspond a process, in this way
we can just first find corresponding process and then take it's "vm" variable and then get it's "hash_elem" variable, then use 
our own `s_lookup` function:
  ```
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
  ```
so that we can access any SPT we want.

> A3: How does your code coordinate accessed and dirty bits between
> kernel and user virtual addresses that alias a single frame, or
> alternatively how do you avoid the issue?

Pintos automically divide user pool and kernel pool by different address value. And also by it's system complement, in pagedir.c
we can use 
  ```
  bool
  pagedir_is_dirty (uint32_t *pd, const void *vpage) 
  {
    uint32_t *pte = lookup_page (pd, vpage, false);
    return pte != NULL && (*pte & PTE_D) != 0;
  }
  ```
assess and coordinate dirty bits.

### SYNCHRONIZATION

> A4: When two user processes both need a new frame at the same time,
> how are races avoided?

In palloc.c, when two user processes both need a new frame at the same time, they both call this function:
  ```
  void *
  palloc_get_multiple (enum palloc_flags flags, size_t page_cnt)
  {
  struct pool *pool = flags & PAL_USER ? &user_pool : &kernel_pool;
  void *pages;
  size_t page_idx;

  if (page_cnt == 0)
    return NULL;

  lock_acquire (&pool->lock);
  page_idx = bitmap_scan_and_flip (pool->used_map, 0, page_cnt, false);
  lock_release (&pool->lock);

  if (page_idx != BITMAP_ERROR)
    pages = pool->base + PGSIZE * page_idx;
  else
    pages = NULL;

  if (pages != NULL) 
    {
      if (flags & PAL_ZERO)
        memset (pages, 0, PGSIZE * page_cnt);
    }
  else 
    {
      if (flags & PAL_ASSERT)
        PANIC ("palloc_get: out of pages");
    }

  return pages;
  }
  ```
it use a pool lock to avoid race.


### RATIONALE

> A5: Why did you choose the data structure(s) that you did for
> representing virtual-to-physical mappings?

We use hash table which has been completed by system to represent mapping.
The reason is:
  For a system, it's page table could be very big, using hash operations:
    ```
    /* Finds and returns an element equal to E in hash table H, or a
    null pointer if no equal element exists in the table. */
    struct hash_elem *
    hash_find (struct hash *h, struct hash_elem *e) 
    {
      return find_elem (h, find_bucket (h, e), e);
    }
    ```
  can get corresponding map quickly.


<center><font size=5>PAGING TO AND FROM DISK</font></center>

### DATA STRUCTURES

> B1: Copy here the declaration of each new or changed `struct' or
> `struct' member, global or static variable, `typedef', or
> enumeration.  Identify the purpose of each in 25 words or less.

The same as A1.

### ALGORITHMS

> B2: When a frame is required but none is free, some frame must be
> evicted.  Describe your code for choosing a frame to evict.

We use a function implement by system in pagedir.c:
  ```
  /* Returns true if the PTE for virtual page VPAGE in PD has been
   accessed recently, that is, between the time the PTE was
   installed and the last time it was cleared.  Returns false if
   PD contains no PTE for VPAGE. */
  bool
  pagedir_is_accessed (uint32_t *pd, const void *vpage) 
  {
    uint32_t *pte = lookup_page (pd, vpage, false);
    return pte != NULL && (*pte & PTE_A) != 0;
  }
  ```
  just iterate frame entry and assess through `pagedir_is_accessed()` if it's not been accessed recently, evict it.

> B3: When a process P obtains a frame that was previously used by a
> process Q, how do you adjust the page table (and any other data
> structures) to reflect the frame Q no longer has?

We implement a function in frame.c:
  ```
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
  ```
  firstly just push out from Q process's framethe of the corresponding frame entry, then use `set_frame()` function to push in a Q's frame entry.


> B4: Explain your heuristic for deciding whether a page fault for an
> invalid virtual address should cause the stack to be extended into
> the page that faulted.

We write a function in exception.c:
  ```
  /* Handler */
  void 
  handle_fault(void *fault_add,struct vm_entry *supt,void *esp)
  {
    struct thread * cur = thread_current();
    void *fp = pg_round_down(fault_add);
    supt = s_lookup(&cur->vm, fp);

    bool fault_on_stack = ((fault_add != NULL && fault_add < PHYS_BASE && fault_add >= lower_stack ) && (fault_add > esp || fault_add == esp-4 || fault_add == esp-32));

    /* No map */
    if (fault_on_stack && supt == NULL)
    {
        void *kpage = get_frame(fp);
        pagedir_set_page(cur->pagedir,fp,kpage,true);
        return;
    }

    /* If load success */
    if (s_load(&cur->vm,cur->pagedir,fp)){return;}
    /* If load fail, exit directly */
    else
    {
        thread_current()->exit_num = -1;thread_exit();
    }
  }
  ```
  In it, we use a bool variable "fault_on_stack" to assess whether cause the stack to be extended into the page that faulted.

### SYNCHRONIZATION

> B5: Explain the basics of your VM synchronization design.  In
> particular, explain how it prevents deadlock.  (Refer to the
> textbook for an explanation of the necessary conditions for
> deadlock.)

We use the pool lock and file lock to prevent data race,if two process want to change page or file at the same time, one will be blocked by the lock,
and actually, because map-bit function doesn't hold some resource, the deadlock will not exist for change these structure.  

> B6: A page fault in process P can cause another process Q's frame
> to be evicted.  How do you ensure that Q cannot access or modify
> the page during the eviction process?  How do you avoid a race
> between P evicting Q's frame and Q faulting the page back in?

If Q want to access or modify the page, it need usu syscall such like sysread or syswrite, and at that time it will assess whether it's being evicted(invalid),
if yes, just wait untill it's done. Also the same way for avoiding race.  

> B7: Suppose a page fault in process P causes a page to be read from
> the file system or swap.  How do you ensure that a second process Q
> cannot interfere by e.g. attempting to evict the frame while it is
> still being read in?

It's also assessed by syscall part, at the same time, if a page will be evicted is being changed now, just wait untill it's done.Also the same way for avoiding race.

> B8: Explain how you handle access to paged-out pages that occur
> during system calls.  Do you use page faults to bring in pages (as
> in user programs), or do you have a mechanism for "locking" frames
> into physical memory, or do you use some other design?  How do you
> gracefully handle attempted accesses to invalid virtual addresses?

In the syscall_handler(), we call the function check_pointer_validity(), which makes sure the validity of address, and we add more checking in the syscall read, this is the difference between this project and the last project. we check it whether be NULL, then check it whether be less than 0x8048000, it whether be user address. After that,  we check page whether in the physical frame, if false, then determine whether grow stack or load back, if all these operations fail, we return exit number -1, exit the thread.


### RATIONALE

> B9: A single lock for the whole VM system would make
> synchronization easy, but limit parallelism.  On the other hand,
> using many locks complicates synchronization and raises the
> possibility for deadlock but allows for high parallelism.  Explain
> where your design falls along this continuum and why you chose to
> design it this way.

In memory implement, system has pool lock initially and we add a oridinary file lock too.
We use lock every time we read or write a file or schedual page pool, it's ensure the manipulation with corectness however still limited about parallelism.


<center><font size=5>MEMORY MAPPED FILES</font></center>

### DATA STRUCTURES

> C1: Copy here the declaration of each new or changed `struct` or
> `struct` member, global or static variable, `typedef`, or
> enumeration.  Identify the purpose of each in 25 words or less.

thread.h:
```
/* record the mapping from memory to file */
struct mapping
{
   /* mapping id */
   int m_id;
   /* user virtual address */
   void *v_add;
   /* page number */
   int pnum;
   /* file pointer */
   struct file* file_p;
   struct list_elem elem;
};


```
The struct mapping records the information of mapping relationship, for example, the mapping ID, user virtual address and so on. 
```
......

static struct lock map_lock;

......
```
Also declares a lock for the mapping process.
```
struct thread
  {
      ......

      struct list map_list;

      ......
  }
```
The list of mapping.

### ALGORITHMS

> C2: Describe how memory mapped files integrate into your virtual
> memory subsystem.  Explain how the page fault and eviction
> processes differ between swap pages and other pages.

The thread has the list of mapping, in our mmap syscall, we check the validity of pointer first, then find the target file, judge whether the file is valid. After that, creating the mapping, insert hash to establish mapping relationship. In munmap syscall, we find the target mapping by traversing the map_list according m_id. Then delete the mapping, write back the content to the file, at last, free the resource and close the file.

> C3: Explain how you determine whether a new file mapping overlaps
> any existing segment.

We traverse all user page, call the fuction pagedir_get_page() and s_lookup() to judge that, if it occurs overlap, we set eax be -1 and return.

### RATIONALE

> C4: Mappings created with "mmap" have similar semantics to those of
> data demand-paged from executables, except that "mmap" mappings are
> written back to their original files, not to swap.  This implies
> that much of their implementation can be shared.  Explain why your
> implementation either does or does not share much of the code for
> the two situations.

They are indeed very similar, they can share to some extent, but in our implementation, we separately implemented their functions. For example, in mmap syscall, we separately updated the information of vm_entry. The reason why we do this is that separate implementation of the two can facilitate our understanding, our debugging work, and narrow the scope of errors.


<center><font size=5>SURVEY QUESTIONS</font></center>

Answering these questions is optional, but it will help us improve the
course in future quarters.  Feel free to tell us anything you
want--these questions are just to spur your thoughts.  You may also
choose to respond anonymously in the course evaluations at the end of
the quarter.

> In your opinion, was this assignment, or any one of the three problems
> in it, too easy or too hard?  Did it take too long or too little time?

> Did you find that working on a particular part of the assignment gave
> you greater insight into some aspect of OS design?

> Is there some particular fact or hint we should give students in
> future quarters to help them solve the problems?  Conversely, did you
> find any of our guidance to be misleading?

> Do you have any suggestions for the TAs to more effectively assist
> students, either for future quarters or the remaining projects?

> Any other comments?
