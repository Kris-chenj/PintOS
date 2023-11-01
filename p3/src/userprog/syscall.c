#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "stdint.h"
#include "list.h"
#include "vm/page.h"

/* 1 some functions */
static void syscall_handler (struct intr_frame *);



static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}



/* check pointer whether be valid */
void*
check_pointer_validity(void* pointer,int size)
{
    unsigned char* check_position = (unsigned char*) pointer;
  if (pointer == NULL)
  {
    thread_current()->exit_num=-1;
    thread_exit();
  }
  
  if (pointer >= PHYS_BASE)
  {
    thread_current()->exit_num=-1;
    thread_exit();
  }

  if (pagedir_get_page (thread_current()->pagedir, pointer) == NULL)
  {
    thread_current()->exit_num=-1;
    thread_exit();
  }

  for (int i = 0; i < size; i++)
  {
    if (!is_user_vaddr((int*)pointer+i))
    {
      thread_current()->exit_num = -1;
      thread_exit();
    }
    if (!pagedir_get_page(thread_current()->pagedir, (int*)pointer+i))
    {
      thread_current()->exit_num = -1;
      thread_exit();
    }
  }
}

/* check pointer whether be valid */
void*
check_pointer_validity_void(void* pointer,int size)
{
    unsigned char* check_position = (unsigned char*) pointer;
  if (pointer == NULL)
  {
    thread_current()->exit_num=-1;
    thread_exit();
  }
  
  if (pointer >= PHYS_BASE)
  {
    thread_current()->exit_num=-1;
    thread_exit();
  }

  if (pagedir_get_page (thread_current()->pagedir, pointer) == NULL)
  {
    thread_current()->exit_num=-1;
    thread_exit();
  }

  for (int i = 0; i < size; i++)
  {
    if (!is_user_vaddr(pointer+i))
    {
      thread_current()->exit_num = -1;
      thread_exit();
    }
    if (!pagedir_get_page(thread_current()->pagedir, pointer+i))
    {
      thread_current()->exit_num = -1;
      thread_exit();
    }
  }
}

/* check pointer whether be valid */
void*
check_pointer_validity_char(void* pointer)
{
    unsigned char* check_position = (unsigned char*) pointer;
  if (pointer == NULL)
  {
    thread_current()->exit_num=-1;
    thread_exit();
  }
  
  if (pointer >= PHYS_BASE)
  {
    thread_current()->exit_num=-1;
    thread_exit();
  }

  if (pagedir_get_page (thread_current()->pagedir, pointer) == NULL)
  {
    thread_current()->exit_num=-1;
    thread_exit();
  }

  for (int i = 0; ; i++)
  {
    if (!is_user_vaddr((char*)pointer+i))
    {
      thread_current()->exit_num = -1;
      thread_exit();
    }
    if (!pagedir_get_page(thread_current()->pagedir, (char*)pointer+i))
    {
      thread_current()->exit_num = -1;
      thread_exit();
    }
    if (*((char*)pointer+i) == '\0'){
      break;
    }    
  }
}

