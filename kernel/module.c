/*
 *  ioctl test module -- Rich West.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h> /* error codes */
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/tty.h>
#include <linux/sched.h>

#include "ramdisk_kernel.h"


MODULE_LICENSE("GPL");

/* This function is the initialize function of the ramdisk. */
void ramdisk_init(void);
/* This function free the 2MB memory of the ramdisk allocated by call the vmalloc function. */
void ramdisk_uninit(void);
/* This function return the length of the directory entry data structure, which is the value 16. */
int ramdisk_get_dir_entry_length(void);

/* This is the main entry point of the kernel module's ioctl function. */
static int rd_ioctl(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg);

/* This function call ramdisk kernel module's ramdisk_creat function
   to create a regular file. */
static int rd_creat(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg);

/* This function call ramdisk kernel module's ramdisk_unlink function
   to remove the file from the ramdisk with absolute
   pathname from the filesystem, freeing its memory in the ramdisk. */
static int rd_unlink(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg);

/* This function call ramdisk kernel module's ramdisk_open function
   to open an existing file corresponding to pathname. */
static int rd_open(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg);

/* This function call ramdisk kernel module's ramdisk_close function
   to close the corresponding file. */
static int rd_close(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg);

/* This function call ramdisk kernel module's ramdisk_read function
   to read up to num_bytes from a regular file
   to the specified address in the calling process. */
static int rd_read(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg);

/* This function call ramdisk kernel module's ramdisk_write function
   to write up to num_bytes from the specified address
   in the calling process to a regular file. */
static int rd_write(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg);

/* This function call ramdisk kernel module's ramdisk_lseek function
   to set the file object's file position
   identified by index node number index_node_number, to offset. */
static int rd_lseek(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg);

/* This function call ramdisk kernel module's ramdisk_mkdir function
   to read one entry from a directory file identified by
   index node number index_node_number, and store the result
   in memory at the specified value of address. */
static int rd_mkdir(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg);

/* This function call ramdisk kernel module's ramdisk_open function
to open an existing file corresponding to pathname. */
static int rd_readdir(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg);

/* This function get the pathname from the calling process. */
char *strdup_ramdisk(pathname_t *pathname);


static struct file_operations pseudo_dev_proc_operations;

static struct proc_dir_entry *proc_entry;


static int __init initialization_routine(void) {
  pseudo_dev_proc_operations.ioctl = rd_ioctl;

  /* Start create proc entry */
  proc_entry = create_proc_entry("ramdisk", 0444, NULL);
  if(!proc_entry)
  {
    printk("<1> Error creating /proc entry.\n");
    return 1;
  }

  //proc_entry->owner = THIS_MODULE; <-- This is now deprecated
  proc_entry->proc_fops = &pseudo_dev_proc_operations;
  ramdisk_init();

  return 0;
}

/* 'printk' version that prints to active tty. */
void my_printk(char *string)
{
  struct tty_struct *my_tty;

  my_tty = current->signal->tty;

  if (my_tty != NULL) {
    (*my_tty->driver->ops->write)(my_tty, string, strlen(string));
    (*my_tty->driver->ops->write)(my_tty, "\015\012", 2);
  }
} 


static void __exit cleanup_routine(void) {

  ramdisk_uninit();
  remove_proc_entry("ramdisk", NULL);

  return;
}


