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
  printk(KERN_INFO "Initializing ramdisk\n");
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
  printk(KERN_INFO "Finished initializing ramdisk\n");
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
  printk(KERN_INFO "Creating file %s\n", pathname);
  int index_node_number = 0, updated_parent = -1;
  index_node_t *parent_directory_index_node = NULL;
  index_node_t *index_node = NULL;
  const char *filename = NULL;
  dir_entry_t entry;
  superblock_t *superblock = (superblock_t *)ramdisk_memory;
  index_node_t *index_node_array = ramdisk_get_index_node(1);
  char *dst = NULL;
  file_position_t file_position;
  dir_entry_t *empty_entry = NULL;

  // 1. iterate from root until the parent file node to find parent's inode
  parent_directory_index_node = ramdisk_get_directory_index_node(pathname);
  if (parent_directory_index_node == NULL)
  {
    return -1;
  }

  // 2. use strrchr to find the last split of '/' for child directory
  filename = strrchr(pathname, '/');
  if (ramdisk_get_dir_entry(parent_directory_index_node, filename, NULL) != NULL)
  {
    return -1;
  }

  // 3. find unused inode from inode array
  int i = 0;
  for (i = 0; i < MAX_INDEX_NODES_COUNT; i++)
  {
    if (0 == strcmp(index_node_array[i].type, ""))
    {
      superblock->num_free_index_nodes--;
      index_node_number = i + 1;
      break;
    }
    else if (i == MAX_INDEX_NODES_COUNT - 1)
    {
      return -1;
    }
  }

  // 4. find correlating block memory address for inode and fill in structures
  index_node = ramdisk_get_index_node(index_node_number);
  memset(index_node, 0, sizeof(index_node_t));
  strcpy(index_node->type, type);
  strcpy(entry.filename, filename);
  entry.index_node_number = index_node_number;
  printk(KERN_INFO "Created file %s, type %s, at index node %d\n", entry.filename, type, entry.index_node_number);

  // 5. find empty entry in parent directory and add new entry
  if ((parent_directory_index_node->size + sizeof(dir_entry_t)) > MAX_FILE_SIZE) {
    return -1; 
  }
  ramdisk_file_position_init(&file_position, parent_directory_index_node, 0, 1);
  // scan through the directory file until an empty entry is found or the end is reached.
  while (file_position.file_position < parent_directory_index_node->size) {
    empty_entry = (dir_entry_t *)ramdisk_get_memory_address(&file_position);
    if (NULL == empty_entry) {
      break;
    }
    ramdisk_file_position_add(&file_position, sizeof(dir_entry_t));
    
    // If an empty child directory entry is found, use it for the new entry.
    if (0 == strcmp(empty_entry->filename, "")) {
      memcpy(empty_entry, &entry, sizeof(dir_entry_t));
      parent_directory_index_node->dir_entry_count++;
      return 0;
    }
  }
  // now that we have memory address add it to the end of the parent directory file
  ramdisk_file_position_init(&file_position, parent_directory_index_node, parent_directory_index_node->size, 0);
  dst = ramdisk_get_memory_address(&file_position);
  if (NULL == dst) {
    return -1; 
  }
  memcpy(dst, &entry, sizeof(dir_entry_t));
  parent_directory_index_node->size = parent_directory_index_node->size + sizeof(dir_entry_t);
  parent_directory_index_node->dir_entry_count++;

  printk(KERN_INFO "Finished creating file %s\n", pathname);
  return 0;
}

// absolute file path from root of directory tree
int ramdisk_mkdir(char *pathname)
{
  return ramdisk_create(pathname, "dir");
}

// open file with absolute pathname from root of directory tree
int ramdisk_open(char *pathname, int *index_node_number)
{
  const char *filename = NULL;
  index_node_t *parent_directory_index_node = NULL;
  index_node_t *index_node = NULL;
  dir_entry_t *entry = NULL;
  superblock_t *superblock = NULL;
  if ((0 == strcmp("", pathname)) || (0 == strcmp("/", pathname)))
  {
    *index_node_number = 0;
    superblock = (superblock_t *)ramdisk_memory;
    superblock->first_block.open_counter++;
    return 0;
  }

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
  *index_node_number = entry->index_node_number;
  printk(KERN_INFO "Opened file %s at index node %d\n", pathname, *index_node_number);

  // increase the number of open entries at inode
  index_node = ramdisk_get_index_node(entry->index_node_number);
  index_node->open_counter++;

  return 0;
}


