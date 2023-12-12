#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <string.h>

MODULE_LICENSE("GPL");

#define BLOCK_SIZE 256
#define INODE_SIZE 64
#define MAX_FILE_SIZE 1067008  // Maximum file size based on block pointers

#define RAMDISK_SIZE (4 * 1024 * 1024)  // Adjust RAMDISK_SIZE as needed

struct index_node {
    char type[4]; // assuming a maximum of 3 characters for type
    size_t size;
    void *locations[10]; // First 8 blocks are direct, 9th is single indirect, 10th is double indirect
    int parent_index;
    unsigned int block_count;
    unsigned int last_block_offset;
};

struct bitmap {
    char bits[BLOCK_SIZE]; // 1 bit per block
};

// superblock is the first block of the RAM disk
struct superblock {
    unsigned int free_blocks;
    unsigned int free_inodes;
};

struct operations{
	char* filename;
	char *pathname;
	int fid;
	int offset;
	char* input_content;
	int input_size;
}

void *ramdisk_memory;
struct proc_dir_entry *proc_entry;

operations ramdisk;
superblock *super_block_pointer;
index_node *inode_pointer;
bitmap *bitmap_pointer;
void *space_pointer;

// Function to initialize the filesystem
int initialize_filesystem(void) {
    printk("Initializing RAM Disk!\n");

    ramdisk_memory = (void *)vmalloc(RAMDISK_SIZE);
    if (!ramdisk_memory) {
        printk(KERN_ERR "Failed to allocate memory for ramdisk.\n");
        return -ENOMEM;
    }

    super_block_pointer = (struct superblock *)ramdisk_memory;
    inode_pointer = (struct index_node *)((char *)ramdisk_memory + BLOCK_SIZE);
    bitmap_pointer = (struct bitmap *)((char *)inode_pointer + BLOCK_SIZE * 256);
    space_pointer = (void *)((char *)bitmap_pointer + BLOCK_SIZE * 4);  // Space for actual file data

    memset(ramdisk_memory, 0, RAMDISK_SIZE);

    // Initialize superblock
    super_block_pointer->free_blocks = (RAMDISK_SIZE / BLOCK_SIZE) - 1 - (4 + 1);  // Adjust for superblock and bitmap blocks
    super_block_pointer->free_inodes = 1024;  // Initial number of free inodes

    // Initialize bitmap
    bitmap_pointer->bits[0] |= 0x80;  // Mark first bit as used (assuming little-endian)

    // Initialize root directory inode
    strcpy(inode_pointer->type, "dir");
    inode_pointer->size = 0;
    inode_pointer->locations[0] = space_pointer;
    inode_pointer->parent_index = -1;  // Root directory has no parent
    inode_pointer->block_count = 0;
    inode_pointer->last_block_offset = 0;

    printk(KERN_INFO "RAM Disk initialized!\n");
    return 0; 
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

//Extract filename from given full path
char *extract_file_name(const char *pathname) {
    char *last_slash;

    // Find the last occurrence of '/' in the path
    last_slash = strrchr(pathname, '/');

    if (last_slash == NULL) {
        // No slash found, the entire path is the file name
        return strdup(pathname);
    } else if (*(last_slash + 1) == '\0') {
        // The path ends with '/', indicating it's a directory, not a file
        return NULL;
    } else {
        // Return the substring after the last '/'
        return strdup(last_slash + 1);
    }
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
    for (i = 0; i < MAX_INODES; i++) {
        if (inode_pointer[i].type[0] == '\0') {
            // Found an unused inode, mark it as used
            inode_pointer[i].type[0] = 'u'; // 'u' for used, can be any non-empty value
            return i; // Return the index of the allocated inode
        }
    }

    // No available inode found
    return -1; // Return an error code
}




int rd_create(char *pathname){
    char *parent_path[50];
    char *child;

    // 1. check superblock for information on free blocks
    if (superblock.free_inodes == 0){
        printk(KERN_ERR "No free i-nodes available.\n");
        return -1;
    }
    if (superblock.free_blocks == 0){
        printk(KERN_ERR "No free blocks available.\n");
        return -1;
    }

    // 2. check to see if parent directory exists, need to recursively check all parent directories
    // Parse parent directory from pathname
    parse_parent_path(pathname, parent_path);
    parent_inode_index = find_inode_index_by_path(parent_path);
    if (parent_inode_index < 0) {
        printk(KERN_ERR "Parent directory does not exist.\n");
        return -1;
    }

    // 3. extract file name with string parsing (Extract file name from pathname)
    child = extract_file_name(pathname);
    if (child == NULL) {
        printk(KERN_ERR "Invalid file name.\n");
        return -1;
    }

    // 4. check if file already exists 
    if (file_exists_in_directory(parent_inode_index, child)) {
        printk(KERN_ERR "File already exists.\n");
        return -1;
    }

    // 5. Allocate an inode for the new file
    inode_index = allocate_inode();
    if (inode_index < 0) {
        printk(KERN_ERR "Failed to allocate inode.\n");
        return -1;
    }

    // 6. Update superblock and bitmap
    super_block_pointer->free_inodes--;
    mark_block_as_used(inode_index);




    // 5. declare FDT to the file

}

int rd_mkdir(char *pathname){
     // 1. check superblock for information on free blocks
    if (superblock.free_inodes == 0){
        printk(KERN_ERR "No free i-nodes available.\n");
        return -ENOMEM;
    }
    if (superblock.free_blocks == 0){
        printk(KERN_ERR "No free blocks available.\n");
        return -ENOMEM;
    }

    // 2. check to see if parent directory exists, need to recursively check all parent directories
    
    // 3. extract file name with string parsing 

    // 4. check if file already exists 

    // 5. declare FDT to the file
}