/* This is the main entry point of the kernel module's ioctl function. */
static int rd_ioctl(struct inode *inode, struct file *file,
				unsigned int cmd, unsigned long arg)
{
  switch (cmd)
  {
  /* Call ramdisk kernel module's ramdisk_creat function
     to create a regular file. */
  case IOCTL_CREAT:
    rd_creat(inode, file, cmd, arg);
    break;

  /* Call ramdisk kernel module's ramdisk_unlink function
     to remove the file from the ramdisk with absolute
     pathname from the filesystem, freeing its memory in the ramdisk. */
  case IOCTL_UNLINK:
    rd_unlink(inode, file, cmd, arg);
    break;

  /* Call ramdisk kernel module's ramdisk_open function
     to open an existing file corresponding to pathname. */
  case IOCTL_OPEN:
    rd_open(inode, file, cmd, arg);
    break;

  /* Call ramdisk kernel module's ramdisk_close function
     to close the corresponding file. */
  case IOCTL_CLOSE:
    rd_close(inode, file, cmd, arg);
    break;

  /* Call ramdisk kernel module's ramdisk_read function
     to read up to num_bytes from a regular file
     to the specified address in the calling process. */
  case IOCTL_READ:
    rd_read(inode, file, cmd, arg);
    break;

  /* Call ramdisk kernel module's ramdisk_write function
     to write up to num_bytes from the specified address
     in the calling process to a regular file. */
  case IOCTL_WRITE:
    rd_write(inode, file, cmd, arg);
    break;

  /* Call ramdisk kernel module's ramdisk_lseek function
     to set the file object's file position
     identified by index node number index_node_number, to offset. */
  case IOCTL_LSEEK:
    rd_lseek(inode, file, cmd, arg);
    break;

  /* Call ramdisk kernel module's ramdisk_mkdir function
     to create a directory. */
  case IOCTL_MKDIR:
    rd_mkdir(inode, file, cmd, arg);
    break;

  /* Call ramdisk kernel module's ramdisk_open function
     to open an existing file corresponding to pathname. */
  case IOCTL_READDIR:
    rd_readdir(inode, file, cmd, arg);
    break;

  default:
    return -EINVAL;
    break;
  }
  
  return 0;
}

/* This function call ramdisk kernel module's ramdisk_creat function
   to create a regular file. */
static int rd_creat(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg)
{
  creat_param_t creat_param;
  char *pathname = NULL;

  copy_from_user(&creat_param, (creat_param_t *)arg,
    sizeof(creat_param_t));
  /* Get the pathname from the calling process. */
  pathname = strdup_ramdisk(&creat_param.pathname);

  /* Call ramdisk kernel module's ramdisk_creat function
     to create a regular file. */
  creat_param.return_value = ramdisk_create(pathname, "reg");
  copy_to_user((int *)arg, &creat_param.return_value, sizeof(int));

  /* Free the pathname. */
  kfree(pathname);

  return 0;
}

/* This function call ramdisk kernel module's ramdisk_unlink function
   to remove the file from the ramdisk with absolute
   pathname from the filesystem, freeing its memory in the ramdisk. */
static int rd_unlink(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg)
{
  creat_param_t unlink_param;
  char *pathname = NULL;

  copy_from_user(&unlink_param, (creat_param_t *)arg,
    sizeof(creat_param_t));
  /* Get the pathname from the calling process. */
  pathname = strdup_ramdisk(&unlink_param.pathname);

  /* Call ramdisk kernel module's ramdisk_unlink function
     to remove the file from the ramdisk with absolute
     pathname from the filesystem, freeing its memory in the ramdisk. */
  unlink_param.return_value = ramdisk_unlink(pathname);
  copy_to_user((int *)arg, &unlink_param.return_value, sizeof(int));

  /* Free the pathname. */
  kfree(pathname);

  return 0;
}

/* This function call ramdisk kernel module's ramdisk_open function
   to open an existing file corresponding to pathname. */
static int rd_open(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg)
{
  open_param_t open_param;
  char *pathname = NULL;

  copy_from_user(&open_param, (open_param_t *)arg,
    sizeof(open_param_t));
  /* Get the pathname from the calling process. */
  pathname = strdup_ramdisk(&open_param.pathname);

  /* Call ramdisk kernel module's ramdisk_open function
     to open an existing file corresponding to pathname. */
  open_param.return_value = ramdisk_open(pathname, &open_param.index_node_number);
  copy_to_user((open_param_t *)arg, &open_param, sizeof(open_param_t));

  /* Free the pathname. */
  kfree(pathname);

  return 0;
}

/* This function call ramdisk kernel module's ramdisk_close function
   to close the corresponding file. */
static int rd_close(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg)
{
  close_param_t close_param;

  copy_from_user(&close_param, (close_param_t *)arg,
    sizeof(close_param_t));

  /* Call ramdisk kernel module's ramdisk_close function
     to close the corresponding file. */
  close_param.return_value = ramdisk_close(close_param.index_node_number);
  copy_to_user((int *)arg, &close_param.return_value, sizeof(int));

  return 0;
}

/* This function call ramdisk kernel module's ramdisk_read function
   to read up to num_bytes from a regular file
   to the specified address in the calling process. */