// free memory and remove the absolute file path 
int ramdisk_unlink(char *pathname)
{
  const char *filename = NULL;
  index_node_t *parent_directory_index_node = NULL;
  index_node_t *index_node = NULL;
  dir_entry_t *entry = NULL;
  superblock_t *superblock = NULL;
  int block_pointer_value = 0;
  block_pointer_t block_pointer;
  int loop = 0;

  // can not unlink root!
  if ((0 == strcmp("", pathname)) || (0 == strcmp("/", pathname)))
  {

    return -1;
  }
  
  // check parent file path
  parent_directory_index_node = ramdisk_get_directory_index_node(pathname);
  if (NULL == parent_directory_index_node)
  {
    return -1;
  }
  
  // check to for the child file
  filename = strrchr(pathname, '/');
  entry = ramdisk_get_dir_entry(parent_directory_index_node, filename, NULL);
  if (NULL == entry)
  {
    return -1;
  }
  
  // checking if it is directory, it has to be empty
  index_node = ramdisk_get_index_node(entry->index_node_number);
  if ((0 == strcmp("dir", index_node->type)) && (index_node->dir_entry_count > 0))
  {
    return -1;
  }

  // make sure file is not open
  if (index_node->open_counter > 0)
  {
    return -1;
  }

  // free the inode at the block memory
  ramdisk_block_pointer_init(&block_pointer, index_node, 0, 1);

  // Free memory block by block in the ramdisk
  for (loop = 0; loop < MAX_BLOCK_COUNT_IN_FILE; loop++) {
      block_pointer_value = ramdisk_alloc_and_get_block_pointer(&block_pointer);
      if (block_pointer_value <= 0) {
          break;
      }
      ramdisk_block_free(block_pointer_value);
      ramdisk_block_pointer_increase(&block_pointer);
  }

  // Free memory for single-indirect block pointer if it exists
  if (index_node->location[SINGLE_INDIRECT_BLOCK_POINTER] > 0) {
      ramdisk_block_free(index_node->location[SINGLE_INDIRECT_BLOCK_POINTER]);
  }

  // Free memory for double-indirect block pointer if it exists
  if (index_node->location[DOUBLE_INDIRECT_BLOCK_POINTER] > 0) {
      int *location = (int *)(ramdisk_memory + (BLK_SZ * (index_node->location[DOUBLE_INDIRECT_BLOCK_POINTER])));
      for (loop = 0; loop < PTRS_PB; loop++) {
          if (location[loop] <= 0) {
              break;
          }
          ramdisk_block_free(location[loop]);
      }
      ramdisk_block_free(index_node->location[DOUBLE_INDIRECT_BLOCK_POINTER]);
  }
  printk(KERN_INFO "Unlinked file at index node %d\n", entry->index_node_number);

  // clear location attribute and reset file attributes
  memset(index_node->location, 0, sizeof(index_node->location));
  index_node->size = 0;
  index_node->dir_entry_count = 0;

  // adjust data structures
  memset(index_node, 0, sizeof(index_node_t));
  superblock = (superblock_t *)ramdisk_memory;
  superblock->num_free_index_nodes++;
  memset(entry, 0, sizeof(dir_entry_t));
  parent_directory_index_node->dir_entry_count--;

  return 0;
}

// close fd table
int ramdisk_close(int index_node_number)
{
  index_node_t *index_node = NULL;

  // decrease number of open index nodes
  index_node = ramdisk_get_index_node(index_node_number);
  index_node->open_counter--;

  return 0;
}

// read number of bytes from a file
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

  // can not read directory file
  index_node = ramdisk_get_index_node(index_node_number);
  if (0 != strcmp("reg", index_node->type))
  {
    return -1;
  }
  // check if we are trying to read too much
  num_bytes = min(num_bytes, index_node->size - pos);
  /* Init the file position data structure. */
  ramdisk_file_position_init(&file_position, index_node, pos, 1);
  dst = address;
  remainder_data_length_to_read = num_bytes;
  // read block by block
  while (remainder_data_length_to_read > 0)
  {
    remainder_data_length_in_block = BLK_SZ - file_position.data_offset_in_block;
    // the data length to read once should not exceed the remainder space in one block.
    data_length_to_read_once = min(remainder_data_length_in_block, remainder_data_length_to_read);
    src = ramdisk_get_memory_address(&file_position);
    if (NULL == src)
    {
      break;
    }
    // copy the data from the ramdisk to the user space.
    copy_to_user(dst, src, data_length_to_read_once);
    data_length_read = data_length_read + data_length_to_read_once;
    dst = dst + data_length_to_read_once;
    remainder_data_length_to_read = remainder_data_length_to_read - data_length_to_read_once;
    ramdisk_file_position_add(&file_position, data_length_to_read_once);
  }
  return data_length_read;
}

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
  // check if we are trying to write too much
  num_bytes = min(num_bytes, MAX_FILE_SIZE - pos);
  if (num_bytes > 0)
  {
    ramdisk_file_position_init(&file_position, index_node, pos, 0);
  }
  src = address;
  remainder_data_length_to_write = num_bytes;
  
  while (remainder_data_length_to_write > 0)
  {
    remainder_space_in_block = BLK_SZ - file_position.data_offset_in_block;

    data_length_to_write_once = min(remainder_space_in_block, remainder_data_length_to_write);
    dst = ramdisk_get_memory_address(&file_position);
    if (NULL == dst)
    {
      break;
    }
  
    copy_from_user(dst, src, data_length_to_write_once);
    data_length_written = data_length_written + data_length_to_write_once;
    src = src + data_length_to_write_once;
    remainder_data_length_to_write = remainder_data_length_to_write - data_length_to_write_once;
    if (remainder_data_length_to_write > 0)
    {
      ramdisk_file_position_add(&file_position, data_length_to_write_once);
    }
  }
  index_node->size = (pos + data_length_written > index_node->size) ? (pos + data_length_written) : index_node->size;

  return data_length_written;
}