/* system call halt */
void 
syshalt(struct intr_frame* f)
{
  shutdown_power_off();
}
/* system call exit */
void 
sysexit (struct intr_frame* f)
{
  /* get the pointer */
  int* p = f->esp;
  p++;
  thread_current()->exit_num = *p;
  thread_exit();
}
/* system call exec */
void 
sysexec (struct intr_frame* f)
{
  /* get the pointer */
  int* p=(int*)f->esp;
  p++;
  check_pointer_validity_char((char*)*p);
    f->eax = process_execute((char*)*p);
}
/* system call wait */
void 
syswait (struct intr_frame* f)
{
  /* get the pointer */
  int* p = f->esp;
  p++;
  f->eax = process_wait(*p);
}
/* system call write */
void 
syswrite (struct intr_frame* f)
{
  /* get the pointer */
  int* p = (int*)f->esp;
  p++;
  int fd = *p;
  p++;
  void* buffer = (void*)(*p);
  p++;
  int size = *p;


  check_pointer_validity_void(buffer,size);
  
  if(fd == 0)
  {
    f->eax = -1;
  }
  else{
    if(fd == 1)
    {
      f->eax=size;
      putbuf(buffer, size);
    }
    else{
      // printf("-------------------------------%d-------------------------------\n",&fd);
      struct ofile* target_file = NULL;
      struct list_elem * i=list_begin (&thread_current ()->files);
      if (!list_empty (&thread_current ()->files)){
        while (1)
        {
          if (i == list_end (&thread_current ()->files))
          {
            break;
          }
          struct ofile * ff = list_entry (i, struct ofile, elem);
          if(ff->fd == fd)
          {
            target_file = ff;
            if(target_file == NULL){
              f->eax = -1;
            }else{
              lock_acquire (&file_lock);
              f->eax = file_write (target_file->file_pointer, buffer, size);
              lock_release (&file_lock);
            } 
            break;
          }
          i=list_next(i);
        }
      }
    }
  } 

}
/* system call create */
void 
syscreate(struct intr_frame* f)
{
  /* get the pointer */
  int* p = f->esp;
  p++;
  char* name = (char*)(*p);
  p++;
  off_t initial_size = *p;
  check_pointer_validity_char(name);
  lock_acquire (&file_lock);
  f->eax = filesys_create (name, initial_size);
  lock_release (&file_lock);

}
/* system call remove */
void 
sysremove(struct intr_frame* f)
{
  /* get the pointer */
  int* p = f->esp;
  p++;
  char *name = (char *)(*p);
  check_pointer_validity_char(name);
  lock_acquire (&file_lock);
  f->eax = filesys_remove(name);
  lock_release (&file_lock);
}
/* system call open */
void 
sysopen (struct intr_frame* f)
{
  /* get the pointer */
  int* p = f->esp;
  p++;
  char* name = (char*)(*p);
  check_pointer_validity_char(name);

  lock_acquire (&file_lock);
  struct file* my_openfile = filesys_open(name);
  lock_release (&file_lock);

  if (my_openfile != NULL){
    struct ofile* f_node = malloc (sizeof (struct ofile));
    f_node -> file_pointer = my_openfile;
    f_node -> fd = thread_current()->file_num;
    thread_current()->file_num++;
    list_push_back(&thread_current()->files, &f_node -> elem);
    f->eax= f_node -> fd;
    return;
  }
  f->eax = -1;
  return;
}
/* system call filesize */
void 
sysfilesize (struct intr_frame* f){
  /* get the pointer */
  int* p = (int*)f->esp;
  p++;
  int fd = *p;

  struct ofile* target_file = NULL;

  struct list_elem * i = list_begin (&thread_current()->files);
  if (!list_empty (&thread_current()->files)){
    while (1)
    {
      if (i == list_end (&thread_current()->files))
      {
        break;
      }
      struct ofile * ff = list_entry (i, struct ofile, elem);
      if(ff->fd == fd)
      {
        target_file = ff;
        if(target_file != NULL)
        {
          lock_acquire (&file_lock);
          f->eax = file_length(target_file->file_pointer);
          lock_release (&file_lock);
          return;
        }
      }
      i=list_next(i);
    }
    
  }
  f->eax = -1;
  return;
}


/* system call read */
void 
sysread (struct intr_frame* f)
{
  /* get the pointer */
  int* p = (int*)f->esp;
  p++;
  int fd = *p;
  p++;
  void* buffer = (void*)(*p);
  p++;
  int size = *p;

  /* check validity */
  for (unsigned i=0; i<size; i++)
  {
    if (buffer+i == NULL)
    {
      thread_current()->exit_num=-1;
      thread_exit();
    }
    if (buffer+i < 0x8048000){
      thread_current()->exit_num=-1;
      thread_exit();
    }
    if (!is_user_vaddr(buffer+i))
    {
      thread_current()->exit_num=-1;
      thread_exit();
    }
    
    
    
    bool is_map = pagedir_get_page (thread_current ()->pagedir, buffer+i);
    if (!is_map){
      if (buffer+i > f->esp){
        void *upage = pg_round_down (buffer+i);
        if (s_lookup (&thread_current ()->vm, upage) == NULL){
          void *kpage = get_frame (upage);
          bool success = pagedir_set_page (thread_current ()->pagedir, upage, kpage, true);
          if (!success)
          {
            thread_current()->exit_num=-1;
            thread_exit();
          }
        }
      }
    }
  }


  for (unsigned i=0; i<size; i++)
  {
    int* pte = lookup_page (thread_current()->pagedir, buffer+i, false);
    if ((*pte&0x2)!=2){
      thread_current()->exit_num=-1;
      thread_exit();
    }
  }


  if(fd == 0)
  {
    for (int i = 0; i < size; i ++)
    {
      ((char*)buffer)[i] = input_getc();
    }
    f->eax = size;
    return;
  }
  else if(fd == 1)
  {
    f->eax = -1;
    return;    
  }
  else{
    struct ofile* target_file = NULL;

    struct list_elem * i = list_begin (&thread_current()->files);
    if (!list_empty (&thread_current()->files)){
      while (1)
      {
        if (i == list_end (&thread_current()->files))
        {
          break;
        }
        struct ofile * ff = list_entry (i, struct ofile, elem);
        if(ff->fd == fd)
        {
          target_file = ff;
          if(target_file != NULL)
          {
            lock_acquire (&file_lock);
            f->eax = file_read (target_file->file_pointer, buffer, size);
            lock_release (&file_lock);
            return;
          }
        }
        i=list_next(i);
      }
    } 
  }
  
}

