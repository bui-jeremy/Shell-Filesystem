#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>


#include "ramdisk_test.h"

typedef struct _ramdisk_file_descriptor
{
  int fd;
  int file_position;
  int index_node_number;
  struct _ramdisk_file_descriptor *next;
  struct _ramdisk_file_descriptor *prev;
} ramdisk_file_descriptor_t;


/* append the file descriptor to the end of the file descriptor table. */
void append_file_descriptor_to_list(ramdisk_file_descriptor_t *file_descriptor);

/* remove the file descriptor from the file descriptor table. */
void remove_file_descriptor_from_list(ramdisk_file_descriptor_t *file_descriptor);

/* find the file descriptor with the specified file descriptor value, fd. */
ramdisk_file_descriptor_t *find_file_descriptor(int fd);



int ramdisk_current_fd = 1;   //0 in, 1 out, 2 error
ramdisk_file_descriptor_t *ramdisk_file_descriptor_list_head = NULL;
ramdisk_file_descriptor_t *ramdisk_file_descriptor_list_tail = NULL;


int rd_creat(char *pathname)
{
  return ramdisk_creat(pathname);
}


int rd_unlink(char *pathname)
{
  return ramdisk_unlink(pathname);
}

int rd_open(char *pathname)
{
  int index_node_number = -1;
  ramdisk_file_descriptor_t *file_descriptor = NULL;

  /* Call the /proc/ramdisk kernel module's ioctl function to open */
  if (0 != ramdisk_open(pathname, &index_node_number))
  {
    return -1;
  }
  /* Create a file descriptor. */
  file_descriptor = (ramdisk_file_descriptor_t *)malloc(sizeof(ramdisk_file_descriptor_t));
  file_descriptor->index_node_number = index_node_number;
  file_descriptor->fd = ramdisk_current_fd++;
  file_descriptor->file_position = 0;
  /* Append the file descriptor to the end of the file descriptor table. */
  append_file_descriptor_to_list(file_descriptor);

  return file_descriptor->fd;
}


/* This function close the corresponding file. */
int rd_close(int fd)
{
  ramdisk_file_descriptor_t *file_descriptor = NULL;

  /* if fd refers to a non-existent file, return -1 */
  file_descriptor = find_file_descriptor(fd);
  if (NULL == file_descriptor)
  {
    return -1;
  }
  /* Call the /proc/ramdisk kernel module's ioctl function
     to close the corresponding file. */
  if (0 != ramdisk_close(file_descriptor->index_node_number))
  {
    return -1;
  }
  /* Remove the file descriptor from the file descriptor table. */
  remove_file_descriptor_from_list(file_descriptor);
  free(file_descriptor);

  return 0;
}


/* read up to num_bytes from a regular file to the specified address in the calling process. */
int rd_read(int fd, char *address, int num_bytes)
{
  int data_length_read = 0;
  ramdisk_file_descriptor_t *file_descriptor = NULL;

  /* if fd refers to a non-existent file,
     this function will return -1 to indicate fail. */
  file_descriptor = find_file_descriptor(fd);
  if (NULL == file_descriptor)
  {
    return -1;
  }
  /* Call the /proc/ramdisk kernel module's ioctl function */
  data_length_read = ramdisk_read(file_descriptor->index_node_number,
    file_descriptor->file_position,
    address,
    num_bytes);
  if (data_length_read < 0)
  {
    return -1;
  }
  /* Update the file position value. */
  file_descriptor->file_position = file_descriptor->file_position + data_length_read;

  return data_length_read;
}

/* write up to num_bytes from the specified address in the calling process to a regular file. */
int rd_write(int fd, char *address, int num_bytes)
{
  int data_length_write = 0;
  ramdisk_file_descriptor_t *file_descriptor = NULL;

  /* if fd refers to a non-existent file,
     this function will return -1 to indicate fail. */
  file_descriptor = find_file_descriptor(fd);
  if (NULL == file_descriptor)
  {
    return -1;
  }
  /* Call the /proc/ramdisk kernel module's ioctl function */
  data_length_write = ramdisk_write(file_descriptor->index_node_number,
    file_descriptor->file_position,
    address,
    num_bytes);
  if (data_length_write < 0)
  {
    return -1;
  }
  /* Update the file position value. */
  file_descriptor->file_position = file_descriptor->file_position + data_length_write;

  return data_length_write;
}

