#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

MODULE_LICENSE("GPL");

#define BLOCK_SIZE 256
#define INODE_SIZE 64
#define MAX_FILE_SIZE 1067008  // Maximum file size based on block pointers

#define RAMDISK_SIZE (2 * 1024 * 1024)  // 2 MB
#define DATA_BLOCK_SIZE ((RAMDISK_SIZE/256) - 1 - 256 - 4) // 3835 blocks: blocks in 2 MB - superblock - inodes - bitmap
#define MAX_INODES 1024  // Maximum number of inodes 256 blocks * 256 bytes / 64 bytes per inode

struct operations{
	char* filename;
	char *pathname;
	int fid;
	int offset;
	char* input_content;
	int input_size;
}

// superblock is the first block of the RAM disk (256 byttes)
struct superblock {
    unsigned int free_blocks;
    unsigned int free_inodes;
    char remaining[248]; // Remaining space in the block
};

// index node for each file
struct index_node {
    char type[4]; // assuming a maximum of 3 characters for type
    size_t size;
    void *location[10]; // First 8 blocks are direct, 9th is single indirect, 10th is double indirect
    int dir_entries_count;
    char remaining[6]; 
};

// bitmap to keep track of free blocks 0 = free, 1 = used
struct bitmap {
    char bits[BLOCK_SIZE]; // 1 bit per block
};

// directory entry for each file
struct dir_entry{
    char name[14];
    int inode_index; // 2 bytes
};

// data block storage
typedef union data_block data_block_struct;
union data_block{
    char data[BLOCK_SIZE];
    struct dir_entry dir_entries[16];
    data_block_struct index_block[BLOCK_SIZE/4];
};

// ramdisk data structure
struct ramdisk_struct{
    // 1 block for superblock
    struct superblock superblock_data;
    // 256 blocks for inodes
    struct index_node index_node_data[MAX_INODES];
    // 4 blocks for bitmap
    struct bitmap bitmap_data[4];
    // 4096 - 1 - 256 - 4 = 3835 blocks for data
    data_block_struct data_block_data[DATA_BLOCK_SIZE];
}

struct ramdisk_struct *ramdisk;
struct proc_dir_entry *proc_entry;

/*
From here, are steps to implemente FDT by using linked list
I will create FDT by a linked list
*/

//step 1: create fdt
typedef struct _file_descriptor   //"_" --> private
{
  int fd;
  int file_position;
  int index_node_number;
  struct _file_descriptor *next;
  struct _file_descriptor *prev;
} file_descriptor_t;


/* This function append the file descriptor to the end of the file descriptor table. */
void append_file_descriptor_to_list(file_descriptor_t *file_descriptor);

/* This function remove the file descriptor from the file descriptor table. */
void remove_file_descriptor_from_list(file_descriptor_t *file_descriptor);

/* This function find the file descriptor with the specified file descriptor value, fd. */
file_descriptor_t *find_file_descriptor(int fd);

int current_fd = 1;
file_descriptor_t *file_descriptor_list_head = NULL;
file_descriptor_t *file_descriptor_list_tail = NULL;


//step2: This function append the file descriptor to the end of the file descriptor table. 
void append_file_descriptor_to_list(file_descriptor_t *file_descriptor)
{
  file_descriptor_t *head = NULL;
  file_descriptor_t *tail = NULL;
  
  head = file_descriptor_list_head;
  tail = file_descriptor_list_tail;
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

  file_descriptor_list_head = head;
  file_descriptor_list_tail = tail;
}

//step3: This function remove the file descriptor from the file descriptor table. 
void remove_file_descriptor_from_list(file_descriptor_t *file_descriptor)
{
  file_descriptor_t *head = NULL;
  file_descriptor_t *tail = NULL;
  file_descriptor_t *prev = NULL;
  file_descriptor_t *next = NULL;
  
  head = file_descriptor_list_head;
  tail = file_descriptor_list_tail;

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

  file_descriptor_list_head = head;
  file_descriptor_list_tail = tail;
}