static int rd_read(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg)
{
  read_write_param_t read_param;

  copy_from_user(&read_param, (read_write_param_t *)arg,
    sizeof(read_write_param_t));

  /* Call ramdisk kernel module's ramdisk_read function
     to read up to num_bytes from a regular file
     to the specified address in the calling process. */
  read_param.return_value = ramdisk_read(read_param.index_node_number,
    read_param.file_position,
    read_param.address,
    read_param.num_bytes);
  copy_to_user((read_write_param_t *)arg, &read_param, sizeof(read_write_param_t));

  return 0;
}

/* This function call ramdisk kernel module's ramdisk_write function
   to write up to num_bytes from the specified address
   in the calling process to a regular file. */
static int rd_write(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg)
{
  read_write_param_t write_param;

  copy_from_user(&write_param, (read_write_param_t *)arg,
    sizeof(read_write_param_t));

  /* Call ramdisk kernel module's ramdisk_write function
     to write up to num_bytes from the specified address
     in the calling process to a regular file. */
  write_param.return_value = ramdisk_write(write_param.index_node_number,
    write_param.file_position,
    write_param.address,
    write_param.num_bytes);
  copy_to_user((read_write_param_t *)arg, &write_param, sizeof(read_write_param_t));

  return 0;
}

/* This function call ramdisk kernel module's ramdisk_lseek function
   to set the file object's file position
   identified by index node number index_node_number, to offset. */
static int rd_lseek(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg)
{
  int seek_result_offset = 0;
  lseek_param_t lseek_param;

  copy_from_user(&lseek_param, (lseek_param_t *)arg,
    sizeof(lseek_param_t));

  /* Call ramdisk kernel module's ramdisk_lseek function
     to set the file object's file position
     identified by index node number index_node_number, to offset. */
  lseek_param.return_value = ramdisk_lseek(lseek_param.index_node_number,
    lseek_param.seek_offset,
    &seek_result_offset);
  if (0 == lseek_param.return_value)
  {
    lseek_param.seek_result_offset = seek_result_offset;
  }
  copy_to_user((lseek_param_t *)arg, &lseek_param, sizeof(lseek_param_t));

  return 0;
}

/* This function call ramdisk kernel module's ramdisk_mkdir function
   to create a directory. */
static int rd_mkdir(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg)
{
  creat_param_t mkdir_param;
  char *pathname = NULL;

  copy_from_user(&mkdir_param, (creat_param_t *)arg,
    sizeof(creat_param_t));
  /* Get the pathname from the calling process. */
  pathname = strdup_ramdisk(&mkdir_param.pathname);

  /* Call ramdisk kernel module's ramdisk_mkdir function
     to create a directory. */
  mkdir_param.return_value = ramdisk_mkdir(pathname);
  copy_to_user((int *)arg, &mkdir_param.return_value, sizeof(int));

  /* Free the pathname. */
  kfree(pathname);

  return 0;
}

/* This function call ramdisk kernel module's ramdisk_mkdir function
   to read one entry from a directory file identified by
   index node number index_node_number, and store the result
   in memory at the specified value of address. */
static int rd_readdir(struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg)
{
  readdir_param_t readdir_param;

  copy_from_user(&readdir_param, (readdir_param_t *)arg,
    sizeof(readdir_param_t));

  /* Call ramdisk kernel module's ramdisk_mkdir function
     to read one entry from a directory file identified by
     index node number index_node_number, and store the result
     in memory at the specified value of address. */
  readdir_param.return_value = ramdisk_readdir(readdir_param.index_node_number,
    readdir_param.address,
    &readdir_param.file_position);
  if (readdir_param.return_value > 0)
  {
    /* Get the length of the directory entry data structure, which is the value 16. */
    readdir_param.dir_data_length = ramdisk_get_dir_entry_length();
  }
  copy_to_user((readdir_param_t *)arg, &readdir_param, sizeof(readdir_param_t));

  return 0;
}

/* This function get the pathname from the calling process. */
char *strdup_ramdisk(pathname_t *pathname)
{
  char *dup_str = NULL;

  dup_str = (char *)kmalloc(pathname->pathname_length + 1, GFP_KERNEL);
  copy_from_user(dup_str, pathname->pathname, pathname->pathname_length);
  dup_str[pathname->pathname_length] = '\0';

  return dup_str;
}

module_init(initialization_routine); 
module_exit(cleanup_routine); 