/* system call seek */
void 
sysseek(struct intr_frame* f)
{
  /* get the pointer */
  int* p = (int*)f->esp;
  p++;
  int fd = *p;
  p++;
  unsigned position = *p;
  
  struct ofile* target_file = NULL;

  struct list_elem * i = list_begin (&thread_current()->files);
  if (!list_empty (&thread_current()->files)){
    while (1)
    {
      if (i == list_end (&thread_current()->files))
      {
        break;
      }
      struct ofile * ff = list_entry (i, struct ofile, elem);
        if(ff->fd == fd)
          {
            target_file = ff;
            if (target_file){
              lock_acquire (&file_lock);
              file_seek(target_file->file_pointer, position);
              lock_release (&file_lock);
            }else{
              thread_current()->exit_num = -1;
              thread_exit();
            }  
          }
      i=list_next(i);
    }
  }
}

/* system call tell */
void 
systell (struct intr_frame* f)
{
  /* get the pointer */
  int* p = (int*)f->esp;
  p++;
  int fd = *p;

  struct ofile* target_file = NULL;

  struct list_elem * i = list_begin (&thread_current()->files);
  if (!list_empty (&thread_current()->files)){
    while (1)
    {
      if (i == list_end (&thread_current()->files))
      {
        break;
      }
      struct ofile * ff = list_entry (i, struct ofile, elem);
      if(ff->fd == fd)
      {
        target_file = ff;
        if (target_file){
          lock_acquire (&file_lock);
          f->eax = file_tell(target_file->file_pointer);
          lock_release (&file_lock);
          return;
        }
      }
      i=list_next(i);
    }
  }
  f->eax = -1;
  return;
}

/* system call close */
void 
sysclose (struct intr_frame* f)
{
  /* get the pointer */
  int* p = (int*)f->esp;
  p++;
  int fd = *p;

  struct ofile* target_file = NULL;

  struct list_elem * i = list_begin (&thread_current ()->files);
  if (!list_empty (&thread_current ()->files)){
    while (1)
    {
      if (i == list_end (&thread_current ()->files))
      {
        break;
      }
      struct ofile * ff = list_entry (i, struct ofile, elem);
      if(ff->fd == fd)
        {
          target_file = ff;
          if (target_file){
            lock_acquire (&file_lock);
            file_close (target_file -> file_pointer);
            list_remove (&target_file->elem);
            free (target_file);
            lock_release (&file_lock);
            return;
          }
        }
      i=list_next(i);
    }
  }
  return;
}

/* system call mmap */
void 
sysmmap (struct intr_frame* f)
{
  /* get the pointer */
  int* p = (int*)f->esp;
  p++;
  int fd = *p;
  p++;
  void* add = (void*)(*p);

  if (fd==0){
    f->eax = -1;
    return;
  }
  if (fd==1)
  {
    f->eax = -1;
    return;
  }
  if (pg_ofs(add))
  {
    f->eax = -1;
    return;
  }
  if (!add)
  {
    f->eax = -1;
    return;
  }
  

  struct file* ff = NULL;
  struct ofile* target_file;
  
  target_file = NULL;

  struct list_elem * i = list_begin (&thread_current()->files);
  if (!(list_empty (&thread_current()->files))){
    while (1)
    {
      if (i == list_end (&thread_current()->files))
      {
        break;
      }
      struct ofile * tf = list_entry (i, struct ofile, elem);
      if(tf->fd == fd)
        {
          target_file = tf;
          if ((!target_file)||(file_length (target_file->file_pointer)<=0)){
            f->eax = -1;
            return;
          }  
          else
          {
            ff = file_reopen(target_file->file_pointer);
          }
        }

      i = list_next (i);
    }
    
  }

  int the_offest = 0;
  int file_len = file_length (target_file->file_pointer);
  while (1)
  {
    if (the_offest >= file_len)
    {
      break;
    }
    void *temp_upage = pg_round_down(add+the_offest);
    if ((pagedir_get_page(thread_current()->pagedir, temp_upage)))
    {
      f->eax = -1;
      return;
    }
    if (s_lookup(&thread_current()->vm, temp_upage))
    {
      f->eax = -1;
      return;
    }
    the_offest = the_offest+PGSIZE;
  }

  lock_acquire (&file_lock); 
  the_offest = 0;
  while (1)
  {
    if (the_offest >= file_len)
    {
      break;
    }
    void *new_upage = add + the_offest;
    struct vm_entry *target = malloc(sizeof(struct vm_entry));
    struct hash_elem *dummy;

    int read_b = 0;
    if (the_offest + PGSIZE >= file_len)
    {
      read_b = file_len - the_offest;
    }else{
      read_b = PGSIZE;
    }
    
    target->page_status = 4;           
    target->origin_status = 4;         
    target->upage = new_upage;
    target->kpage = NULL;
    target->file = ff;
    target->file_offset = the_offest;
    target->b_read = read_b;
    target->write = true;

    hash_insert (&thread_current()->vm, &target->hash_elem);
    
    the_offest = the_offest+PGSIZE;
  }

  if (ff)
  {
    f->eax = create_map(add, ff, file_length(target_file->file_pointer));
  }else{
    f->eax = -1;
  }
  lock_release (&file_lock);
  return;
}