/* set the file object's file offset */
int rd_lseek(int fd, int offset)
{
  int seek_result_offset = -1;
  ramdisk_file_descriptor_t *file_descriptor = NULL;

  /* if fd refers to a non-existent file,
     this function will return -1 to indicate fail. */
  file_descriptor = find_file_descriptor(fd);
  if (NULL == file_descriptor)
  {
    return -1;
  }
  /* Call the /proc/ramdisk kernel module's ioctl function
     to set the file object's file position
     identified by index node number index_node_number, to offset. */
  if (0 != ramdisk_lseek(file_descriptor->index_node_number,
    offset,
    &seek_result_offset))
  {
    return -1;
  }
  /* Update the file position value. */
  file_descriptor->file_position = seek_result_offset;

  return 0;
}


/* create a directory file */
int rd_mkdir(char *pathname)
{
  return ramdisk_mkdir(pathname);
}

/* read one entry from a directory file and store the result
   in memory at the specified value of address. */
int rd_readdir(int fd, char *address)
{
  int read_result = 0;
  int next_file_position = 0;
  ramdisk_file_descriptor_t *file_descriptor = NULL;

  /* if either of the arguments to rd_readdir() are invalid,
     this function will return -1 to indicate fail. */
  if (NULL == address)
  {
    return -1;
  }
  /* if fd refers to a non-existent file,
     this function will return -1 to indicate fail. */
  file_descriptor = find_file_descriptor(fd);
  if (NULL == file_descriptor)
  {
    return -1;
  }
  /* Call the /proc/ramdisk kernel module's ioctl function
     to read one entry from a directory file and store the result
     in memory at the specified value of address. */
  next_file_position = file_descriptor->file_position;
  read_result = ramdisk_readdir(file_descriptor->index_node_number,
    address,
    &next_file_position);
  /* Update the file position value. */
  if (read_result >= 0)
  {
    file_descriptor->file_position = next_file_position;
  }

  return read_result;
}

/* append the file descriptor to the end of the file descriptor table. */
void append_file_descriptor_to_list(ramdisk_file_descriptor_t *file_descriptor)
{
  ramdisk_file_descriptor_t *head = NULL;
  ramdisk_file_descriptor_t *tail = NULL;
  
  head = ramdisk_file_descriptor_list_head;
  tail = ramdisk_file_descriptor_list_tail;
  if (NULL == head)
  {
    head = file_descriptor;
    file_descriptor->prev = NULL;
  }
  else
  {
    file_descriptor->prev = tail;
    tail->next = file_descriptor;
  }
  file_descriptor->next = NULL;
  tail = file_descriptor;

  ramdisk_file_descriptor_list_head = head;
  ramdisk_file_descriptor_list_tail = tail;
}


/* remove the file descriptor from the file descriptor table. */
void remove_file_descriptor_from_list(ramdisk_file_descriptor_t *file_descriptor)
{
  ramdisk_file_descriptor_t *head = NULL;
  ramdisk_file_descriptor_t *tail = NULL;
  ramdisk_file_descriptor_t *prev = NULL;
  ramdisk_file_descriptor_t *next = NULL;
  
  head = ramdisk_file_descriptor_list_head;
  tail = ramdisk_file_descriptor_list_tail;

  prev = file_descriptor->prev;
  next = file_descriptor->next;
  if (NULL != next)
  {
    next->prev = prev;
  }
  if (NULL != prev)
  {
    prev->next = next;
  }
  if (head == file_descriptor)
  {
    head = next;
  }
  if (tail == file_descriptor)
  {
    tail = prev;
  }

  ramdisk_file_descriptor_list_head = head;
  ramdisk_file_descriptor_list_tail = tail;
}


