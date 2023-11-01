#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "threads/thread.h"
#include "filesys/cache.h"
#include "filesys/directory.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  cache_init();
  
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  flush_all();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  if ((*name == NULL)||(strlen(name) > 14)){
    return false;
  }

  block_sector_t inode_sector = 0;
  
  struct dir *current_dir = thread_current()->pwd;
  
  if (current_dir){
    current_dir = dir_reopen (current_dir);
  }else{
    current_dir = dir_open_root ();
  }

  char *tar_name = malloc(15);
  struct dir *tar_dir = NULL;
  struct inode *inode = NULL;
  
  bool ok = path_divide(name, current_dir, &tar_dir, &tar_name);
  if (!ok)
  {
    dir_close (tar_dir);
    inode_close (inode);
    free (tar_name);
    return false;
  }
  /* find the same name */
  if (dir_lookup(tar_dir, tar_name, &inode))
  {
    dir_close (tar_dir);
    inode_close (inode);
    free (tar_name);
    return false;
  }
  
  bool success = 0;

  if (tar_dir != NULL)
  {
    if (free_map_allocate (1, &inode_sector))
    {
      if (inode_create (inode_sector, initial_size))
      {
        if (dir_add (tar_dir, tar_name, inode_sector))
        {
          success = 1;
        }
        
      }
      
    }
    
  }
  
  dir_close (tar_dir);
  free (tar_name);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  if (*name == NULL){
    return NULL; 
  }
  
  struct dir *current_dir = thread_current()->pwd;
  if (current_dir){
    current_dir = dir_reopen (current_dir);
  }else{
    current_dir = dir_open_root ();
  }

    
  char *tar_name = malloc(15);
  struct dir *tar_dir = NULL;
  struct inode *inode = NULL;
  
  bool success = path_divide (name, current_dir, &tar_dir, &tar_name);
  if (success)
  {
    if (dir_lookup (tar_dir, tar_name, &inode))
    {
      dir_close (tar_dir);
      free (tar_name);
      return file_open(inode);
    }
  }
  
  dir_close (tar_dir);
  free (tar_name);
  return NULL;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{

  if (*name == NULL){
    return false;
  }
    
  struct dir *current_dir = thread_current()->pwd;
  if (current_dir){
    current_dir = dir_reopen (current_dir);
  }else{
    current_dir = dir_open_root ();
  }
    
  char *tar_name = malloc(15);
  struct dir *tar_dir = NULL;
  struct inode *inode = NULL;

  bool success = path_divide (name, current_dir, &tar_dir, &tar_name);
  if (!dir_lookup (tar_dir, tar_name, &inode))
  {
    dir_close (tar_dir);
    free (tar_name);
    return false;
  }
  if (!success)
  {
    dir_close (tar_dir);
    free (tar_name);
    return false;
  }

  success = 0;
  if (whether_is_dir(inode)){
    struct dir* move_dir = dir_open (inode);
    if (dir_is_empty (move_dir))
    {
      if (count_open_inode (inode)<2)
      {
        if (clear_dir (move_dir))
        {
          if (dir_remove (tar_dir, tar_name))
          {
            success = 1;
          }
        }
      }
    }
  }else{
    success = dir_remove(tar_dir, tar_name);
  }
  inode_close (inode);
  dir_close (tar_dir);
  free (tar_name);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}





bool 
chdir (const char *name)
{

  if (*name == NULL){
    return false;
  }
    
  struct thread *curr = thread_current();
  struct dir *current_dir = curr->pwd;

  if (current_dir){
    current_dir = dir_reopen (current_dir);
  }else{
    current_dir = dir_open_root ();
  }

  char *tar_name = malloc(15);
  struct dir *tar_dir = NULL;
  struct inode *inode = NULL;

  bool success = path_divide (name, current_dir, &tar_dir, &tar_name);
  if (success)
  {
    if (dir_lookup (tar_dir, tar_name, &inode))
    {
      if (whether_is_dir(inode))
      {
        dir_close (curr->pwd);
        curr->pwd = dir_open (inode);
        dir_close (tar_dir);
        free (tar_name);
        return true;
      }
    }
  }
  dir_close (tar_dir);
  free (tar_name);
  return false;
}


bool 
mkdir (const char *name)
{
  if ((*name == NULL)||(name == NULL)){
    return false;
  }

  struct dir *current_dir = thread_current()->pwd;
  if (current_dir){
    current_dir = dir_reopen (current_dir);
  }else{
    current_dir = dir_open_root ();
  }
    

  char *tar_name = malloc(15);
  struct dir *tar_dir = NULL;
  struct inode *inode = NULL;

  bool success = path_divide (name, current_dir, &tar_dir, &tar_name);
  if (!success)
  {
    dir_close (tar_dir);
    inode_close (inode);
    free (tar_name);
    return false;
  }
  if (dir_lookup (tar_dir, tar_name, &inode))
  {
    dir_close (tar_dir);
    inode_close (inode);
    free (tar_name);
    return false;
  }
  

  block_sector_t sector;
  if (free_map_allocate (1, &sector))
  {
    if (dir_create (sector, 0))
    {
      success = dir_add (tar_dir, tar_name, sector);
      dir_close (tar_dir);
      free (tar_name);
      return success;
    }
    
  }
  dir_close (tar_dir);
  free (tar_name);
  return false;
}
