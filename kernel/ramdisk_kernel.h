



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



typedef struct _index_node
{
  
  char type[4];
  int size;
  int location[10];
  int dir_entry_count;
  int open_counter;
  char padding[8]; //Padding bytes to ensure structure alignment

} index_node_t;


typedef struct _superblock
{
  int num_free_blocks;
  int num_free_index_nodes;
  index_node_t first_block;     // The root directory's index node. 

} superblock_t;


/* The directory entry's data structure for the directory index node. */
typedef struct _dir_entry
{
  char filename[14];   //13 charactor + '/0'
  short index_node_number;
} dir_entry_t;


typedef enum _block_pointer_type
{
  direct_block_pointer_type = 1,
  single_indirect_block_pointer_type = 2,
  double_indirect_block_pointer_type = 3,
} block_pointer_type_t;


typedef struct _block_pointer
{
  /* When we use the block pointer to read data,
     we will not create new blocks to store
     indirect block pointers. */
  int is_read_mode;
  
  block_pointer_type_t block_pointer_type;     
  int direct_block_pointer;                    // The direct block pointer index number (0-7). 
  int single_indirect_block_pointer;           //(0-63). 
  int double_indirect_block_pointer_row;       //row:0-63
  int double_indirect_block_pointer_column;    //column: 0-63
  
  index_node_t *index_node;                    // The current index node for us to read or write data. 

} block_pointer_t;


typedef struct _file_position
{
  int file_position;
  int data_offset_in_block;                     // The data's current offset inside the block. 
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
  int return_value;                               // The return value of the ramdisk kernel module. 
  pathname_t pathname;

} creat_param_t;

typedef struct _open_param
{
  int return_value;
  pathname_t pathname;
  /* The index node's index node number
     return by the ramdisk kernel module. */
  int index_node_number;

} open_param_t;

typedef struct _close_param
{
  int return_value;
  int index_node_number;

} close_param_t;

typedef struct _read_write_param
{
  int return_value;
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
  int return_value;
  int index_node_number;
  int seek_offset;
  int seek_result_offset;  //The new offset after operations

} lseek_param_t;


typedef struct _readdir_param
{
  int return_value;
  int index_node_number;
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


void ramdisk_init(void);
void ramdisk_uninit(void);
/* This function return the length of the directory entry data structure, which is the value 16. */
int ramdisk_get_dir_entry_length(void);

void ramdisk_free_index_node_memory(index_node_t *index_node);    //Freeing the file index node's memory in the ramdisk. 

/* These functions return memory addresses of ... */
superblock_t *ramdisk_get_superblock(void);                
index_node_t *ramdisk_get_index_node(int index_node_number);
unsigned char *ramdisk_get_block_bitmap(void);
char *ramdisk_get_block_memory_address(int block_pointer);


int ramdisk_get_unused_index_node(void);    // return one unused index node 

int ramdisk_block_malloc(void);  // allocate one free block

int ramdisk_block_calloc(void);  // allocate one free block and clear its all memory to zero

void ramdisk_block_free(int block_pointer);   // set one allocated used block to free status and free it to the free block list. 


int ramdisk_update_parent_directory_file(index_node_t *index_node, dir_entry_t *entry);  //  add an directory entry to its parent directory file index node. 

index_node_t *ramdisk_get_directory_index_node(const char *pathname);    // find the directory file index node of a specified pathname.  

/* Find the child directory entry by a specified name
from its parent directory file index node.*/
dir_entry_t *ramdisk_get_dir_entry(index_node_t *index_node, const char *filename_start, const char *filename_end);

/* Find an empty child directory entry
from its parent directory file index node.*/
dir_entry_t *ramdisk_get_empty_entry(index_node_t *index_node);

/* Initialize the file position's data structure. */
void ramdisk_file_position_init(file_position_t *file_position,
  index_node_t *index_node,
  int pos,
  int is_read_mode);

/* Add the file position by a specified offset, offser. */
void ramdisk_file_position_add(file_position_t *file_position, int offset);


/* Allocate the neccesary block memory for storing the block pointer and for storing the file's data.*/
char *ramdisk_get_memory_address(file_position_t *file_position);

/* Initialize the block pointer data structure. */
void ramdisk_block_pointer_init(block_pointer_t *block_pointer,
  index_node_t *index_node,
  int block_number,
  int is_read_mode);

/* Increate the block pointer. */
void ramdisk_block_pointer_increase(block_pointer_t *block_pointer);

/* Allocate the neccesary block memory
   for storing the block pointer and for storing the file's data.
int ramdisk_alloc_and_get_block_pointer(block_pointer_t *block_pointer);


int ramdisk_creat(char *pathname);

int ramdisk_unlink(char *pathname);

int ramdisk_open(char *pathname, int *index_node_number);

int ramdisk_close(int index_node_number);

int ramdisk_read(int index_node_number, int file_position, char *address, int num_bytes);

int ramdisk_write(int index_node_number, int file_position, char *address, int num_bytes);

int ramdisk_lseek(int index_node_number, int seek_offset, int *seek_result_offset);

int ramdisk_mkdir(char *pathname);

int ramdisk_readdir(int index_node_number, char *address, int *file_position);



#endif