//step4: This function find the file descriptor with the specified file descriptor value, fd. 
file_descriptor_t *find_file_descriptor(int fd)
{
  file_descriptor_t *curr = NULL;

  curr = file_descriptor_list_head;
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


// Function to initialize the filesystem
int initialize_filesystem(void) {
    printk("Initializing RAM Disk!\n");

    ramdisk= (ramdisk_struct*)vmalloc(RAMDISK_SIZE);
    if (!ramdisk_memory) {
        printk(KERN_ERR "Failed to allocate memory for ramdisk.\n");
        return -ENOMEM;
    }

    // superblock initialization
    ramdisk->superblock_data.free_blocks = DATA_BLOCK_SIZE;
    ramdisk->superblock_data.free_inodes = MAX_INODES;

    // IMPLEMENT ME - need to initialize process file table 
    file_descriptor_list_head = NULL;
    file_descriptor_list_tail = NULL;
    current_fd = 1;  // Initialize the global file descriptor counter

    int result = rd_mkdir("/"); // Create the root directory
    if (result != 0) {
        printk(KERN_INFO "RAM Disk initialized!\n");
    }
    else{
        printk(KERN_ERR "Failed to initialize RAM Disk.\n");
    }
    return result;
}


int ramdisk_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg){
    char path[256];
    char *path_pointer;
    int ret = -1;
    int* fd;
    operations *user_input_content;

    switch(cmd){
        case IOCTL_CREAT:
            printk(KERN_INFO "Creating file through ioctl.\n");
            user_input_content = (operations *)vmalloc(sizeof(operations));
            copy_from_user(user_input_content, (operations *)arg, sizeof(operations));
            ret = rd_create(user_input_content->pathname);
            vfree(user_input_content);
        case IOCTL_MKDIR:
            printk(KERN_INFO "Creating directory through ioctl.\n");
            user_input_content = (operations *)vmalloc(sizeof(operations));
            copy_from_user(user_input_content, (operations *)arg, sizeof(operations));
            ret = rd_mkdir(user_input_content->pathname);
            vfree(user_input_content);
    }
    return ret;
}

static int __init initialization_routine(void) {
    int init_fs_result = initialize_filesystem();
    if (init_fs_result != 0) {
        printk(KERN_ERR "Failed to initialize filesystem.\n");
        return init_fs_result;
    }

    ramdisk.ioctl = ramdisk_ioctl;
    proc_entry = proc_create_entry("ramdisk", 0666, NULL, NULL);
    if (!proc_entry) {
        printk(KERN_ERR "Error creating proc entry.\n");
        vfree(ramdisk_memory);
        return -ENOMEM;
    }
    proc_entry->proc_fops = &ramdisk;
    printk(KERN_INFO "Proc entry created.\n");
    return 0;
}

static void __exit cleanup_routine(void) {
    if (proc_entry) {
        remove_proc_entry("ramdisk", NULL);
    }
    if (ramdisk_memory) {
        vfree(ramdisk_memory);
    }
    printk(KERN_INFO "Proc entry removed.\n");
}

module_init(initialization_routine);
module_exit(cleanup_routine);







//this function is to parse the path of the parent directory from the complete file path
void parse_parent_path(const char *path, char *parent_path) {
    char *last_slash;

    // Find the last occurrence of '/'
    last_slash = strrchr(path, '/');

    if (last_slash == NULL) {
        // If no slash is found, it means the path is a file in the root directory
        strcpy(parent_path, "/");
    } else {
        // Copy everything from the start of the path to the last '/'
        strncpy(parent_path, path, last_slash - path);
        // Null-terminate the string
        parent_path[last_slash - path] = '\0';
    }
}


/*
Assuming that each index_node structure has a path associated with it 
(this detail has not been specified now, so just assume a simple implementation), 
we can iterate through all inodes to find matching paths. Here is the basic implementation of the function:
*/
int find_inode_index_by_path(const char *pathname) {
    struct index_node *current_inode;
    int i;

    // Iterate through all possible inodes
    for (i = 0; i < MAX_INODES; i++) {
        current_inode = &inode_pointer[i];

        // Check if the current inode is valid
        if (current_inode->type[0] != '\0') {
            // Compare the path of the inode with the provided pathname
            if (strcmp(current_inode->path, pathname) == 0) {
                // Matching inode found, return its index
                return i;
            }
        }
    }

    // No matching path found
    return -1;
}

