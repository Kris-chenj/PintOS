#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
static char dummy_ze[BLOCK_SECTOR_SIZE];

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    int i_type;                         /* 0 represent directory, 1 represent common file */
    int dir_b[DIR_L];                   /* Direct point array */
    int indir_b[INDIR_L];               /* Indirect point array */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[85];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
  {
    /* If in direct sector */
    if(pos < DIR_SECTOR){return inode->data.dir_b[pos/BLOCK_SECTOR_SIZE];}
    /* If in indirect sector */
    else if(pos < MAX_SIZE)
    {
      int ans;
      int *dummy = calloc(TABLE_L,sizeof(int*));
      int table_entry = (pos-DIR_SECTOR)/TABLE_SECTOR;
      int block_entry = (pos-DIR_SECTOR-table_entry*TABLE_SECTOR)/BLOCK_SECTOR_SIZE;
      /* Read data to dummy address*/
      c_read(inode->data.indir_b[table_entry],dummy);
      ans = dummy[block_entry];
      /* Clean dummy space */
      free(dummy);return ans;
    }
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      int dir_len,indir_len,t_index;
      bool whether_dir;
      char dummy[BLOCK_SECTOR_SIZE];
      size_t sectors = bytes_to_sectors (length);
      /* Common file */
      disk_inode->i_type = 1;
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      /* Assess whether direct can hold it */

      /* Direct hold */
      if(sectors <= DIR_L){dir_len = sectors;whether_dir = false;}
      else{dir_len = DIR_L;whether_dir = true;}
      /* Set data for every block */
      for(int i = 0;i<dir_len;++i)
      {
        /* If space not enough */
        if(!free_map_allocate(1,&disk_inode->dir_b[i])){free(disk_inode);return false;}
        c_write(disk_inode->dir_b[i],dummy_ze);
      }
      if(sectors <= DIR_L){c_write(sector,disk_inode);free(disk_inode);return true;}

      /* Indirect hold */
      indir_len = sectors-DIR_L;
      int *dummy_sector = calloc (TABLE_L, sizeof(int*));
      for (int i=0;i<indir_len;i+=TABLE_L)
      {
        /* If space not enough */
        if (!free_map_allocate(1,&disk_inode->indir_b[i/TABLE_L])){free(disk_inode);free(dummy_sector);return false;}
        /* Fill with zeros */
        memset(dummy_sector, 0, BLOCK_SECTOR_SIZE);
        /* Assess whether fit with table length */
        if(indir_len - i <= TABLE_L){t_index = indir_len - i;}
        else{t_index = TABLE_L;}

        /* Fit */
        for (int j=0;j<t_index;++j)
        {
          /* If space not enough */
          if (!free_map_allocate(1,&dummy_sector[j])){free(disk_inode);free(dummy_sector);return false;}
          c_write(dummy_sector[j],dummy_ze);
        }
        /* Write block to cache with the indirect index */
        c_write(disk_inode->indir_b[i/TABLE_L],dummy_sector);
      }
      free(dummy_sector);
      c_write(sector,disk_inode);free(disk_inode);success = true;
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  /* Cache complement */
  c_read(inode->sector,&inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          int block,leng;
          /* For direct pointer */
          int sector = bytes_to_sectors(inode->data.length);
          if(sector <= DIR_L){block = sector;}
          else{block = DIR_L;}
          for(int i = 0;i<block;++i){free_map_release(inode->data.dir_b[i],1);}
          if(sector <= DIR_L){free_map_release(inode->sector,1);free(inode);return;}
          /* For indirect pointer */
          block = sector - DIR_L;
          int *dummy = calloc(TABLE_L,sizeof(int *));
          for(int i = 0;i<block;i+=TABLE_L)
          {
            int index = i / TABLE_L;
            c_read(inode->data.indir_b[index],dummy);
            if(block-i <= TABLE_L){leng = block-i;}
            else{leng = TABLE_L;}
            for(int j = 0;j<leng;++j){free_map_release(dummy[j],1);}
            free_map_release(inode->data.indir_b[index],1);
          }
          free_map_release (inode->sector, 1);
          free(dummy);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          /* Cache complement */
          c_read(sector_idx,buffer+bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          /* Cache complement */
          c_read(sector_idx,bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  int off = offset + size - 1;
  write_sec(inode,off);

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          /* Cache complement */
          c_write(sector_idx,buffer+bytes_written);
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            /* Cache complement */
            c_read(sector_idx,bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          /* Cache complement */
          c_write(sector_idx,bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

/* Assess whether the inode is a diretory. */
bool whether_is_dir(struct inode* inode){return !(inode->data.i_type);}

/* Build association between file and inode. */
void set_dir(struct inode* inode)
{
  inode->data.i_type = 0;
  c_write(inode->sector,&inode->data);
}

/* Count number of the inode associated opening. */
int count_open_inode(struct inode* inode){return inode->open_cnt;}

/* Assitant function on write at */
void write_sec(struct inode *inode, off_t off)
{
  /* Find corresponding sector by offset */
  int index_of,index_indir;
  if(off > inode->data.length)
  {
    /* For range of direct pointer */
    int next_pos = bytes_to_sectors(off+1);
    int inde = bytes_to_sectors(inode->data.length);
    if(inde < DIR_L)
    {
      int iteration;
      if(next_pos <= DIR_L){iteration = next_pos;}
      else{iteration = DIR_L;}
      for(int i = inde;i<iteration;++i)
      {
        if(!free_map_allocate(1,&inode->data.dir_b[i])){return;}
        c_write(inode->data.dir_b[i],dummy_ze);
      }
    }
    /* Update variable of remains */
    if(next_pos <= DIR_L)
    {
      inode->data.length = off+1;c_write(inode->sector,&inode->data);
      return;
    }
    /* For range of indirect pointer */
    int in_new = next_pos - DIR_L;
    int *dummy = calloc(TABLE_L,sizeof(int *));
    int inde_new;
    if(inde > DIR_L){inde_new = inde - DIR_L;}
    else{inde_new = 0;}
    int table_num = (inde_new + TABLE_L - 1) / TABLE_L;
    for(int i= (inde_new/ TABLE_L)*TABLE_L;i<in_new; i+=TABLE_L)
    {
      index_indir = i / TABLE_L;
      if(table_num < index_indir + 1)
      {
        if(!free_map_allocate(1,&inode->data.indir_b[index_indir])){free(dummy);return;}
        /* Set 0 to dummy space */
        memset(dummy,0,BLOCK_SECTOR_SIZE);
        index_of = 0;
      }
      else{c_read(inode->data.indir_b[index_indir],dummy);index_of = inde_new % TABLE_L;}
      /* Last iteration */
      int last_iteration;
      if(in_new-i < TABLE_L){last_iteration = in_new-i;}
      else{last_iteration = TABLE_L;}
      /* Manipulate to each table entry */
      for(int j = index_of;j<last_iteration;++j)
      {
        if(!free_map_allocate(1,&dummy[j])){free(dummy);return;}
        c_write(dummy[j],dummy_ze);
      }
      /* Write data back to cache */
      c_write(inode->data.indir_b[index_indir],dummy);
    }
    free(dummy);
  }
  else{return;}
  /* Update data length of inode */
  inode->data.length = off + 1;
  /* Lastly, write back all remain data */
  c_write(inode->sector,&inode->data);
  return;
}