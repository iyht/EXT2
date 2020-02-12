/*
 *------------
 * This code is provided solely for the personal and private use of
 * students taking the CSC369H5 course at the University of Toronto.
 * Copying for purposes other than this use is expressly prohibited.
 * All forms of distribution of this code, whether as given or with
 * any changes, are expressly prohibited.
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019 MCS @ UTM
 * -------------
 */

#include "ext2fsal.h"
#include "e2fs.h"


//syncronization is here
void ext2_fsal_init(const char* image)
{
    /**
     * TODO: Initialization tasks, e.g., initialize synchronization primitives used,
     * or any other structures that may need to be initialized in your implementation,
     * open the disk image by mmap-ing it, etc.
     */
    int fd = open(image, O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); //This is used to read a image into memory
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    groupDesc = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
    inodeBitmap = (unsigned char *)(disk + EXT2_BLOCK_SIZE * groupDesc->bg_inode_bitmap);
    blockBitmap = (unsigned char *)(disk + EXT2_BLOCK_SIZE * groupDesc->bg_block_bitmap);
    inodeTable = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE*groupDesc->bg_inode_table);
    inode_bitmap_lock = malloc(sizeof(pthread_mutex_t ));
    block_bitmap_lock = malloc(sizeof(pthread_mutex_t ));
    
    pthread_mutex_init(inode_bitmap_lock, NULL);
    pthread_mutex_init(block_bitmap_lock, NULL);

}

void ext2_fsal_destroy()
{
    free(inode_bitmap_lock);
    free(block_bitmap_lock);
}