/* find the file descriptor with the specified file descriptor value, fd. */
ramdisk_file_descriptor_t *find_file_descriptor(int fd)
{
  ramdisk_file_descriptor_t *curr = NULL;

  curr = ramdisk_file_descriptor_list_head;
  while (NULL != curr)
  {
    // Return the file descriptor with the specified file descriptor value, fd.
    if (fd == curr->fd)
    {
      return curr;
    }
    curr = curr->next;
  }

  return NULL;
}


/* call the /proc/ramdisk kernel module's ioctl function to create a regular file. */
int ramdisk_creat(char *pathname)
{
  int ret = 0;   //return value of the ioctl function call
  int fd = 0;
  creat_param_t creat_param;

  fd = open("/proc/ramdisk", O_RDONLY);
  if (fd < 0)
  {
    return -1;
  }
  creat_param.return_value = -1;
  creat_param.pathname.pathname = (const char *)pathname;
  creat_param.pathname.pathname_length = (int)strlen(pathname);
  /* Call the /proc/ramdisk kernel module's ioctl function --->  make request */
  ret = ioctl(fd, IOCTL_CREAT, &creat_param);
  close(fd);
  if (ret != 0)
  {
    return -1;
  }
  if (creat_param.return_value != 0)
  {
    return creat_param.return_value;
  }

  return 0;
}


/* remove the file, freeing its memory in the ramdisk. */
int ramdisk_unlink(char *pathname)
{
  int ret = 0;
  int fd = 0;
  creat_param_t unlink_param;

  fd = open("/proc/ramdisk", O_RDONLY);
  if (fd < 0)
  {
    return -1;
  }
  unlink_param.return_value = -1;
  unlink_param.pathname.pathname = (const char *)pathname;
  unlink_param.pathname.pathname_length = (int)strlen(pathname);

  ret = ioctl(fd, IOCTL_UNLINK, &unlink_param); //make request to kernel
  close(fd);
  if (ret != 0)
  {
    return -1;
  }
  if (unlink_param.return_value != 0)
  {
    return unlink_param.return_value;
  }

  return 0;
}

/* call the /proc/ramdisk kernel module's ioctl function to open an existing file. */
int ramdisk_open(char *pathname, int *index_node_number)
{
  int ret = 0;
  int fd = 0;
  open_param_t open_param;

  fd = open("/proc/ramdisk", O_RDONLY);
  if (fd < 0)
  {
    return -1;
  }
  open_param.return_value = -1;
  open_param.pathname.pathname = (const char *)pathname;
  open_param.pathname.pathname_length = (int)strlen(pathname);

  ret = ioctl(fd, IOCTL_OPEN, &open_param);   //make request to kernel
  close(fd);
  if (ret != 0)
  {
    return -1;
  }
  if (open_param.return_value != 0)
  {
    return open_param.return_value;
  }
  *index_node_number = open_param.index_node_number;

  return 0;
}

/* call the /proc/ramdisk kernel module's ioctl function
   to close the corresponding file. */
int ramdisk_close(int index_node_number)
{
  int ret = 0;
  int fd = 0;
  close_param_t close_param;

  fd = open("/proc/ramdisk", O_RDONLY);
  if (fd < 0)
  {
    return -1;
  }
  close_param.return_value = -1;
  close_param.index_node_number = index_node_number;
  ret = ioctl(fd, IOCTL_CLOSE, &close_param);

  close(fd);   //make request to kernel
  if (ret != 0)
  {
    return -1;
  }

  return close_param.return_value;
}

/* call the /proc/ramdisk kernel module's ioctl function
   to read up to num_bytes from a regular file
   to the specified address in the calling process. . */
int ramdisk_read(int index_node_number, int file_position, char *address, int num_bytes)
{
  int ret = 0;
  int fd = 0;
  read_write_param_t read_param;

  fd = open("/proc/ramdisk", O_RDONLY);
  if (fd < 0)
  {
    return -1;
  }
  read_param.return_value = -1;
  read_param.index_node_number = index_node_number;
  read_param.file_position = file_position;
  read_param.address = address;
  read_param.num_bytes = num_bytes;

  ret = ioctl(fd, IOCTL_READ, &read_param);   ////make request to kernel
  close(fd);
  if (ret != 0)
  {
    return -1;
  }
  if (read_param.return_value < 0)
  {
    return read_param.return_value;
  }
  
  return read_param.return_value;
}

