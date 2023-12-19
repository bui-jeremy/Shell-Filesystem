



#ifndef _RAMDISK_H
#define _RAMDISK_H

typedef struct _pathname
{
  int pathname_length;
  const char *pathname;

}pathname_t;

typedef struct _creat_param
{
  /* The return value of the ramdisk kernel module. */
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



int ramdisk_creat(char *pathname);

int ramdisk_unlink(char *pathname);

int ramdisk_open(char *pathname, int *index_node_number);

int ramdisk_close(int index_node_number);

int ramdisk_read(int index_node_number, int file_position, char *address, int num_bytes);

int ramdisk_write(int index_node_number, int file_position, char *address, int num_bytes);

int ramdisk_lseek(int index_node_number, int seek_offset, int *seek_result_offset);

int ramdisk_mkdir(char *pathname);

int ramdisk_readdir(int index_node_number, char *address, int *file_position);

int rd_creat(char *pathname);

int rd_unlink(char *pathname);

int rd_open(char *pathname);

int rd_close(int fd);

int rd_read(int fd, char *address, int num_bytes);

int rd_write(int fd, char *address, int num_bytes);

int rd_lseek(int fd, int offset);

int rd_mkdir(char *pathname);

int rd_readdir(int fd, char *address);


#endif