//Parse the directory and filename separately //
void extract_directory_and_filename(const char* input_path, char* output_directory, char* output_filename) {
    int index;
    int length = strlen(input_path);
    
    for (index = length - 1; index >= 0; index--) {
        // directory is subdirectory (from root)
        if (input_path[index] == '/' && index != 0) {
            strncpy(output_directory, input_path, index);
            output_directory[index] = '\0'; 
            break;
        }
        // the directory is the root
        if (input_path[index] == '/' && index == 0) {
            strcpy(output_directory, "/\0");
            break;
        }
    }
    strcpy(output_filename, &input_path[index + 1]);
}

/*
This function is used to check whether a file of a specific name exists in a given directory inode. 
This function will iterate through the inodes of a directory, 
comparing the name of each subfile or subdirectory to see if it matches the supplied filename.
*/
int file_exists_in_directory(int dir_inode_index, const char *file_name) {
    struct index_node *dir_inode;
    struct index_node *child_inode;
    int i;

    // Get the inode for the directory
    dir_inode = &inode_pointer[dir_inode_index];

    // Check if the inode is a directory
    if (strcmp(dir_inode->type, "dir") != 0) {
        // Not a directory
        return 0; // File does not exist
    }

    // Iterate through the directory's entries
    for (i = 0; i < dir_inode->block_count; i++) {
        // Get the inode for the current entry
        child_inode = (struct index_node *)(dir_inode->locations[i]);

        // Check if the current entry's name matches the file name
        if (strcmp(child_inode->name, file_name) == 0) {
            // Found a matching entry
            return 1; // File exists
        }
    }

    // No matching entry found
    return 0; // File does not exist
}


/*
Find an unused inode in the file system's inode array and assign it to a new file or directory. 
This function should return the index of the newly allocated inode, or an error code if no inode is available.

P.S: Note that the implementation of this function assumes that the inode array is accessed through the inode_pointer pointer, 
and whether an inode is used is determined by the first character of its type field.
*/
int allocate_inode(void) {
    int i;

    // Iterate through the inode array to find an unused inode
    for (i = 0; i < 64; i++) {
        if (inode_pointer[i].type[0] == '\0') {
            // Found an unused inode, mark it as used
            inode_pointer[i].type[0] = 'u'; // 'u' for used, can be any non-empty value
            return i; // Return the index of the allocated inode
        }
    }

    // No available inode found
    return -1; // Return an error code
}

/*
Mark a specific block in a bitmap as used

P.S :Assume that the bitmap structure has a character array named bits to store the bitmap, 
and each character (byte) represents the usage of 8 blocks
*/
void mark_block_as_used(int block_index) {
    int byte_index = block_index / 8; // 8 bits for a byte
    int bit_offset = block_index % 8;

    // Set the corresponding bit to 1 to mark this block as used
    bitmap_pointer->bits[byte_index] |= (1 << bit_offset);
}

/*
The implementation of this function assumes that the type field is a character array
*/
void initialize_inode(int inode_index, const char *type, int parent_index) {
    struct index_node *inode;

    // Get the inode at the specified index
    inode = &inode_pointer[inode_index];

    // Set the inode's type, assuming 'type' field is a character array
    strncpy(inode->type, type, sizeof(inode->type) - 1);
    inode->type[sizeof(inode->type) - 1] = '\0'; // Ensure string is null-terminated

    // Initialize other attributes of the inode
    inode->size = 0; // Set the initial size of the new file or directory to 0
    memset(inode->locations, 0, sizeof(inode->locations)); // Clear data block locations
    inode->parent_index = parent_index; // Set the parent directory index
    inode->block_count = 0; // Initialize block count to 0
    inode->last_block_offset = 0; // Initialize the last block offset to 0
}



