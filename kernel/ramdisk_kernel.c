




#include "ramdisk_kernel.h"
#include <linux/uaccess.h>
#include <linux/vmalloc.h>


static unsigned char *ramdisk_memory;

#define NULL 0


/* This function implement the string copy function. */
void strcpy_ramdisk(char *dst, const char *src)
{
  int index = 0;

  /* Copy each character of the string. */
  while ('\0' != src[index])
  {
    dst[index] = src[index];
    index++;
  }
  dst[index] = '\0';
}

/* This function compare the content of two string. */
int strcmp_ramdisk(const char *str1, const char *str2)
{
  const char *cur_str1 = NULL;
  const char *cur_str2 = NULL;

  cur_str1 = str1;
  cur_str2 = str2;
  /* Compare the value of each character of the string. */
  while (('\0' != cur_str1[0]) && ('\0' != cur_str2[0]))
  {
    if (cur_str1[0] != cur_str2[0])
    {
      return (cur_str1[0] - cur_str2[0]);
    }
    cur_str1++;
    cur_str2++;
  }
  /* Compare the length of these two string. */
  if (('\0' == cur_str1[0]) && ('\0' != cur_str2[0]))
  {
    return -1;
  }
  else if (('\0' != cur_str1[0]) && ('\0' == cur_str2[0]))
  {
    return 1;
  }

  return 0;
}

/* This function compare the content of two string. */
int strncmp_ramdisk(const char *str1, const char *str2_start, const char *str2_end)
{
  const char *cur_str1 = NULL;
  const char *cur_str2 = NULL;

  cur_str1 = str1;
  cur_str2 = str2_start;
  /* Compare the value of each character of the string. */
  while (('\0' != cur_str1[0]) && (cur_str2 < str2_end))
  {
    if (cur_str1[0] != cur_str2[0])
    {
      return (cur_str1[0] - cur_str2[0]);
    }
    cur_str1++;
    cur_str2++;
  }
  /* Compare the length of these two string. */
  if (('\0' == cur_str1[0]) && (cur_str2 < str2_end))
  {
    return -1;
  }
  else if (('\0' != cur_str1[0]) && (cur_str2 >= str2_end))
  {
    return 1;
  }

  return 0;
}

/* This function find the end of the child directory entry name. */
const char *ramdisk_find_next_dir_split(const char *str)
{
  const char *curr_str = NULL;

  curr_str = str;
  while ('\0' != curr_str[0])
  {
    /* The '/' or the '\\' character indicate the end of
       the child directory entry name. */
    if (('/' == curr_str[0]) || ('\\' == curr_str[0]))
    {
      return curr_str;
    }
    curr_str++;
  }

  return NULL;
}

/* This function get the filename from the pathname. */
const char *get_filename(const char *pathname)
{
  const char *filename_start = NULL;
  const char *filename_end = NULL;

  filename_start = pathname;
  if ('/' == filename_start[0])
  {
    filename_start = filename_start + 1;
  }
  while (1)
  {
    /* If we can not find the end of the child directory entry name,
       this indicate that we find the filename from the pathname .*/
    filename_end = ramdisk_find_next_dir_split(filename_start);
    if (NULL == filename_end)
    {
      break;
    }
    filename_start = filename_end + 1;
  }

  return filename_start;
}

/* This function set one value to a block memory. */
void memset_ramdisk(void *dst, unsigned char value, int size)
{
  int loop = 0;
  unsigned char *curr = NULL;

  /* Set the value to each byte of the memory in the block. */
  curr = (unsigned char *)dst;
  for (loop = 0; loop < size; loop++)
  {
    curr[loop] = value;
  }
}

/* This function copy the data from one block memory to the other block memory. */
void memcpy_ramdisk(void *dst, void *src, int size)
{
  int loop = 0;
  unsigned char *curr_dst = NULL;
  unsigned char *curr_src = NULL;

  curr_dst = (unsigned char *)dst;
  curr_src = (unsigned char *)src;
  /* Copy each byte of data from
     one block memory to the other block memory. */
  for (loop = 0; loop < size; loop++)
  {
    curr_dst[loop] = curr_src[loop];
  }
}


/* This calculate the upper bound of an integer division. */
int upper_bound(int dividend, int divisor)
{
  int upper_bound_result = 0;

  upper_bound_result = dividend / divisor;
  if (0 != (dividend % divisor))
  {
    upper_bound_result++;
  }

  return upper_bound_result;
}

/* This function return the smaller value of two integer values. */
int min_ramdisk(int value1, int value2)
{
  return ((value1 < value2) ? value1 : value2);
}



