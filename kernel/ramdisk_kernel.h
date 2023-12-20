#ifndef _RAMDISK_KERNEL_H
#define _RAMDISK_KERNEL_H

#define RAMDISK_MEMORY_SIZE         (2 * 1024 * 1024)
#define MAX_INDEX_NODES_COUNT       1024
#define BLK_SZ 256	
#define PTR_SZ 4		
#define PTRS_PB  (BLK_SZ / PTR_SZ) 
#define INDEX_NODE_ARRAY_BLOCK_COUNT    256
#define BLOCK_BITMAP_BLOCK_COUNT        4

#define DIRECT_BLOCK_POINTER_COUNT               8
#define SINGLE_INDIRECT_BLOCK_POINTER_COUNT      1
#define DOUBLE_INDIRECT_BLOCK_POINTER_COUNT      1

#define SINGLE_INDIRECT_BLOCK_POINTER (DIRECT_BLOCK_POINTER_COUNT)
#define DOUBLE_INDIRECT_BLOCK_POINTER (SINGLE_INDIRECT_BLOCK_POINTER + SINGLE_INDIRECT_BLOCK_POINTER_COUNT)


#define MAX_BLOCK_COUNT_IN_FILE   (DIRECT_BLOCK_POINTER_COUNT + PTRS_PB + PTRS_PB * PTRS_PB)
#define MAX_FILE_SIZE   (MAX_BLOCK_COUNT_IN_FILE * BLK_SZ)

// index node structure
typedef struct index_node_struct
{
  char type[4];
  int size;
  int location[10];
  int dir_entry_count;
  int open_counter;
  char padding[8];
} index_node_t;

typedef struct superblock_struct
{
  int num_free_blocks;
  int num_free_index_nodes;
  // root index node
  index_node_t first_block;
} superblock_t;


// directory entry for superblock
typedef struct dir_entry_struct
{
  char filename[14];
  short index_node_number;
} dir_entry_t;

// determining block pointer type
typedef enum block_pointer_struct
{
  direct_block_pointer_type = 1,
  single_indirect_block_pointer_type = 2,
  double_indirect_block_pointer_type = 3,
} block_pointer_type_t;

// actual blocks
typedef struct _block_pointer_struct
{
  int is_read_mode;
  block_pointer_type_t block_pointer_type;
  // blocks 0-7
  int direct_block_pointer;
  // blocks 0-63
  int single_indirect_block_pointer;
  // double indirect of row 0-63
  int double_indirect_block_pointer_row;
  // double indirect of column 0-63
  int double_indirect_block_pointer_column;
  index_node_t *index_node;
} block_pointer_t;

// file position in block
typedef struct file_position
{
  int file_position;
  int data_offset_in_block;
  block_pointer_t block_pointer;
} file_position_t;

typedef struct pathname
{
  int pathname_length;
  const char *pathname;
}pathname_t;

typedef struct creat_param
{
  int return_value;
  pathname_t pathname;
} creat_param_t;

typedef struct _open_param
{
  int return_value;
  pathname_t pathname;
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
  int file_position;
  char *address;
  int num_bytes;
} read_write_param_t;

typedef struct _lseek_param
{
  int return_value;
  int index_node_number;
  int seek_offset;
  int seek_result_offset;
} lseek_param_t;

typedef struct _readdir_param
{
  int return_value;
  int index_node_number;
  int file_position;
  int dir_data_length;
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
int ramdisk_get_dir_entry_length(void);
void ramdisk_free_index_node_memory(index_node_t *index_node);
superblock_t *ramdisk_get_superblock(void);
index_node_t *ramdisk_get_index_node(int index_node_number);
unsigned char *ramdisk_get_block_bitmap(void);
char *ramdisk_get_block_memory_address(int block_pointer);
int search_bitmap(void);
int ramdisk_block_calloc(void);
void ramdisk_block_free(int block_pointer);
int ramdisk_update_parent_directory_file(index_node_t *index_node, dir_entry_t *entry);
index_node_t *ramdisk_get_directory_index_node(const char *pathname);
dir_entry_t *ramdisk_get_dir_entry(index_node_t *index_node, const char *filename_start, const char *filename_end);
dir_entry_t *ramdisk_get_empty_entry(index_node_t *index_node);

void ramdisk_file_position_init(file_position_t *file_position,index_node_t *index_node,int pos,int is_read_mode);
void ramdisk_file_position_add(file_position_t *file_position, int offset);
char *ramdisk_get_memory_address(file_position_t *file_position);
void ramdisk_block_pointer_init(block_pointer_t *block_pointer,index_node_t *index_node,int block_number,int is_read_mode);
void ramdisk_block_pointer_increase(block_pointer_t *block_pointer);
int ramdisk_alloc_and_get_block_pointer(block_pointer_t *block_pointer);

int ramdisk_create(char *pathname, char *type);

int ramdisk_unlink(char *pathname);

int ramdisk_open(char *pathname, int *index_node_number);

int ramdisk_close(int index_node_number);

int ramdisk_read(int index_node_number, int file_position, char *address, int num_bytes);

int ramdisk_write(int index_node_number, int file_position, char *address, int num_bytes);

int ramdisk_lseek(int index_node_number, int seek_offset, int *seek_result_offset);

int ramdisk_mkdir(char *pathname);

int ramdisk_readdir(int index_node_number, char *address, int *file_position);
#endif




