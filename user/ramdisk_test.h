



#ifndef _RAMDISK_H
#define _RAMDISK_H

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

/* This function create a regular file with absolute pathname
   from the root of the directory tree,
   where each directory filename is delimited by a "/" character. */
int rd_creat(char *pathname);

/* This function remove the file from the ramdisk with absolute
   pathname from the filesystem, freeing its memory in the ramdisk. */
int rd_unlink(char *pathname);

/* This function open an existing file corresponding to pathname. */
int rd_open(char *pathname);

/* This function close the corresponding file. */
int rd_close(int fd);

/* This function read up to num_bytes from a regular file
   to the specified address in the calling process. */
int rd_read(int fd, char *address, int num_bytes);

/* This function write up to num_bytes from the specified address
   in the calling process to a regular file. */
int rd_write(int fd, char *address, int num_bytes);

/* This function set the file object's file position
   identified by file descriptor fd, to offset,
   returning the new position, or the end of the
   file position if the offset is beyond the file's current size. */
int rd_lseek(int fd, int offset);

/* This function create a directory file with absolute pathname
   from the root of the directory tree,
   where each directory filename is delimited by a "/" character. */
int rd_mkdir(char *pathname);

/* This function read one entry from a directory file identified by
   file descriptor fd, and store the result in memory at the
   specified value of address. */
int rd_readdir(int fd, char *address);


#endif