/* This function is the initialize function of the ramdisk. */
void ramdisk_init()
{
  int loop = 0;
  int i = 0;
  int j = 0;
  int index = 0;
  superblock_t *superblock = NULL;
  index_node_t *index_node_array = NULL;
  unsigned char *block_bitmap = NULL;

  /* Call the vmalloc function to allocate 2MB memory for the ramdisk. */
  ramdisk_memory = (unsigned char *)vmalloc(sizeof(unsigned char) * RAMDISK_MEMORY_SIZE);

  /* Initialize the superblock's data structure. */
  superblock = ramdisk_get_superblock();
  memset_ramdisk(superblock, 0, BLK_SZ);
  superblock->num_free_blocks = RAMDISK_MEMORY_SIZE / BLK_SZ;
  superblock->num_free_index_nodes = MAX_INDEX_NODES_COUNT;
  /* Initialize the root directory's index node which is reside in the superblock */
  strcpy_ramdisk(superblock->first_block.type, "dir");

  /* Initialize the total 1024 index node array and clear them with 0. */
  index_node_array = ramdisk_get_index_node(1);
  memset_ramdisk(index_node_array, 0, sizeof(unsigned char) * (BLK_SZ * INDEX_NODE_ARRAY_BLOCK_COUNT));

  /* Initialize the total 8192 block's block bitmap flag to free status. */
  block_bitmap = ramdisk_get_block_bitmap();
  for (i = 0; i < BLOCK_BITMAP_BLOCK_COUNT; i++)
  {
    for (j = 0; j < BLK_SZ; j++)
    {
      block_bitmap[index++] = 0x0FF;
    }
  }

  /* Set the first (1 + INDEX_NODE_ARRAY_BLOCK_COUNT + BLOCK_BITMAP_BLOCK_COUNT) block's
     block bitmap flag to used status. (Include the superblock, 256 "index node" structures blocks
     and four "block bitmap" used to keep track of free and allocated blocks
     in the rest of the ramdisk partition.)  */
  for (loop = 0; loop < (1 + INDEX_NODE_ARRAY_BLOCK_COUNT + BLOCK_BITMAP_BLOCK_COUNT); loop++)
  {
    ramdisk_block_malloc();
  }
}

/* This function free the 2MB memory of the ramdisk allocated by call the vmalloc function. */
void ramdisk_uninit()
{
  if (NULL != ramdisk_memory)
  {
    vfree(ramdisk_memory);
    ramdisk_memory = NULL;
  }
}

/* This function create a regular file with absolute pathname
   from the root of the directory tree,
   where each directory filename is delimited by a "/" character.
   */
int ramdisk_creat(char *pathname)
{
  int index_node_number = 0;
  index_node_t *parent_directory_index_node = NULL;
  index_node_t *index_node = NULL;
  const char *filename = NULL;
  dir_entry_t entry;

  /* If the parent directory for the new regular file does not exist,
     this function will return -1 to indicate fail. */
  parent_directory_index_node = ramdisk_get_directory_index_node(pathname);
  if (NULL == parent_directory_index_node)
  {
    return -1;
  }
  /* If the file corresponding to pathname already exists,
     this function will return -1 to indicate fail. */
  filename = get_filename(pathname);
  if (NULL != ramdisk_get_dir_entry(parent_directory_index_node, filename, NULL))
  {
    return -1;
  }
  /* If there has no unused index node,
     this function will return -1 to indicate fail. */
  index_node_number = ramdisk_get_unused_index_node();
  if (-1 == index_node_number)
  {
    return -1;
  }
  index_node = ramdisk_get_index_node(index_node_number);
  memset_ramdisk(index_node, 0, sizeof(index_node_t));
  /* set the type of the new used index node to regular file. */
  strcpy_ramdisk(index_node->type, "reg");

  /* update the parent directory file, to include the newentry */
  strcpy_ramdisk(entry.filename, filename);
  entry.index_node_number = index_node_number;
  if (0 != ramdisk_update_parent_directory_file(parent_directory_index_node, &entry))
  {
    return -1;
  }

  return 0;
}

/* This function remove the file from the ramdisk with absolute
   pathname from the filesystem, freeing its memory in the ramdisk. */
int ramdisk_unlink(char *pathname)
{
  const char *filename = NULL;
  index_node_t *parent_directory_index_node = NULL;
  index_node_t *index_node = NULL;
  dir_entry_t *entry = NULL;
  superblock_t *superblock = NULL;

  /* If unlink the root directory file,
     this function will return -1 to indicate fail. */
  if ((0 == strcmp_ramdisk("", pathname))
    || (0 == strcmp_ramdisk("/", pathname)))
  {
    return -1;
  }
  /* If the parent directory for the given pathname does not exist,
     this function will return -1 to indicate fail. */
  parent_directory_index_node = ramdisk_get_directory_index_node(pathname);
  if (NULL == parent_directory_index_node)
  {
    return -1;
  }
  /* If the pathname does not exist,
     this function will return -1 to indicate fail. */
  filename = get_filename(pathname);
  entry = ramdisk_get_dir_entry(parent_directory_index_node, filename, NULL);
  if (NULL == entry)
  {
    return -1;
  }
  /* If unlink a non-empty directory file,
     this function will return -1 to indicate fail. */
  index_node = ramdisk_get_index_node(entry->index_node_number);
  if ((0 == strcmp_ramdisk("dir", index_node->type))
    && (index_node->dir_entry_count > 0))
  {
    return -1;
  }
  /* If unlink an open file,
     this function will return -1 to indicate fail. */
  if (index_node->open_counter > 0)
  {
    return -1;
  }

  /* Freeing the index node's memory in the ramdisk. */
  ramdisk_free_index_node_memory(index_node);

  /* Free the index node and return it to the unused index node array. */
  memset_ramdisk(index_node, 0, sizeof(index_node_t));
  superblock = ramdisk_get_superblock();
  superblock->num_free_index_nodes++;

  /* remove the correspond directory entry from the parent directory. */
  memset_ramdisk(entry, 0, sizeof(dir_entry_t));
  parent_directory_index_node->dir_entry_count--;

  /* If the parent directory has no any directory entry,
     free the parent directory's index node's memory in the ramdisk. */
  if (0 == parent_directory_index_node->dir_entry_count)
  {
    ramdisk_free_index_node_memory(parent_directory_index_node);
  }

  return 0;
}

