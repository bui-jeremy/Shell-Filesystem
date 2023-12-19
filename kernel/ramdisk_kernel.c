#include "ramdisk_kernel.h"
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/kernel.h>


static unsigned char *ramdisk_memory;

#define NULL 0


// parse to next directory in file path 
const char *find_next_directory(const char *str)
{
  const char *curr_str = NULL;

  curr_str = str;
  while ('\0' != curr_str[0])
  {
    if (('/' == curr_str[0]))
    {
      return curr_str;
    }
    curr_str++;
  }

  return NULL;
}

// initialize ramdisk memory
void ramdisk_init()
{
  int x = 0;
  int i = 0;
  int j = 0;
  int index = 0;
  superblock_t *superblock = NULL;
  index_node_t *index_node_array = NULL;
  unsigned char *block_bitmap = NULL;
  printk(KERN_INFO "ramdisk: Initializing ramdisk\n");
  // create 2MB of memory for ramdisk
  ramdisk_memory = (unsigned char *)vmalloc(sizeof(unsigned char) * RAMDISK_MEMORY_SIZE);
  
  // initialize superblock
  superblock = (superblock_t *)ramdisk_memory;
  memset(superblock, 0, BLK_SZ);
  superblock->num_free_blocks = RAMDISK_MEMORY_SIZE / BLK_SZ;
  superblock->num_free_index_nodes = MAX_INDEX_NODES_COUNT;
  // initialize root directory
  strcpy(superblock->first_block.type, "dir");

  // initialize index node array
  index_node_array = ramdisk_get_index_node(1);
  memset(index_node_array, 0, sizeof(unsigned char) * (BLK_SZ * INDEX_NODE_ARRAY_BLOCK_COUNT));

  // initialize block bitmap
  block_bitmap = (ramdisk_memory + BLK_SZ * (1 + INDEX_NODE_ARRAY_BLOCK_COUNT));
  for (i = 0; i < BLOCK_BITMAP_BLOCK_COUNT; i++)
  {
    for (j = 0; j < BLK_SZ; j++)
    {
      block_bitmap[index++] = 0x0FF;
    }
  }

  // initialize block bitmap for the superblock, index node array, and block bitmap itself to used
  for (x = 0; x < (1 + INDEX_NODE_ARRAY_BLOCK_COUNT + BLOCK_BITMAP_BLOCK_COUNT); x++)
  {
    search_bitmap();
  }
  printk(KERN_INFO "ramdisk: Finished initializing ramdisk\n");
}


void ramdisk_uninit()
{
  if (NULL != ramdisk_memory)
  {
    vfree(ramdisk_memory);
    ramdisk_memory = NULL;
  }
}

