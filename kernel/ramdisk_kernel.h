



#ifndef _RAMDISK_KERNEL_H
#define _RAMDISK_KERNEL_H



#define RAMDISK_MEMORY_SIZE         (2 * 1024 * 1024)
#define MAX_INDEX_NODES_COUNT       1024
#define BLK_SZ 256		/* Block size */
#define PTR_SZ 4		/* 32-bit [relative] addressing */
#define PTRS_PB  (BLK_SZ / PTR_SZ) /* Pointers per index block */
#define INDEX_NODE_ARRAY_BLOCK_COUNT    256
#define BLOCK_BITMAP_BLOCK_COUNT        4

#define DIRECT_BLOCK_POINTER_COUNT               8
#define SINGLE_INDIRECT_BLOCK_POINTER_COUNT      1
#define DOUBLE_INDIRECT_BLOCK_POINTER_COUNT      1

#define SINGLE_INDIRECT_BLOCK_POINTER            (DIRECT_BLOCK_POINTER_COUNT)
#define DOUBLE_INDIRECT_BLOCK_POINTER            (SINGLE_INDIRECT_BLOCK_POINTER + SINGLE_INDIRECT_BLOCK_POINTER_COUNT)


#define MAX_BLOCK_COUNT_IN_FILE   (DIRECT_BLOCK_POINTER_COUNT + PTRS_PB + PTRS_PB * PTRS_PB)
#define MAX_FILE_SIZE   (MAX_BLOCK_COUNT_IN_FILE * BLK_SZ)


/* The index node's data structure */
typedef struct _index_node
{
  /* type (4 bytes) -- either "dir" for directory or "reg" for regular files. */
  char type[4];
  /* size (4 bytes) -- the current size of the corresponding file in bytes.
     For directories, this will be the size of all entries in the corresponding
     directory file, where each entry's size is the number of bytes needed to
     record a (filename,index_node_number) pair */
  int size;
  /* location (40 bytes) -- this attribute member identifies the
     blocks (potentially) scattered within the ramdisk that store
     the file's contents.
     The location attribute of a file contains 10 block pointers of 4-bytes each:
     the first 8 pointers are direct block pointers,
     the 9th pointer is a single-indrect block pointer,
     and the 10th pointer is a double-indirect block pointer.
     This means that the maximum size of asingle file will be:
     256 * 8 + 64 * 256 + 64^2 * 256 = 1067008 bytes.
     */
  int location[10];
  /* dir_entry_count (4 bytes) -- If the index node's type is a directory,
     this field will record the total directory entry count in this directory. */
  int dir_entry_count;
  /* open_counter (4 bytes) -- indicate whether this index node is open. */
  int open_counter;
  /* The padding bytes will make the size of this struct to 64 bytes. */
  char padding[8];

} index_node_t;

/* The superblock's data structure */
typedef struct _superblock
{
  /* the number of free blocks that can be allocated for storing
     directory and regular file contents. */
  int num_free_blocks;
  /* the number of free index nodes that can be associated with
     newly created files. Note that each file in the system
     will have its own index node*/
  int num_free_index_nodes;
  /* The superblock will contain the root directory's index node. */
  index_node_t first_block;

} superblock_t;


/* The directory entry's data structure for the directory index node. */
typedef struct _dir_entry
{
  char filename[14];
  short index_node_number;
} dir_entry_t;

/* Three type of block pointer: direct block pointers,
   single-indrect block pointer and double-indirect block pointer. */
typedef enum _block_pointer_type
{
  direct_block_pointer_type = 1,
  single_indirect_block_pointer_type = 2,
  double_indirect_block_pointer_type = 3,
} block_pointer_type_t;


/* The block pointer's data structure. */
typedef struct _block_pointer
{
  /* When we use the block pointer to read data,
     we will not create new blocks to store
     indirect block pointers. */
  int is_read_mode;
  /* The block pointer's type. */
  block_pointer_type_t block_pointer_type;
  /* The direct block pointer index number (0-7). */
  int direct_block_pointer;
  /* The single-indirect block pointer index number (0-63). */
  int single_indirect_block_pointer;
  /* The row index of the two dimension double-indirect
     block pointer index number (0-63). */
  int double_indirect_block_pointer_row;
  /* The column index of the two dimension double-indirect
     block pointer index number (0-63). */
  int double_indirect_block_pointer_column;
  /* The current index node for us to read or write data. */
  index_node_t *index_node;

} block_pointer_t;