/* This function open an existing file corresponding to pathname. */
int ramdisk_open(char *pathname, int *index_node_number)
{
  const char *filename = NULL;
  index_node_t *parent_directory_index_node = NULL;
  index_node_t *index_node = NULL;
  dir_entry_t *entry = NULL;
  superblock_t *superblock = NULL;

  /* If open the root directory, then return 0
     as the root directory's index node number. */
  if ((0 == strcmp_ramdisk("", pathname))
    || (0 == strcmp_ramdisk("/", pathname)))
  {
    *index_node_number = 0;
    /* Increase the index node's open counter. */
    superblock = ramdisk_get_superblock();
    superblock->first_block.open_counter++;
    return 0;
  }
  /* If the parent directory for the given pathname does not exist,
     this function will return -1 to indicate fail. */
  parent_directory_index_node = ramdisk_get_directory_index_node(pathname);
  if (NULL == parent_directory_index_node)
  {
    return -1;
  }
  /* If the pathname does not exist,
     this function will return -1 to indicate fail. */
  filename = get_filename(pathname);
  entry = ramdisk_get_dir_entry(parent_directory_index_node, filename, NULL);
  if (NULL == entry)
  {
    return -1;
  }
  /* The index node which is not the root directory's index node
     will has a index node number range from 1 to 1024. */
  *index_node_number = entry->index_node_number;

  /* Increase the index node's open counter. */
  index_node = ramdisk_get_index_node(entry->index_node_number);
  index_node->open_counter++;

  return 0;
}

/* This function close the corresponding file. */
int ramdisk_close(int index_node_number)
{
  index_node_t *index_node = NULL;

  /* Decrease the index node's open counter. */
  index_node = ramdisk_get_index_node(index_node_number);
  index_node->open_counter--;

  return 0;
}

/* This function read up to num_bytes from a regular file
   to the specified address in the calling process. */
int ramdisk_read(int index_node_number, int pos, char *address, int num_bytes)
{
  int data_length_read = 0;
  int data_length_to_read_once = 0;
  int remainder_data_length_in_block = 0;
  int remainder_data_length_to_read = 0;
  char *dst = NULL;
  char *src = NULL;
  index_node_t *index_node = NULL;
  file_position_t file_position;

  /* If the type of the index node is a a directory file,
     this function will return -1 to indicate fail. */
  index_node = ramdisk_get_index_node(index_node_number);
  if (0 != strcmp_ramdisk("reg", index_node->type))
  {
    return -1;
  }
  /* check whether the num_bytes exceed the remainder data length
     in this file. */
  num_bytes = min_ramdisk(num_bytes, index_node->size - pos);
  /* Init the file position data structure. */
  ramdisk_file_position_init(&file_position, index_node, pos, 1);
  dst = address;
  remainder_data_length_to_read = num_bytes;
  /* read the data block by block, copy the data from the ramdisk
     to the user space. */
  while (remainder_data_length_to_read > 0)
  {
    remainder_data_length_in_block = BLK_SZ - file_position.data_offset_in_block;
    /* the data length to read once should not exceed the
       remainder data length in one block. */
    data_length_to_read_once = min_ramdisk(remainder_data_length_in_block, remainder_data_length_to_read);
    src = ramdisk_get_memory_address(&file_position);
    if (NULL == src)
    {
      break;
    }
    /* copy the data from the ramdisk to the user space.*/
    copy_to_user(dst, src, data_length_to_read_once);
    data_length_read = data_length_read + data_length_to_read_once;
    dst = dst + data_length_to_read_once;
    remainder_data_length_to_read = remainder_data_length_to_read - data_length_to_read_once;
    /* add the file position. */
    ramdisk_file_position_add(&file_position, data_length_to_read_once);
  }

  return data_length_read;
}

/* This function write up to num_bytes from the specified address
   in the calling process to a regular file. */
int ramdisk_write(int index_node_number, int pos, char *address, int num_bytes)
{
  int data_length_written = 0;
  int data_length_to_write_once = 0;
  int remainder_space_in_block = 0;
  int remainder_data_length_to_write = 0;
  char *dst = NULL;
  char *src = NULL;
  index_node_t *index_node = NULL;
  file_position_t file_position;

  /* If the type of the index node is a directory file,
     this function will return -1 to indicate fail. */
  index_node = ramdisk_get_index_node(index_node_number);
  if (0 != strcmp_ramdisk("reg", index_node->type))
  {
    return -1;
  }
  /* The file size should not exceed
     256 * 8 + 64 * 256 + 64^2 * 256 = 1067008 bytes. */
  num_bytes = min_ramdisk(num_bytes, MAX_FILE_SIZE - pos);
  /* Init the file position data structure. */
  if (num_bytes > 0)
  {
    ramdisk_file_position_init(&file_position, index_node, pos, 0);
  }
  src = address;
  remainder_data_length_to_write = num_bytes;
  /* read the data block by block, copy the data from the user space
     to the ramdisk. */
  while (remainder_data_length_to_write > 0)
  {
    remainder_space_in_block = BLK_SZ - file_position.data_offset_in_block;
    /* the data length to read once should not exceed the
       remainder space in one block. */
    data_length_to_write_once = min_ramdisk(remainder_space_in_block, remainder_data_length_to_write);
    dst = ramdisk_get_memory_address(&file_position);
    /* If there has no block space in the ramdisk, stop writting. */
    if (NULL == dst)
    {
      break;
    }
    /* copy the data from the user space to the ramdisk. */
    copy_from_user(dst, src, data_length_to_write_once);
    data_length_written = data_length_written + data_length_to_write_once;
    src = src + data_length_to_write_once;
    remainder_data_length_to_write = remainder_data_length_to_write - data_length_to_write_once;
    /* add the file position. */
    if (remainder_data_length_to_write > 0)
    {
      ramdisk_file_position_add(&file_position, data_length_to_write_once);
    }
  }
  index_node->size = (pos + data_length_written > index_node->size) ? (pos + data_length_written) : index_node->size;

  return data_length_written;
}

