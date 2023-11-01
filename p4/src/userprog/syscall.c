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
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"

/* 1 some functions */
static void syscall_handler (struct intr_frame *);




/*----------------------------------my code begin---------------------------*/

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

/*----------------------------------my code end----------------------------*/

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
  
  lock_acquire (&file_lock);
  if(fd == 0)
  {
    f->eax = -1;
    lock_release (&file_lock);
  }
  else{
    if(fd == 1)
    {
      f->eax=size;
      putbuf(buffer, size);
      lock_release (&file_lock);
    }
    else{
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
              f->eax = file_write (target_file->file_pointer, buffer, size);
            } 
            break;
          }
          i=list_next(i);
        }
      }
      lock_release (&file_lock);
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

    if (!whether_is_dir(file_get_inode (my_openfile))){
      f_node->dir_pointer = NULL;
    }else{
      f_node->dir_pointer = dir_open (file_get_inode (my_openfile));
    }
      
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

  check_pointer_validity_void(buffer,size);


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

void
check(char * p){
  if(!is_user_vaddr (p)){
    thread_current()->exit_num=-1;
    thread_exit();
  }
  if (!pagedir_get_page (thread_current ()->pagedir, p))
  {
    thread_current()->exit_num=-1;
    thread_exit();
  }
  
}

void 
syschdir (struct intr_frame* f){
  int* p = (int*)f->esp;
  p++;
  char *dir = (char *)(*p);

  for (char * p = dir; ; p++)
  {
    check(p);
    if (*p == '\0'){
      break;
    }
      
  }
  lock_acquire (&file_lock);
  f->eax = chdir (dir);
  lock_release (&file_lock);
}

void 
sysmkdir (struct intr_frame* f){
  int* p = (int*)f->esp;
  p++;
  char *dir = (char *)(*p);

  for (char * p = dir; ; p++)
  {
    check(p);
    if (*p == '\0'){
      break;
    }
  }
  lock_acquire (&file_lock);
  f->eax = mkdir (dir);
  lock_release (&file_lock);
}

void 
sysisdir (struct intr_frame* f){
  int* p = (int*)f->esp;
  p++;
  int fd = *(p);
  lock_acquire (&file_lock);
  f->eax = isdir (fd);
  lock_release (&file_lock);
}

void 
sysinumber (struct intr_frame* f){
  int* p = (int*)f->esp;
  p++;
  int fd = *(p);
  lock_acquire (&file_lock);
  f->eax = inumber (fd);
  lock_release (&file_lock);
}

void 
sysreaddir (struct intr_frame* f){
  int* p = (int*)f->esp;
  p++;
  int fd = *(p);
  p++;
  char *name = (void*)(*p);

  for (char * p = name; ; p++)
  {
    check(p);
    if (*p == '\0'){
      break;
    }
  }
  lock_acquire (&file_lock);
  f->eax = readdir (fd, name);
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
  else if (which_sys == SYS_CHDIR)
  {
    syschdir(f);
  }
  else if (which_sys == SYS_MKDIR)
  {
    sysmkdir(f);
  }
  else if (which_sys == SYS_ISDIR)
  {
    sysisdir(f);
  }
  else if (which_sys == SYS_INUMBER)
  {
    sysinumber(f);
  }
  else if (which_sys == SYS_READDIR)
  {
    sysreaddir(f);
  }
  else
  {
    thread_current()->exit_num = -1;
    thread_exit();
  }
}