/* system call munmap */
void 
sysmunmap (struct intr_frame* f)
{
  /* get the pointer */
  int* p = (int*)f->esp;
  p++;
  int mid = *p;

  lock_acquire (&file_lock);
  
  if (list_empty (&thread_current()->map_list)==1){
    thread_current()->exit_num = -1;
    thread_exit();
  }

  struct list_elem* delete_elem;
  struct list_elem* i = list_begin (&thread_current()->map_list);
  while (1)
  {
    if (i == list_end (&thread_current()->map_list))
    {
      break;
    }
    
    struct mapping* temp_map = list_entry(i, struct mapping, elem);
    if(temp_map->m_id == mid)
    {
      delete_elem = list_remove(&temp_map->elem);
      if (delete_elem)
      {
        struct mapping* be_freed = list_entry(i, struct mapping, elem);

        int temp_offset = 0;
        while (1)
        {
          if (temp_offset == be_freed->pnum)
          {
            break;
          }
          struct vm_entry spte;
          spte.upage = temp_offset*PGSIZE + be_freed->v_add;
          struct hash_elem* h_e = hash_delete (&thread_current()->vm, &spte.hash_elem);
          if (h_e)
          {
            struct vm_entry* spte_p = hash_entry(h_e, struct vm_entry, hash_elem);
            if (pagedir_is_dirty(thread_current()->pagedir, spte_p->upage))
            {
              if (spte_p->page_status == 1)
              {
                file_seek(spte_p->file, spte_p->file_offset);
                file_write(spte_p->file, spte_p->upage, spte_p->b_read);
              }
            }
            free(spte_p);
          }
          temp_offset++;
        }
        file_close(be_freed->file_p);
      }
    }

    i = list_next(i);
  }
  lock_release (&file_lock);
}



void
syscall_init (void) 
{
  lock_init (&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  struct thread * cur = thread_current();

  cur->esp = f->esp;

  int* p = f->esp;
  check_pointer_validity(p+1,4);

  int which_sys = *(int* )f->esp;

  /*different system call*/
  if (which_sys == SYS_EXEC)
  {
    sysexec(f);
  }
  else if (which_sys == SYS_HALT)
  {
    syshalt(f);
  }
  else if (which_sys == SYS_EXIT)
  {
    sysexit(f);
  }
  else if (which_sys == SYS_WAIT)
  {
    syswait(f);
  }
  else if (which_sys == SYS_CREATE)
  {
    syscreate(f);
  }
  else if (which_sys == SYS_REMOVE)
  {
    sysremove(f);
  }
  else if (which_sys == SYS_OPEN)
  {
    sysopen(f);
  }
  else if (which_sys == SYS_WRITE)
  {
    syswrite(f);
  }
  else if (which_sys == SYS_SEEK)
  {
    sysseek(f);
  }
  else if (which_sys == SYS_TELL)
  {
    systell(f);
  }
  else if (which_sys == SYS_CLOSE)
  {
    sysclose(f);
  }
  else if (which_sys == SYS_READ)
  {
    sysread(f);
  }
  else if (which_sys == SYS_FILESIZE)
  {
    sysfilesize(f);
  }
  else if (which_sys == SYS_MMAP)
  {
    sysmmap(f);
  }
  else if (which_sys == SYS_MUNMAP)
  {
    sysmunmap(f);
  }
  else
  {
    thread_current()->exit_num = -1;
    thread_exit();
  }
}