/* This function set the file object's file position
   identified by index node number index_node_number,
   to offset, returning the new position, or the end of the
   file position if the offset is beyond the file's current size. */
int ramdisk_lseek(int index_node_number, int seek_offset, int *seek_result_offset)
{
  index_node_t *index_node = NULL;

  /* If the type of the index node is a directory file,
     this function will return -1 to indicate fail. */
  index_node = ramdisk_get_index_node(index_node_number);
  if (0 != strcmp_ramdisk("reg", index_node->type))
  {
    return -1;
  }
  /* return the beginning of the file position if the offset is below zero. */
  if (seek_offset < 0)
  {
    *seek_result_offset = 0;
  }
  /* return the end of the file position if the offset is beyond the file's current size. */
  else if (seek_offset > index_node->size)
  {
    *seek_result_offset = index_node->size;
  }
  /* returning the new position. */
  else
  {
    *seek_result_offset = seek_offset;
  }

  return 0;
}

/* This function create a directory file with absolute pathname
   from the root of the directory tree,
   where each directory filename is delimited by a "/" character. */
int ramdisk_mkdir(char *pathname)
{
  int index_node_number = 0;
  index_node_t *parent_directory_index_node = NULL;
  index_node_t *index_node = NULL;
  const char *directory_name = NULL;
  dir_entry_t entry;

  /* If the parent directory for the new regular file does not exist,
     this function will return -1 to indicate fail. */
  parent_directory_index_node = ramdisk_get_directory_index_node(pathname);
  if (NULL == parent_directory_index_node)
  {
    return -1;
  }
  /* If the directory corresponding to pathname already exists,
     this function will return -1 to indicate fail. */
  directory_name = get_filename(pathname);
  if (NULL != ramdisk_get_dir_entry(parent_directory_index_node, directory_name, NULL))
  {
    return -1;
  }
  /* If there has no unused index node,
  this function will return -1 to indicate fail. */
  index_node_number = ramdisk_get_unused_index_node();
  if (-1 == index_node_number)
  {
    return -1;
  }
  /* set the type of the new used index node to directory. */
  index_node = ramdisk_get_index_node(index_node_number);
  memset_ramdisk(index_node, 0, sizeof(index_node_t));
  strcpy_ramdisk(index_node->type, "dir");

  /* update the parent directory file, to include the newentry */
  strcpy_ramdisk(entry.filename, directory_name);
  entry.index_node_number = index_node_number;
  if (0 != ramdisk_update_parent_directory_file(parent_directory_index_node, &entry))
  {
    return -1;
  }

  return 0;
}

/* This function read one entry from a directory file identified by
   index node number index_node_number, and store the result
   in memory at the specified value of address. */
int ramdisk_readdir(int index_node_number, char *address, int *pos)
{
  dir_entry_t *entry = NULL;
  index_node_t *index_node = NULL;
  file_position_t file_position;

  /* If the type of the index node is a regular file,
     this function will return -1 to indicate fail. */
  index_node = ramdisk_get_index_node(index_node_number);
  if (0 != strcmp_ramdisk("dir", index_node->type))
  {
    return -1;
  }
  /* Init the file position data structure. */
  ramdisk_file_position_init(&file_position, index_node, *pos, 1);
  /* Scan through the directory file from the specified file position
     to the end of the directory file. */
  while (file_position.file_position < index_node->size)
  {
    entry = (dir_entry_t *)ramdisk_get_memory_address(&file_position);
    if (NULL == entry)
    {
      break;
    }
    /* Add the file position by the size of the directory entry,
       whose value is 16. */
    ramdisk_file_position_add(&file_position, sizeof(dir_entry_t));

    /* If we find a valid entry, then increment the file position
       to refer to the next entry and return the found directory entry. */
    if (0 != strcmp_ramdisk(entry->filename, ""))
    {
      memcpy_ramdisk(address, entry, sizeof(dir_entry_t));
      *pos = file_position.file_position;
      return 1;
    }
  }

  /* If either the directory is empty or the last entry has been read
     by a previous call to ramdisk_readdir(),, then increment the file position
     to refer to the next entry and return 0 to indicate end-of-file.. */
  *pos = file_position.file_position;

  return 0;
}