/* The file position's data structure. */
typedef struct _file_position
{
  /* The file position */
  int file_position;
  /* The data's current offset inside the block. */
  int data_offset_in_block;
  /* The current block's block pointer. */
  block_pointer_t block_pointer;
} file_position_t;

typedef struct _pathname
{
  int pathname_length;
  const char *pathname;

}pathname_t;

/* The parameter for the ioctl function
   to create regular file, create directory
   and remove the file from the ramdisk, */
typedef struct _creat_param
{
  /* The return value of the ramdisk kernel module. */
  int return_value;
  pathname_t pathname;

} creat_param_t;

/* The parameter for the ioctl function
   to open an existing file corresponding to pathname. */
typedef struct _open_param
{
  /* The return value of the ramdisk kernel module. */
  int return_value;
  pathname_t pathname;
  /* The index node's index node number
     return by the ramdisk kernel module. */
  int index_node_number;

} open_param_t;


/* The parameter for the ioctl function
to create regular file, create directory
and remove the file from the ramdisk, */
typedef struct _close_param
{
  /* The return value of the ramdisk kernel module. */
  int return_value;
  /* The index node's index node number */
  int index_node_number;

} close_param_t;

/* The parameter for the ioctl function
   to read data from the ramdisk's file
   or write data to the ramdisk's file, */
typedef struct _read_write_param
{
  /* The return value of the ramdisk kernel module. */
  int return_value;
  /* The index node's index node number */
  int index_node_number;
  /* The start file position to
     read data from or write data to. */
  int file_position;
  /* The start address of the data in the calling process. */
  char *address;
  /* The size of data to read or to write. */
  int num_bytes;

} read_write_param_t;

/* The parameter for the ioctl function
   to set the file object's file position
   identified by file descriptor fd, to offset. */
typedef struct _lseek_param
{
  /* The return value of the ramdisk kernel module. */
  int return_value;
  /* The index node's index node number */
  int index_node_number;
  /* The offset to set to. */
  int seek_offset;
  /* The offset returned by the ramdisk kernel module. */
  int seek_result_offset;

} lseek_param_t;


/* The parameter for the ioctl function
   to read one entry from a directory file identified by
   index node number index_node_number, and store the result
   in memory at the specified value of address. */
typedef struct _readdir_param
{
  /* The return value of the ramdisk kernel module. */
  int return_value;
  /* The index node's index node number */
  int index_node_number;
  /* The start file position to
     read one directory entry from. */
  int file_position;
  /* The data length of one directory entry
     return by the ramdisk kernel module (The value is 16).*/
  int dir_data_length;
  /* The data of the directory entry
     read from the ramdisk kernel module. */
  char address[16];

} readdir_param_t;


#define IOCTL_CREAT _IOWR(0, 1, creat_param_t)
#define IOCTL_UNLINK _IOWR(0, 2, creat_param_t)
#define IOCTL_OPEN _IOWR(0, 3, open_param_t)
#define IOCTL_CLOSE _IOWR(0, 4, close_param_t)
#define IOCTL_READ _IOWR(0, 5, read_write_param_t)
#define IOCTL_WRITE _IOWR(0, 6, read_write_param_t)
#define IOCTL_LSEEK _IOWR(0, 7, lseek_param_t)
#define IOCTL_MKDIR _IOWR(0, 8, creat_param_t)
#define IOCTL_READDIR _IOWR(0, 9, readdir_param_t)


/* This function is the initialize function of the ramdisk. */
void ramdisk_init(void);
/* This function free the 2MB memory of the ramdisk allocated by call the vmalloc function. */
void ramdisk_uninit(void);
/* This function return the length of the directory entry data structure, which is the value 16. */
int ramdisk_get_dir_entry_length(void);

/* This function Freeing the file index node's memory in the ramdisk. */
void ramdisk_free_index_node_memory(index_node_t *index_node);

/* This function return the superblock's memory address in the ramdisk. */
superblock_t *ramdisk_get_superblock(void);
/* This function return the memory address of
   one index node number specified by the index_node_number in the ramdisk. */