/* call the /proc/ramdisk kernel module's ioctl function
   to write up to num_bytes from the specified address
   in the calling process to a regular file. */
int ramdisk_write(int index_node_number, int file_position, char *address, int num_bytes)
{
  int ret = 0;
  int fd = 0;
  read_write_param_t write_param;

  fd = open("/proc/ramdisk", O_RDONLY);
  if (fd < 0)
  {
    return -1;
  }
  write_param.return_value = -1;
  write_param.index_node_number = index_node_number;
  write_param.file_position = file_position;
  write_param.address = address;
  write_param.num_bytes = num_bytes;

  ret = ioctl(fd, IOCTL_WRITE, &write_param);    //make request to kernel
  close(fd);
  if (ret != 0)
  {
    return -1;
  }
  if (write_param.return_value < 0)
  {
    return write_param.return_value;
  }

  return write_param.return_value;
}

/* call the /proc/ramdisk kernel module's ioctl function
   to set the file object's file position
   identified by index node number index_node_number, to offset. */
int ramdisk_lseek(int index_node_number, int seek_offset, int *seek_result_offset)
{
  int ret = 0;
  int fd = 0;
  lseek_param_t lseek_param;

  fd = open("/proc/ramdisk", O_RDONLY);
  if (fd < 0)
  {
    return -1;
  }
  lseek_param.return_value = -1;
  lseek_param.index_node_number = index_node_number;
  lseek_param.seek_offset = seek_offset;
  lseek_param.seek_result_offset = -1;

  ret = ioctl(fd, IOCTL_LSEEK, &lseek_param);    //make request to kernel
  close(fd);
  if (ret != 0)
  {
    return -1;
  }
  if (0 != lseek_param.return_value)
  {
    return lseek_param.return_value;
  }
  *seek_result_offset = lseek_param.seek_result_offset;

  return lseek_param.return_value;
}

/* call the /proc/ramdisk kernel module's ioctl function
   to create a directory. */
int ramdisk_mkdir(char *pathname)
{
  int ret = 0;
  int fd = 0;
  creat_param_t mkdir_param;

  fd = open("/proc/ramdisk", O_RDONLY);
  if (fd < 0)
  {
    return -1;
  }
  mkdir_param.return_value = -1;
  mkdir_param.pathname.pathname = (const char *)pathname;
  mkdir_param.pathname.pathname_length = (int)strlen(pathname);

  ret = ioctl(fd, IOCTL_MKDIR, &mkdir_param);     //make request to kernel
  close(fd); 
  if (ret != 0)
  {
    return -1;
  }
  if (mkdir_param.return_value != 0)
  {
    return mkdir_param.return_value;
  }

  return 0;
}

/* call the /proc/ramdisk kernel module's ioctl function
   to read one entry from a directory file, and store the result
   in memory at the specified value of address. */
int ramdisk_readdir(int index_node_number, char *address, int *file_position)
{
  int ret = 0;
  int fd = 0;
  readdir_param_t readdir_param;

  fd = open("/proc/ramdisk", O_RDONLY);
  if (fd < 0)
  {
    return -1;
  }
  readdir_param.return_value = -1;
  readdir_param.index_node_number = index_node_number;
  readdir_param.file_position = *file_position;

  ret = ioctl(fd, IOCTL_READDIR, &readdir_param);     //make request to kernel
  close(fd);
  if (ret != 0)
  {
    return -1;
  }
  if (readdir_param.return_value < 0)
  {
    return readdir_param.return_value;
  }
  *file_position = readdir_param.file_position;
  if (readdir_param.return_value > 0)
  {
    memcpy(address, readdir_param.address, readdir_param.dir_data_length);
  }

  return readdir_param.return_value;
}