// create file with absolute pathname from root of directory tree
int ramdisk_create(char *pathname, char *type)
{
  printk(KERN_INFO "ramdisk: Creating file %s\n", pathname);
  int index_node_number = 0, updated_parent = -1;
  index_node_t *parent_directory_index_node = NULL;
  index_node_t *index_node = NULL;
  const char *filename = NULL;
  dir_entry_t entry;

  // iterrate through path up until parent
  parent_directory_index_node = ramdisk_get_directory_index_node(pathname);
  if (NULL == parent_directory_index_node)
  {
    return -1;
  }

  // use strrchr to find the last split of '/' for child directory
  filename = strrchr(pathname, '/');
  if (ramdisk_get_dir_entry(parent_directory_index_node, filename, NULL) != NULL)
  {
    return -1;
  }


  // find an unused index node
  index_node_number = ramdisk_get_unused_index_node();
  if (-1 == index_node_number)
  {
    return -1;
  }
  // return the block from index node array 
  index_node = ramdisk_get_index_node(index_node_number);
  memset(index_node, 0, sizeof(index_node_t));
  // set type to file
  strcpy(index_node->type, type);
  printk(KERN_INFO "Created directory entry at index node %d\n", index_node_number);

 
  strcpy(entry.filename, filename);
  entry.index_node_number = index_node_number;

  // update parent directory to include file directory
  updated_parent = ramdisk_update_parent_directory_file(parent_directory_index_node, &entry);
  if (updated_parent != 0)
  {
    return -1;
  }
  printk(KERN_INFO "ramdisk: Finished creating file %s\n", pathname);
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
  if ((0 == strcmp("", pathname)) || (0 == strcmp("/", pathname)))
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
  filename = strrchr(pathname, '/');
  entry = ramdisk_get_dir_entry(parent_directory_index_node, filename, NULL);
  if (NULL == entry)
  {
    return -1;
  }
  /* If unlink a non-empty directory file,
     this function will return -1 to indicate fail. */
  index_node = ramdisk_get_index_node(entry->index_node_number);
  if ((0 == strcmp("dir", index_node->type))
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
  memset(index_node, 0, sizeof(index_node_t));
  superblock = (superblock_t *)ramdisk_memory;
  superblock->num_free_index_nodes++;

  /* remove the correspond directory entry from the parent directory. */
  memset(entry, 0, sizeof(dir_entry_t));
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
  if ((0 == strcmp("", pathname))
    || (0 == strcmp("/", pathname)))
  {
    *index_node_number = 0;
    /* Increase the index node's open counter. */
    superblock = (superblock_t *)ramdisk_memory;
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
  filename = strrchr(pathname, '/');
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
  if (0 != strcmp("reg", index_node->type))
  {
    return -1;
  }
  /* check whether the num_bytes exceed the remainder data length
     in this file. */
  num_bytes = min(num_bytes, index_node->size - pos);
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
    data_length_to_read_once = min(remainder_data_length_in_block, remainder_data_length_to_read);
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

  // type of index node is directory file
  index_node = ramdisk_get_index_node(index_node_number);
  if (0 != strcmp("reg", index_node->type))
  {
    return -1;
  }
  /* The file size should not exceed
     256 * 8 + 64 * 256 + 64^2 * 256 = 1067008 bytes. */
  num_bytes = min(num_bytes, MAX_FILE_SIZE - pos);
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
    data_length_to_write_once = min(remainder_space_in_block, remainder_data_length_to_write);
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
  if (0 != strcmp("reg", index_node->type))
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

// absolute file path from root of directory tree
int ramdisk_mkdir(char *pathname)
{
  return ramdisk_create(pathname, "dir");
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
  if (0 != strcmp("dir", index_node->type))
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
    if (0 != strcmp(entry->filename, ""))
    {
      memcpy(address, entry, sizeof(dir_entry_t));
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
    location = (int *)(ramdisk_memory + (BLK_SZ * (index_node->location[DOUBLE_INDIRECT_BLOCK_POINTER])));
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

// return memory address of inode array
index_node_t *ramdisk_get_index_node(int index_node_number)
{
  superblock_t *superblock = NULL;
  index_node_t *index_node_array = NULL;

  // if the inode is 0 then we are just at root directory
  if (index_node_number <= 0)
  {
    superblock = (superblock_t *)ramdisk_memory;
    return &superblock->first_block;
  }

  // if not then return the memory address of the inode
  index_node_array = (index_node_t *)(ramdisk_memory + BLK_SZ);
  return index_node_array + (index_node_number - 1);
}

/* This function find and return one unused index node,
   if there has no unused index node, then return -1. */
int ramdisk_get_unused_index_node()
{
  int loop = 0;
  index_node_t *index_node_array = NULL;
  superblock_t *superblock = NULL;

  // first check superblock for unused inode
  superblock = (superblock_t *)ramdisk_memory;
  if (superblock->num_free_index_nodes <= 0)
  {
    return -1;
  }
  index_node_array = ramdisk_get_index_node(1);
  for (loop = 0; loop < MAX_INDEX_NODES_COUNT; loop++)
  {
    /* If the index node's type is an empty string,
       then it indicate that this index node is an unused index node. */
    if (0 == strcmp(index_node_array[loop].type, ""))
    {
      superblock->num_free_index_nodes--;
      return (loop + 1);
    }
  }

  /* if there has no unused index node, then return -1. */
  return -1;
}

// find free block and allocate it in bitmap
int search_bitmap() {
  int blockPointer = 0;
  int blockIndex = 0;
  int byteIndex = 0;
  int bitIndex = 0;
  unsigned char bitMask = 0;
  superblock_t *superblock = NULL;
  unsigned char *blockBitmap = NULL;

  // Pointers to superblock and block bitmap
  superblock = (superblock_t *)ramdisk_memory;
  blockBitmap = (ramdisk_memory + BLK_SZ * (1 + INDEX_NODE_ARRAY_BLOCK_COUNT));
  // Loop through the block bitmap to find a free block
  for (byteIndex = 0; byteIndex < BLOCK_BITMAP_BLOCK_COUNT; byteIndex++) {
    for (blockIndex = 0; blockIndex < BLK_SZ; blockIndex++) {
      bitMask = 1;
      for (bitIndex = 0; bitIndex < 8; bitIndex++) {
        // Check if the bit indicates a free block
        if ((blockBitmap[blockPointer] & bitMask) != 0) {
          // Mark the block as allocated and update free block count
          blockBitmap[blockPointer] = blockBitmap[blockPointer] - bitMask;
          superblock->num_free_blocks--;
          return blockPointer;
        }
        blockPointer++;
        bitMask = bitMask << 1; // Shift the bit mask to check the next bit
      }
    }
  }
  return -1;
}


// allocate_free_block
int ramdisk_block_calloc()
{
  int block_index = 0;

  block_index = search_bitmap();
  if (block_index > 0)
  {
    /* clear its all memory to zero. */
    memset((ramdisk_memory + (BLK_SZ * block_index)), 0, BLK_SZ);
  }

  return block_index;
}

// allocate block to free status
void ramdisk_block_free(int block_pointer)
{
  int index = 0;
  int k = 0;
  int mask = 0;
  superblock_t *superblock = NULL;
  unsigned char *block_bitmap = NULL;

  superblock = (superblock_t *)ramdisk_memory;
  block_bitmap = (ramdisk_memory + BLK_SZ * (1 + INDEX_NODE_ARRAY_BLOCK_COUNT));

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

// add entry to parent directory index node
int ramdisk_update_parent_directory_file(index_node_t *index_node, dir_entry_t *entry) {
  char *dst = NULL;
  file_position_t file_position;
  dir_entry_t *empty_entry = NULL;

  // check size of parent index node
  if ((index_node->size + sizeof(dir_entry_t)) > MAX_FILE_SIZE) {
    return -1; // Parent directory file is full.
  }

  // Init the file position data structure.
  ramdisk_file_position_init(&file_position, index_node, 0, 1);

  // scan through the directory file until an empty entry is found or the end is reached.
  while (file_position.file_position < index_node->size) {
    empty_entry = (dir_entry_t *)ramdisk_get_memory_address(&file_position);
    if (NULL == empty_entry) {
      break;
    }
    ramdisk_file_position_add(&file_position, sizeof(dir_entry_t));
    
    // If an empty child directory entry is found, use it for the new entry.
    if (0 == strcmp(empty_entry->filename, "")) {
      memcpy(empty_entry, entry, sizeof(dir_entry_t));
      index_node->dir_entry_count++;
      return 0;
    }
  }

  // now that we have memory address add it to the end of the parent directory file
  ramdisk_file_position_init(&file_position, index_node, index_node->size, 0);
  dst = ramdisk_get_memory_address(&file_position);
  if (NULL == dst) {
    return -1; // Unable to retrieve memory address.
  }

  // Append the directory entry to the end of the parent directory file.
  memcpy(dst, entry, sizeof(dir_entry_t));
  index_node->size = index_node->size + sizeof(dir_entry_t);
  index_node->dir_entry_count++;

  return 0;
}

// find directory index node
index_node_t *ramdisk_get_directory_index_node(const char *pathname)
{
  index_node_t *index_node = NULL;
  const char *filename_start = NULL;
  const char *filename_end = NULL;
  dir_entry_t *entry = NULL;

  index_node = &((superblock_t *)ramdisk_memory)->first_block;
  filename_start = pathname;
  // omit first '/'
  if ('/' == filename_start[0])
  {
    filename_start = filename_start + 1;
  }
  while (1)
  {
    // cycle through path name to make sure each directory exists in specified
    filename_end = find_next_directory(filename_start);
    if (NULL == filename_end)
    {
      break;
    }
    // find the parent path checking each directory down file path
    entry = ramdisk_get_dir_entry(index_node, filename_start, filename_end);
    if (NULL == entry)
    {
      return NULL;
    }
    
    // keep traversing down path of directory
    index_node = ramdisk_get_index_node(entry->index_node_number);
    if (strcmp(index_node->type, "dir"))
    {
      return NULL;
    }

    filename_start = filename_end + 1;
  }

  return index_node;
}

// find child entry from parent directory
dir_entry_t *ramdisk_get_dir_entry(index_node_t *index_node, const char *filename_start, const char *filename_end)
{
  dir_entry_t *entry = NULL;
  file_position_t file_position;

  ramdisk_file_position_init(&file_position, index_node, 0, 1);
  
  // scan through until we reach end
  while (file_position.file_position < index_node->size)
  {
    // find memory block after we allocate it in the right file position
    entry = (dir_entry_t *)ramdisk_get_memory_address(&file_position);
    if (NULL == entry)
    {
      break;
    }
    ramdisk_file_position_add(&file_position, sizeof(dir_entry_t));
    /* If we find a child directory entry whose name is the same as the specified name,
       then return the found directory entry. */
    if (NULL == filename_end)
    {
      if (0 == strcmp(filename_start, entry->filename))
      {
        return entry;
      }
    }
    else
    {
      if (0 == strncmp(entry->filename, filename_start, filename_end))
      {
        return entry;
      }
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

// find memory address of the block pointer
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
  return (ramdisk_memory + (BLK_SZ * block_pointer))+ file_position->data_offset_in_block;
}
  
/* This function initialize the block pointer data structure. */
void ramdisk_block_pointer_init(block_pointer_t *block_pointer,index_node_t *index_node,int block_number,int is_read_mode)
{
  memset(block_pointer, 0, sizeof(block_pointer_t));
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

// allocate block and return address
int ramdisk_alloc_and_get_block_pointer(block_pointer_t *block_pointer)
{
  int block_pointer_index = 0;
  int *location = NULL;

  location = block_pointer->index_node->location;
  // check single indirect
  if (single_indirect_block_pointer_type == block_pointer->block_pointer_type)
  {
    if (0 == location[SINGLE_INDIRECT_BLOCK_POINTER])
    {
        location[SINGLE_INDIRECT_BLOCK_POINTER] = ramdisk_block_calloc();
        if (location[SINGLE_INDIRECT_BLOCK_POINTER] <= 0)
        {
          return -1;
        }
    }
    location = (int *)(ramdisk_memory + (BLK_SZ * location[SINGLE_INDIRECT_BLOCK_POINTER]));
  }
  // check double indirect
  else if (double_indirect_block_pointer_type == block_pointer->block_pointer_type)
  {

    if (0 == location[DOUBLE_INDIRECT_BLOCK_POINTER])
    {
      if (block_pointer->is_read_mode)
      {
        return -1;
      }
      else
      {
        int block_index = 0;
        block_index = search_bitmap();
        if (block_index > 0)
        {
          memset((ramdisk_memory + (BLK_SZ * block_index)), 0, BLK_SZ);
        }
        location[DOUBLE_INDIRECT_BLOCK_POINTER] = block_index;
        if (location[DOUBLE_INDIRECT_BLOCK_POINTER] <= 0)
        {
          return -1;
        }
      }
    }
    location = (int *)(ramdisk_memory + (BLK_SZ * location[DOUBLE_INDIRECT_BLOCK_POINTER]));
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
    location = (int *) (ramdisk_memory + (location[block_pointer->double_indirect_block_pointer_row]));
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
      location[block_pointer_index] = search_bitmap();
      if (location[block_pointer_index] <= 0)
      {
        return -1;
      }
    }
  }
  /* Return the block pointer value of the correspond block we want to read data from or write data to. */
  return location[block_pointer_index];
}