// seek to a position in a file
int ramdisk_lseek(int index_node_number, int seek_offset, int *seek_result_offset)
{
  index_node_t *index_node = NULL;

  // can not seek directory file
  index_node = ramdisk_get_index_node(index_node_number);
  if (0 != strcmp("reg", index_node->type))
  {
    return -1;
  }
  if (seek_offset < 0)
  {
    *seek_result_offset = 0;
  }
  // check if we are trying to seek too much and return end of file
  else if (seek_offset > index_node->size)
  {
    *seek_result_offset = index_node->size;
  }
  // return the seek offset
  else
  {
    *seek_result_offset = seek_offset;
  }

  return 0;
}

// read directory file and store in address
int ramdisk_readdir(int index_node_number, char *address, int *pos)
{
  dir_entry_t *entry = NULL;
  index_node_t *index_node = NULL;
  file_position_t file_position;

  index_node = ramdisk_get_index_node(index_node_number);
  if (0 != strcmp("dir", index_node->type))
  {
    return -1;
  }
  /* Init the file position data structure. */
  ramdisk_file_position_init(&file_position, index_node, *pos, 1);
  while (file_position.file_position < index_node->size)
  {
    entry = (dir_entry_t *)ramdisk_get_memory_address(&file_position);
    if (NULL == entry)
    {
      break;
    }
    ramdisk_file_position_add(&file_position, sizeof(dir_entry_t));

    if (0 != strcmp(entry->filename, ""))
    {
      memcpy(address, entry, sizeof(dir_entry_t));
      *pos = file_position.file_position;
      return 1;
    }
  }

  *pos = file_position.file_position;
  return 0;
}

// length of directory entry in bytes
int ramdisk_get_dir_entry_length()
{
  return ((int)sizeof(dir_entry_t));
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
  // check bitmap
  if (0 == (block_bitmap[index] & mask))
  {
    block_bitmap[index] = block_bitmap[index] | mask;
    superblock->num_free_blocks++;
  }
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


void ramdisk_file_position_init(file_position_t *file_position,index_node_t *index_node,int pos,int is_read_mode)
{
  int block_number = 0;

  /* get the correspond current block number of the file position */
  block_number = pos / BLK_SZ;
  /* Set the file position */
  file_position->file_position = pos;
  ramdisk_block_pointer_init(&file_position->block_pointer,index_node,block_number,is_read_mode);
  file_position->data_offset_in_block = pos % BLK_SZ;
}

// add offset to file position
void ramdisk_file_position_add(file_position_t *file_position, int offset)
{
  file_position->file_position = file_position->file_position + offset;
  file_position->data_offset_in_block = file_position->data_offset_in_block + offset;
  // if we reach end of block then increase block pointer
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

  block_pointer = ramdisk_alloc_and_get_block_pointer(&file_position->block_pointer);
  if (block_pointer <= 0)
  {
    return NULL;
  }
  return (ramdisk_memory + (BLK_SZ * block_pointer))+ file_position->data_offset_in_block;
}
  
void ramdisk_block_pointer_init(block_pointer_t *block_pointer,index_node_t *index_node,int block_number,int is_read_mode)
{
  memset(block_pointer, 0, sizeof(block_pointer_t));
  block_pointer->is_read_mode = is_read_mode;
  block_pointer->index_node = index_node;

  if (block_number < DIRECT_BLOCK_POINTER_COUNT)
  {
    block_pointer->block_pointer_type = direct_block_pointer_type;
    block_pointer->direct_block_pointer = block_number;
  }
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