/* This function return the length of the directory entry data structure, which is the value 16. */
int ramdisk_get_dir_entry_length()
{
  return ((int)sizeof(dir_entry_t));
}

/* This function Freeing the file index node's memory in the ramdisk. */
void ramdisk_free_index_node_memory(index_node_t *index_node)
{
  int loop = 0;
  int block_pointer_value = 0;
  int *location = NULL;
  block_pointer_t block_pointer;

  /* Initialize the block pointer. */
  ramdisk_block_pointer_init(&block_pointer, index_node, 0, 1);
  /* Freeing the file index node's memory
     block by block in the ramdisk. */
  for (loop = 0; loop < MAX_BLOCK_COUNT_IN_FILE; loop++)
  {
    /* If we have free the last block's memory of the file index node,
       then stop freeing. */
    block_pointer_value = ramdisk_alloc_and_get_block_pointer(&block_pointer);
    if (block_pointer_value <= 0)
    {
      break;
    }
    ramdisk_block_free(block_pointer_value);
    ramdisk_block_pointer_increase(&block_pointer);
  }
  /* Free the block memory for the single-indrect block pointer. */
  if (index_node->location[SINGLE_INDIRECT_BLOCK_POINTER] > 0)
  {
    ramdisk_block_free(index_node->location[SINGLE_INDIRECT_BLOCK_POINTER]);
  }
  /* Free the block memory for the double-indrect block pointer. */
  if (index_node->location[DOUBLE_INDIRECT_BLOCK_POINTER] > 0)
  {
    location = (int *)ramdisk_get_block_memory_address(index_node->location[DOUBLE_INDIRECT_BLOCK_POINTER]);
    for (loop = 0; loop < PTRS_PB; loop++)
    {
      if (location[loop] <= 0)
      {
        break;
      }
      ramdisk_block_free(location[loop]);
    }
    ramdisk_block_free(index_node->location[DOUBLE_INDIRECT_BLOCK_POINTER]);
  }
  /* Clear the location attribute of the file index node with zero. */
  for (loop = 0; loop < 10; loop++)
  {
    index_node->location[loop] = 0;
  }
  /* Set the current size of the corresponding file in bytes to zero. */
  index_node->size = 0;
  /* Set total directory entry count in the directory file index node to zero. */
  index_node->dir_entry_count = 0;

}

/* This function return the superblock's memory address in the ramdisk. */
superblock_t *ramdisk_get_superblock()
{
  return (superblock_t *)ramdisk_memory;
}

/* This function return the memory address of
   one index node number specified by the index_node_number in the ramdisk. */
index_node_t *ramdisk_get_index_node(int index_node_number)
{
  superblock_t *superblock = NULL;
  index_node_t *index_node_array = NULL;

  /* If the index_node_number is zero,
     then return the memory address of
     the root directory's index node. */
  if (index_node_number <= 0)
  {
    superblock = ramdisk_get_superblock();
    return &superblock->first_block;
  }
  /* return the memory address of other index node. */
  index_node_array = (index_node_t *)(ramdisk_memory + BLK_SZ);
  return index_node_array + (index_node_number - 1);
}

/* This function return the memory address of 
   the beginning of the "block bitmap". */
unsigned char *ramdisk_get_block_bitmap()
{
  return (ramdisk_memory + BLK_SZ * (1 + INDEX_NODE_ARRAY_BLOCK_COUNT));
}

/* This function return the memory address of
   one block specified by the block_pointer in the ramdisk. */
char *ramdisk_get_block_memory_address(int block_pointer)
{
  return (ramdisk_memory + (BLK_SZ * block_pointer));
}

/* This function find and return one unused index node,
   if there has no unused index node, then return -1. */
int ramdisk_get_unused_index_node()
{
  int loop = 0;
  index_node_t *index_node_array = NULL;
  superblock_t *superblock = NULL;

  /* if there has no unused index node, then return -1. */
  superblock = ramdisk_get_superblock();
  if (superblock->num_free_index_nodes <= 0)
  {
    return -1;
  }
  index_node_array = ramdisk_get_index_node(1);
  for (loop = 0; loop < MAX_INDEX_NODES_COUNT; loop++)
  {
    /* If the index node's type is an empty string,
       then it indicate that this index node is an unused index node. */
    if (0 == strcmp_ramdisk(index_node_array[loop].type, ""))
    {
      superblock->num_free_index_nodes--;
      return (loop + 1);
    }
  }

  /* if there has no unused index node, then return -1. */
  return -1;
}

/* This function allocate one free block,
   if there has no free block, then return -1. */
int ramdisk_block_malloc()
{
  int index = 0;
  int block_pointer = 0;
  int i = 0;
  int j = 0;
  int k = 0;
  unsigned char mask = 0;
  superblock_t *superblock = NULL;
  unsigned char *block_bitmap = NULL;

  superblock = ramdisk_get_superblock();
  block_bitmap = ramdisk_get_block_bitmap();
  for (i = 0; i < BLOCK_BITMAP_BLOCK_COUNT; i++)
  {
    for (j = 0; j < BLK_SZ; j++)
    {
      mask = 1;
      for (k = 0; k < 8; k++)
      {
        /* If the bitmap mask bit of the correspond block is 1,
           then it indicate that this block is a free block,
           reset its bitmap mask bit to zero and return its
           block pointer number. */
        if ((block_bitmap[index] & mask) != 0)
        {
          block_bitmap[index] = block_bitmap[index] & (~mask);
          superblock->num_free_blocks--;
          return block_pointer;
        }

        block_pointer++;
        mask = mask << 1;
      }
      index++;
    }
  }

  /* if there has no free block, then return -1. */
  return -1;
}