index_node_t *ramdisk_get_index_node(int index_node_number);
/* This function return the memory address of
   the beginning of the "block bitmap". */
unsigned char *ramdisk_get_block_bitmap(void);
/* This function return the memory address of
   one block specified by the block_pointer in the ramdisk. */
char *ramdisk_get_block_memory_address(int block_pointer);

/* This function find and return one unused index node,
   if there has no unused index node, then return -1. */
int ramdisk_get_unused_index_node(void);

/* This function allocate one free block,
   if there has no free block, then return -1. */
int ramdisk_block_malloc(void);
/* This function allocate one free block and clear its all memory to zero,
   if there has no free block, then return -1. */
int ramdisk_block_calloc(void);
/* This function set one allocated used block to free status,
   and free it to the free block list. */
void ramdisk_block_free(int block_pointer);

/* This function add an directory entry to its parent directory file index node. */
int ramdisk_update_parent_directory_file(index_node_t *index_node, dir_entry_t *entry);
/* This function find the directory file index node
   of a specified pathname. If the directory does not exists,
   then return NULL. */
index_node_t *ramdisk_get_directory_index_node(const char *pathname);
/* This function find the child directory entry by a specified name
   from its parent directory file index node.*/
dir_entry_t *ramdisk_get_dir_entry(index_node_t *index_node, const char *filename_start, const char *filename_end);
/* This function find an empty child directory entry
   from its parent directory file index node.*/
dir_entry_t *ramdisk_get_empty_entry(index_node_t *index_node);

/* This function initialize the file position's data structure. */
void ramdisk_file_position_init(file_position_t *file_position,
  index_node_t *index_node,
  int pos,
  int is_read_mode);
/* This function add the file position by a specified offset, offser. */
void ramdisk_file_position_add(file_position_t *file_position, int offset);
/* This function return the correspond memory address of a file position.
   When we use the file position for writing data,
   this function will allocate the neccesary block memory
   for storing the block pointer and for storing the file's data.*/
char *ramdisk_get_memory_address(file_position_t *file_position);

/* This function initialize the block pointer data structure. */
void ramdisk_block_pointer_init(block_pointer_t *block_pointer,
  index_node_t *index_node,
  int block_number,
  int is_read_mode);
/* This function increate the block pointer. */
void ramdisk_block_pointer_increase(block_pointer_t *block_pointer);
/* When we use the block pointer for writing data,
   this function will allocate the neccesary block memory
   for storing the block pointer and for storing the file's data.
   This function will eventually return the block pointer value
   of the correspond block we want to read data from or write data to. */
int ramdisk_alloc_and_get_block_pointer(block_pointer_t *block_pointer);

/* This function create a regular file with absolute pathname
   from the root of the directory tree,
   where each directory filename is delimited by a "/" character.
   */
int ramdisk_creat(char *pathname);

/* This function remove the file from the ramdisk with absolute
   pathname from the filesystem, freeing its memory in the ramdisk. */
int ramdisk_unlink(char *pathname);

/* This function open an existing file corresponding to pathname. */
int ramdisk_open(char *pathname, int *index_node_number);

/* This function close the corresponding file. */
int ramdisk_close(int index_node_number);

/* This function read up to num_bytes from a regular file
   to the specified address in the calling process. */
int ramdisk_read(int index_node_number, int file_position, char *address, int num_bytes);

/* This function write up to num_bytes from the specified address
   in the calling process to a regular file. */
int ramdisk_write(int index_node_number, int file_position, char *address, int num_bytes);

/* This function set the file object's file position
   identified by index node number index_node_number,
   to offset, returning the new position, or the end of the
   file position if the offset is beyond the file's current size. */
int ramdisk_lseek(int index_node_number, int seek_offset, int *seek_result_offset);

/* This function create a directory file with absolute pathname
   from the root of the directory tree,
   where each directory filename is delimited by a "/" character. */
int ramdisk_mkdir(char *pathname);

/* This function read one entry from a directory file identified by
   index node number index_node_number, and store the result
   in memory at the specified value of address. */
int ramdisk_readdir(int index_node_number, char *address, int *file_position);



#endif