// full pathname, type: reg (file) or dir (directory)
int rd_create(char *pathname, char* type){
    printk(KERN_INFO "Creating through rd_create... \n");

    char *my_parent = (char *)vmalloc(strlen(pathname));
    char *my_child = (char *)vmalloc(strlen(pathname));
    struct index_node *parent_inode_index = NULL;
    struct index_node *inode_index = NULL;
    struct dir_entry new_entry;
    // 1. check superblock for information on free blocks
    if (superblock.free_inodes == 0){
        printk(KERN_ERR "No free i-nodes available.\n");
        return -1;
    }
    if (superblock.free_blocks == 0){
        printk(KERN_ERR "No free blocks available.\n");
        return -1;
    }

    // 2. parse parent directory and filename
    memset(my_parent, 0, strlen(pathname));
    memset(my_child, 0, strlen(my_child));
    // /root/bostonuniversity/CS552/ramdisk.c: parent = /root/bostonuniversity/CS552, child = ramdisk.c
    extract_directory_and_filename(pathname, my_parent, my_child);

    // 3. check to see if parent directory exists, need to recursively check all parent directories
    parent_inode_index = find_inode_index_by_path(my_parent);
    if (parent_inode_index < 0) {
        printk(KERN_ERR "Parent directory does not exist.\n");
        return -1;
    }
   
    // 4. check if file already exists 
    if (file_exists_in_directory(parent_inode_index, my_child)) {
        printk(KERN_ERR "File already exists.\n");
        return -1;
    }

    // 5. IMPLEMENT ME - find free inode index by looping through i node array 
    //implemented by calling allocate_inode() to find a free inode
    inode_index = allocate_inode();
    if (inode_index < 0) {
        printk(KERN_ERR "Failed to allocate inode.\n");
        return -1;
    }

    // 6. Initialize inode with child content 
    initialize_inode(inode_index, my_child, type, parent_inode_index);

    // 7. Update superblock and bitmap
    super_block_pointer->free_inodes--;
    // UNDERSTAND or replace
    mark_block_as_used(inode_index);

    // 8. create file object (understand)
    dir_struct *new_entry = free_entry(inode_index);
    if (new_entry == NULL) {
        printk(KERN_ERR "Failed to allocate directory entry.\n");
        return -1;
    }
    strcpy(new_entry->name, my_child);
    new_entry->inode_index = inode_index;
    inode_pointer[parent_inode_index]->offset += 16;

    // 9. IMPLEMENT ME - update parent directory to include child inode
    
    // 10. free used pointers
    vfree(my_parent);
    vfree(my_child);

    printk(KERN_INFO "File '%s' created successfully.\n", pathname);

    return 0; // Success
}

// mkdir uses rd_create to create a directory as only diff is type input
int rd_mkdir(char *pathname){
    // special case to declare root 
    if (strcmp(pathname, "/") == 0 && superblock->free_inodes == MAX_INODES) {
        printk(KERN_INFO "Initializing root directory");
        superblock->free_inodes--;
        strcpy(inode_pointer[0].type, "dir");
        return 0;
    }
    // normal directory
    else {
        int result = rd_create(pathname, "dir");
        if (result == 0) {
            printk(KERN_INFO "Directory '%s' created successfully.\n", pathname);
        }
        return result;
    }
    return -1;
}

//open current file
int rd_open(char *pathname) {
    printk(KERN_INFO "Opening file through rd_open...\n");

    int inode_index;
    file_descriptor_t *new_fd;

    // 1. find the inode of the file
    inode_index = find_inode_index_by_path(pathname);
    if (inode_index < 0) {
        printk(KERN_ERR "File not found.\n");
        return -1;  // file not found
    }

    // 2. allocate new fd
    new_fd = (file_descriptor_t *)vmalloc(sizeof(file_descriptor_t));
    if (new_fd == NULL) {
        printk(KERN_ERR "Failed to allocate memory for file descriptor.\n");
        return -ENOMEM;  
    }

    // 3. initialize the fd
    new_fd->fd = current_fd++;
    new_fd->file_position = 0;  //set the position(offset) to 0
                                //means we will start read & write from beginning
    new_fd->index_node_number = inode_index;
    new_fd->prev = NULL;
    new_fd->next = NULL;

    // 4. append the new fd to fdt
    append_file_descriptor_to_list(new_fd);

    printk(KERN_INFO "File '%s' opened successfully with FD %d.\n", pathname, new_fd->fd);

    
    return new_fd->fd;
}

//close current file
int rd_close(int fd) {
    printk(KERN_INFO "Closing file with FD %d...\n", fd);

    file_descriptor_t *fd_to_remove;

    // 1. Find file descriptor
    fd_to_remove = find_file_descriptor(fd);
    if (fd_to_remove == NULL) {
        printk(KERN_ERR "Invalid file descriptor.\n");
        return -1;  // Invalid file descriptor
    }

    // 2. Remove from file descriptor table
    remove_file_descriptor_from_list(fd_to_remove);

    // 3. Release the memory occupied by the file descriptor structure
    vfree(fd_to_remove);

    printk(KERN_INFO "File with FD %d closed successfully.\n", fd);
    return 0; //close seccessfully
}