/* This function allocate one free block and clear its all memory to zero,
   if there has no free block, then return -1. */
int ramdisk_block_calloc()
{
  int block_pointer = 0;

  block_pointer = ramdisk_block_malloc();
  if (block_pointer > 0)
  {
    /* clear its all memory to zero. */
    memset_ramdisk(ramdisk_get_block_memory_address(block_pointer), 0, BLK_SZ);
  }

  return block_pointer;
}

/* This function set one allocated used block to free status,
   and free it to the free block list. */
void ramdisk_block_free(int block_pointer)
{
  int index = 0;
  int k = 0;
  int mask = 0;
  superblock_t *superblock = NULL;
  unsigned char *block_bitmap = NULL;

  superblock = ramdisk_get_superblock();
  block_bitmap = ramdisk_get_block_bitmap();

  k = block_pointer % 8;
  index = block_pointer / 8;
  mask = (1 << k);
  /* If the bitmap mask bit of the correspond block is 0,
     then it indicate that this block is a allocated used block,
     set its bitmap mask bit to 1 to indicate the free status. */
  if (0 == (block_bitmap[index] & mask))
  {
    block_bitmap[index] = block_bitmap[index] | mask;
    superblock->num_free_blocks++;
  }
}

/* This function add an directory entry to its parent directory file index node. */
int ramdisk_update_parent_directory_file(index_node_t *index_node, dir_entry_t *entry)
{
  char *dst = NULL;
  file_position_t file_position;

  /* If there is an empty directory entry in the directory file index node,
     set the empty directory entry to the added empty directory entry. */
  if ((index_node->dir_entry_count * sizeof(dir_entry_t)) < ((unsigned int)index_node->size))
  {
    memcpy_ramdisk(ramdisk_get_empty_entry(index_node), entry, sizeof(dir_entry_t));
    index_node->dir_entry_count++;
    return 0;
  }

  /* If the size of the parent directory file exceed
     256 * 8 + 64 * 256 + 64^2 * 256 = 1067008 bytes,
     return -1 to indicate fail. */
  if ((index_node->size + sizeof(dir_entry_t)) > MAX_FILE_SIZE)
  {
    return -1;
  }
  /* Init the file position data structure. */
  ramdisk_file_position_init(&file_position, index_node, index_node->size, 0);
  /* If there has no block space in the ramdisk,
     return -1 to indicate fail.  */
  dst = ramdisk_get_memory_address(&file_position);
  if (NULL == dst)
  {
    return -1;
  }
  /* append the directory entry to the end of its parent directory file. */
  memcpy_ramdisk(dst, entry, sizeof(dir_entry_t));
  index_node->size = index_node->size + sizeof(dir_entry_t);
  index_node->dir_entry_count++;

  return 0;
}

/* This function find the directory file index node
   of a specified pathname. If the directory does not exists,
   then return NULL. */
index_node_t *ramdisk_get_directory_index_node(const char *pathname)
{
  index_node_t *index_node = NULL;
  const char *filename_start = NULL;
  const char *filename_end = NULL;
  dir_entry_t *entry = NULL;

  index_node = &ramdisk_get_superblock()->first_block;
  filename_start = pathname;
  if ('/' == filename_start[0])
  {
    filename_start = filename_start + 1;
  }
  while (1)
  {
    /* Find the end of the child directory entry name in the pathname.
       If the child directory entry name doest not exist in the pathname,
       then we have found the directory file index node, stop searching. */
    filename_end = ramdisk_find_next_dir_split(filename_start);
    if (NULL == filename_end)
    {
      break;
    }
    /* Find the child directory entry by its name from
       its parent directory file index node.
       if the child directory entry with the specified name does not exists in
       its parent directory file index node, then this indicate that
       the directory file index node we want to find does not exists,
       return -1 to indicate fail.  */
    entry = ramdisk_get_dir_entry(index_node, filename_start, filename_end);
    if (NULL == entry)
    {
      return NULL;
    }
    /* If the type of the child entry index node is a regular file,
       then this indicate that the directory file index node we
       want to find does not exists, return -1 to indicate fail, */
    index_node = ramdisk_get_index_node(entry->index_node_number);
    if (strcmp_ramdisk(index_node->type, "dir"))
    {
      return NULL;
    }

    filename_start = filename_end + 1;
  }

  return index_node;
}

/* This function find the child directory entry by a specified name
   from its parent directory file index node.*/
dir_entry_t *ramdisk_get_dir_entry(index_node_t *index_node, const char *filename_start, const char *filename_end)
{
  dir_entry_t *entry = NULL;
  file_position_t file_position;

  /* Init the file position data structure. */
  ramdisk_file_position_init(&file_position, index_node, 0, 1);
  /* Scan through the directory file until we reach
     the end of the directory file. */
  while (file_position.file_position < index_node->size)
  {
    entry = (dir_entry_t *)ramdisk_get_memory_address(&file_position);
    if (NULL == entry)
    {
      break;
    }
    /* Add the file position by the size of the directory entry,
       whose value is 16. */
    ramdisk_file_position_add(&file_position, sizeof(dir_entry_t));
    /* If we find a child directory entry whose name is the same as the specified name,
       then return the found directory entry. */
    if (NULL == filename_end)
    {
      if (0 == strcmp_ramdisk(filename_start, entry->filename))
      {
        return entry;
      }
    }
    else
    {
      if (0 == strncmp_ramdisk(entry->filename, filename_start, filename_end))
      {
        return entry;
      }
    }
  }

  return NULL;
}

