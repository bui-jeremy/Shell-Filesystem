#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

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

void *ramdisk_memory;
struct proc_dir_entry *proc_entry;

// Function to initialize the filesystem
int initialize_filesystem(void) {
    printk("Initializing RAM Disk!\n");

    ramdisk_memory = (void *)vmalloc(RAMDISK_SIZE);
    if (!ramdisk_memory) {
        printk(KERN_ERR "Failed to allocate memory for ramdisk.\n");
        return -ENOMEM;
    }

    struct superblock *super_block_pointer = (struct superblock *)ramdisk_memory;
    struct index_node *inode_pointer = (struct index_node *)((char *)ramdisk_memory + BLOCK_SIZE);
    struct bitmap *bitmap_pointer = (struct bitmap *)((char *)inode_pointer + BLOCK_SIZE * 4);  // Assuming 4 inode blocks
    void *space_pointer = (void *)((char *)bitmap_pointer + BLOCK_SIZE);  // Space for actual file data

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

    return 0; 
}

static int __init initialization_routine(void) {
    int init_fs_result = initialize_filesystem();
    if (init_fs_result != 0) {
        printk(KERN_ERR "Failed to initialize filesystem.\n");
        return init_fs_result;
    }

    proc_entry = proc_create("ramdisk", 0666, NULL, NULL);
    if (!proc_entry) {
        printk(KERN_ERR "Error creating proc entry.\n");
        vfree(ramdisk_memory);
        return -ENOMEM;
    }

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