/* This function find an empty child directory entry
   from its parent directory file index node.*/
dir_entry_t *ramdisk_get_empty_entry(index_node_t *index_node)
{
  dir_entry_t *entry = NULL;
  file_position_t file_position;

  /* Init the file position data structure. */
  ramdisk_file_position_init(&file_position, index_node, 0, 1);
  /* Scan through the directory file until we reach
     the end of the directory file. */
  while (file_position.file_position < index_node->size)
  {
    entry = (dir_entry_t *)ramdisk_get_memory_address(&file_position);
    if (NULL == entry)
    {
      break;
    }
    /* Add the file position by the size of the directory entry,
       whose value is 16. */
    ramdisk_file_position_add(&file_position, sizeof(dir_entry_t));
    /* If we find an empty child directory entry,
       then return the found directory entry. */
    if (0 == strcmp_ramdisk(entry->filename, ""))
    {
      return entry;
    }
  }

  return NULL;
}


/* This function initialize the file position's data structure. */
void ramdisk_file_position_init(file_position_t *file_position,
  index_node_t *index_node,
  int pos,
  int is_read_mode)
{
  int block_number = 0;

  /* get the correspond current block number of the file position */
  block_number = pos / BLK_SZ;
  /* Set the file position */
  file_position->file_position = pos;
  /* Initialize the current block's block pointer. */
  ramdisk_block_pointer_init(&file_position->block_pointer,
    index_node,
    block_number,
    is_read_mode);
  /* Initialize the data's current offset inside the block. */
  file_position->data_offset_in_block = pos % BLK_SZ;
}

/* This function add the file position by a specified offset, offser. */
void ramdisk_file_position_add(file_position_t *file_position, int offset)
{
  /* add the file position by a specified offset, offser. */
  file_position->file_position = file_position->file_position + offset;
  file_position->data_offset_in_block = file_position->data_offset_in_block + offset;
  /* If the data's current offset inside the block exceed the block's size,
     then increase the current block's block pointer, */
  if (file_position->data_offset_in_block >= BLK_SZ)
  {
    ramdisk_block_pointer_increase(&file_position->block_pointer);
    file_position->data_offset_in_block = file_position->data_offset_in_block - BLK_SZ;
  }
}

/* This function return the correspond memory address of a file position.
   When we use the file position for writing data,
   this function will allocate the neccesary block memory
   for storing the block pointer and for storing the file's data.*/
char *ramdisk_get_memory_address(file_position_t *file_position)
{
  int block_pointer = 0;

  /* Get the block pointer value of the correspond block we want to read data from or write data to. */
  block_pointer = ramdisk_alloc_and_get_block_pointer(&file_position->block_pointer);
  if (block_pointer <= 0)
  {
    return NULL;
  }
  /* Return the correspond memory address of a file position. */
  return ramdisk_get_block_memory_address(block_pointer) + file_position->data_offset_in_block;
}
  
/* This function initialize the block pointer data structure. */
void ramdisk_block_pointer_init(block_pointer_t *block_pointer,
  index_node_t *index_node,
  int block_number,
  int is_read_mode)
{
  memset_ramdisk(block_pointer, 0, sizeof(block_pointer_t));
  block_pointer->is_read_mode = is_read_mode;
  /* Initialize the current index node. */
  block_pointer->index_node = index_node;

  /* Initialize the direct block pointer. */
  if (block_number < DIRECT_BLOCK_POINTER_COUNT)
  {
    block_pointer->block_pointer_type = direct_block_pointer_type;
    block_pointer->direct_block_pointer = block_number;
  }
  /* Initialize the single-indirect block pointer. */
  else if (block_number < (DIRECT_BLOCK_POINTER_COUNT + PTRS_PB))
  {
    block_pointer->block_pointer_type = single_indirect_block_pointer_type;
    block_pointer->single_indirect_block_pointer = block_number - DIRECT_BLOCK_POINTER_COUNT;
  }
  /* Initialize the double-indirect block pointer. */
  else
  {
    block_pointer->block_pointer_type = double_indirect_block_pointer_type;
    block_pointer->double_indirect_block_pointer_row = (block_number - (DIRECT_BLOCK_POINTER_COUNT + PTRS_PB)) / PTRS_PB;
    block_pointer->double_indirect_block_pointer_column = (block_number - (DIRECT_BLOCK_POINTER_COUNT + PTRS_PB)) % PTRS_PB;
  }
}


/* This function increate the block pointer. */
void ramdisk_block_pointer_increase(block_pointer_t *block_pointer)
{
  /* Increase the direct block pointer. */
  if (direct_block_pointer_type == block_pointer->block_pointer_type)
  {
    block_pointer->direct_block_pointer++;
    /* If we reach the end of the direct block pointer,
       then we move to the beginning of the single-indirect block pointer*/
    if (DIRECT_BLOCK_POINTER_COUNT == block_pointer->direct_block_pointer)
    {
      block_pointer->block_pointer_type = single_indirect_block_pointer_type;
      block_pointer->single_indirect_block_pointer = 0;
    }
  }
  /* Increase the single-direct block pointer. */
  else if (single_indirect_block_pointer_type == block_pointer->block_pointer_type)
  {
    block_pointer->single_indirect_block_pointer++;
    /* If we reach the end of the single-indirect block pointer,
       then we move to the beginning of the double-indirect block pointer*/
    if (PTRS_PB == block_pointer->single_indirect_block_pointer)
    {
      block_pointer->block_pointer_type = double_indirect_block_pointer_type;
      block_pointer->double_indirect_block_pointer_row = 0;
      block_pointer->double_indirect_block_pointer_column = 0;
    }
  }
  /* Increase the double-direct block pointer. */
  else if (double_indirect_block_pointer_type == block_pointer->block_pointer_type)
  {
    block_pointer->double_indirect_block_pointer_column++;
    /* If we reach the end of the current column of the two dimension double-indirect
       block pointer, then we move to the beginning of the next column
       of the two dimension double-indirec block pointer. */
    if (PTRS_PB == block_pointer->double_indirect_block_pointer_column)
    {
      block_pointer->double_indirect_block_pointer_row++;
      block_pointer->double_indirect_block_pointer_column = 0;
    }
  }
}

/* When we use the block pointer for writing data,
   this function will allocate the neccesary block memory
   for storing the block pointer and for storing the file's data.
   This function will eventually return the block pointer value
   of the correspond block we want to read data from or write data to. */
int ramdisk_alloc_and_get_block_pointer(block_pointer_t *block_pointer)
{
  int block_pointer_index = 0;
  int *location = NULL;

  location = block_pointer->index_node->location;
  if (single_indirect_block_pointer_type == block_pointer->block_pointer_type)
  {
    /* When we use the block pointer for writing data,
       this function will allocate the neccesary block memory
       for storing the single-indirect block pointer
       when the type of the block pointer is single-indirect block pointer. */
    if (0 == location[SINGLE_INDIRECT_BLOCK_POINTER])
    {
      if (block_pointer->is_read_mode)
      {
        return -1;
      }
      else
      {
        /* If we fail in allocate the neccesary block memory,
           then this function will return -1. */
        location[SINGLE_INDIRECT_BLOCK_POINTER] = ramdisk_block_calloc();
        if (location[SINGLE_INDIRECT_BLOCK_POINTER] <= 0)
        {
          return -1;
        }
      }
    }
    location = (int *)ramdisk_get_block_memory_address(location[SINGLE_INDIRECT_BLOCK_POINTER]);
  }
  else if (double_indirect_block_pointer_type == block_pointer->block_pointer_type)
  {
    /* When we use the block pointer for writing data,
       this function will allocate the neccesary block memory
       for storing the double-indirect block pointer
       when the type of the block pointer is double-indirect block pointer. */
    if (0 == location[DOUBLE_INDIRECT_BLOCK_POINTER])
    {
      if (block_pointer->is_read_mode)
      {
        return -1;
      }
      else
      {
        /* If we fail in allocate the neccesary block memory,
           then this function will return -1. */
        location[DOUBLE_INDIRECT_BLOCK_POINTER] = ramdisk_block_calloc();
        if (location[DOUBLE_INDIRECT_BLOCK_POINTER] <= 0)
        {
          return -1;
        }
      }
    }
    location = (int *)ramdisk_get_block_memory_address(location[DOUBLE_INDIRECT_BLOCK_POINTER]);
    if (0 == location[block_pointer->double_indirect_block_pointer_row])
    {
      if (block_pointer->is_read_mode)
      {
        return -1;
      }
      else
      {
        /* If we fail in allocate the neccesary block memory,
           then this function will return -1. */
        location[block_pointer->double_indirect_block_pointer_row] = ramdisk_block_calloc();
        if (location[block_pointer->double_indirect_block_pointer_row] <= 0)
        {
          return -1;
        }
      }
    }
    location = (int *)ramdisk_get_block_memory_address(location[block_pointer->double_indirect_block_pointer_row]);
  }

  if (direct_block_pointer_type == block_pointer->block_pointer_type)
  {
    block_pointer_index = block_pointer->direct_block_pointer;
  }
  else if (single_indirect_block_pointer_type == block_pointer->block_pointer_type)
  {
    block_pointer_index = block_pointer->single_indirect_block_pointer;
  }
  else if (double_indirect_block_pointer_type == block_pointer->block_pointer_type)
  {
    block_pointer_index = block_pointer->double_indirect_block_pointer_column;
  }
  /* When we use the block pointer for writing data,
     this function will allocate the neccesary block memory
     for storing the file's data. */
  if (0 == location[block_pointer_index])
  {
    if (block_pointer->is_read_mode)
    {
      return -1;
    }
    else
    {
      /* If we fail in allocate the neccesary block memory,
         then this function will return -1. */
      location[block_pointer_index] = ramdisk_block_malloc();
      if (location[block_pointer_index] <= 0)
      {
        return -1;
      }
    }
  }
  /* Return the block pointer value of the correspond block we want to read data from or write data to. */
  return location[block_pointer_index];
}

