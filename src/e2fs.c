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

/**
 * TODO: Make sure to add all necessary includes here.
 */

#include "e2fs.h"
// #include "ext2fsal.h"


#include <stdlib.h>
#include <stdio.h>


unsigned char *disk;
struct ext2_super_block *sb;
struct ext2_group_desc *groupDesc;
unsigned char *inodeBitmap;
unsigned char *blockBitmap;
struct ext2_inode *inodeTable;
pthread_mutex_t * inode_bitmap_lock;
pthread_mutex_t * block_bitmap_lock;



/**
 * allocDirBlock allocates a new data block for a directory.
 * @param inode inode of the directory that needs a new block for entries
 * @return 1 if block limit of 15 has reached, 0 if there is space in disk,
 *          blockNumber if successfully allocated
 */
int allocDirBlock(struct ext2_inode *inode) {

    if ((inode->i_blocks/2) == 15) {
        //Max limit reached
        return 1;
    }

    unsigned int availableBlock = firstAvailableBlock();
    if (availableBlock == 0) {
        //No space available
        return 0;
    }else {
        // Add it to the the inode blocks list
        inode->i_blocks += 2;
        inode->i_block[(inode->i_blocks/2)-1] = availableBlock;
        // sb->s_blocks_count += 1;
        // sb->s_free_blocks_count -= 1;

        superUpdater(0, -1);
        groupUpdater(0, -1,0);

        return availableBlock;
    }

}

/**
 * allocFileBlock allocates a new block for a file
 * @param inode of the file
 * @return 0 if disk-space is full
 *         1 if max limit has been reached
 *         2 if the block has been successfully allocated
 */
int allocFileBlock(struct ext2_inode *inode) {
    //Max file blocks = 256 + 12
    unsigned int availableBlock = firstAvailableBlock();
    if (availableBlock == 0) {
        //No space available
        return 0;
    }else {
        unsigned int allocInd = 0;
        if ((inode->i_blocks/2) > 12) {
            //Allocate in the indirect block
            int index = (inode->i_blocks/2) - 13;
            int indBlock = inode->i_block[12];
            int *bIndex = (int *)(disk + (EXT2_BLOCK_SIZE * indBlock));
            bIndex[index] = availableBlock; 
        }else if ((inode->i_blocks/2) == 12) {
            //Need to allocate a indirectBlock
            toggleBit(blockBitmap, availableBlock-1);
            unsigned int newIndBlock = firstAvailableBlock();
            if (newIndBlock == 0) {
                toggleBit(blockBitmap, availableBlock-1);
                return 0;
            }
            toggleBit(blockBitmap, newIndBlock-1);
            inode->i_block[12] = availableBlock;
            int indBlock = inode->i_block[12];
            int *bIndex = (int *)(disk + (EXT2_BLOCK_SIZE * indBlock));
            bIndex[0] = newIndBlock;
            inode->i_blocks += 2;
            allocInd = 1;
            superUpdater(0,-1);
            groupUpdater(0,-1,0);
        }else {
            //There is space before the indirect index
            inode->i_block[(inode->i_blocks/2)] = availableBlock;
        }
        if (allocInd != 1) {
            toggleBit(blockBitmap, availableBlock-1);
        }
        inode->i_blocks += 2;
        superUpdater(0,-1);
        groupUpdater(0,-1,0);
    }

    return 2;

}

/**
 * deallocIndirectBlock deallocate blocks from the indirect block
 * @param block The block where the indirect block exists
 * @param dtBlocks Number of blocks that needs to be deleted
 * @param totalBlocks Total blocks that exist within the
 */
void deallocIndirectBlock(int block, int dtBlocks, int totalBlocks) {
    int *blocks = (int *)(disk + EXT2_BLOCK_SIZE * block);

    int index = totalBlocks -1;
    for (int i = 0; i < dtBlocks; i ++) {
        int block = blocks[index];
        toggleBit(blockBitmap, block-1);
        index -= 1;
    }
}

/**
 * deallocFileBlock deallocates the specified number of blocks for regular file within the image
 * the function should only be used when removing or overwriting a exsiting file
 * @param inode inode of the file from which the blocks will be deallocated
 * @param blocks number of blocks to deallocate from the inode (assumes blocks <= inode.i_blocks)
 */
void deallocFileBlock(struct ext2_inode *inode, int blocks) {

    if ((inode->i_blocks/2) <= 12) {
        //There is not indirect block
        for (int i = 0; i < blocks; i ++) {
            int block = inode->i_block[(inode->i_blocks/2) -1];
            toggleBit(blockBitmap, block-1);
            inode->i_blocks -= 2;

            superUpdater(0,1);
            groupUpdater(0,1,0);
        }
    }else {
        //There are blocks in the indirect block
        int amt = (inode->i_blocks/2) - 13;
        if (amt == blocks) {
            //Clear all blocks in the indirect
            deallocIndirectBlock(inode->i_block[12], amt, amt);
            toggleBit(blockBitmap, inode->i_block[12]-1);
            // sb->s_free_blocks_count -= 1;
            inode->i_blocks -= (amt * 2) + 2;

            superUpdater(0,amt+1);
            groupUpdater(0,amt+1,0);
            

        }else if (amt > blocks) {
            //clear amt blocks in the indirect block
            deallocIndirectBlock(inode->i_block[12], blocks, amt);
            inode->i_blocks -= (blocks * 2);
            superUpdater(0,blocks);
            groupUpdater(0,blocks,0);
        }else {
            //clear all blocks in the indirect
            deallocIndirectBlock(inode->i_block[12], amt, amt);
            toggleBit(blockBitmap, inode->i_block[12]-1);
            superUpdater(0,amt+1);
            groupUpdater(0,amt+1,0);
            inode->i_blocks -= (amt * 2) + 2;
            //clear blocks in the main blocks array
            int left = blocks - amt;
            for (int i = 0; i < left; i ++) {
                int block = inode->i_block[(inode->i_blocks/2) -1];
                toggleBit(blockBitmap, block-1);
                inode->i_blocks -= 2;
                superUpdater(0,1);
                groupUpdater(0,1,0);
            }
        }
    }
}

/**
 * determineFileMode determines the file type based of the filemode
 * @param mode file mode of the inode
 * @return the corresponding fileType integer
 */
int determineFileMode(unsigned short mode) {

    if (((~mode) & EXT2_S_IFLNK ) == 0  ) {
        return EXT2_FT_SYMLINK;
    }else if ((mode & EXT2_S_IFREG) != 0) {
        return EXT2_FT_REG_FILE;
    }else if ((mode & EXT2_S_IFDIR) != 0) {
        return EXT2_FT_DIR;
    }

    return EXT2_FT_UNKNOWN;
}

/**
 * addDirectoryEntry creates and adds a new directory entry within
 * a directory data block. the function allocates a new block if the current is not valid
 * @param info  contains the inode of parent, item inode, accumulated rec_len in the last block of parent blocks
 * @param name  name of the new directory entry
 * @return 1 if successful, 0 if either the block limit has been reached/ or there is no space at all
 */
int addDirectoryEntry(struct dirEntryInfo info, char *name){
    struct ext2_inode *parentInode = (struct ext2_inode *)(&inodeTable[info.parentInode-1]);

    // Need to see if there is enough space in the current block.
    // There should be at least 8 bytes left.
    unsigned int padding = ((1 + ((strlen(name) - 1)/4)) * 4) - strlen(name);
    unsigned int totalEntrySize = sizeof(struct ext2_dir_entry) + strlen(name) + padding;
    unsigned int block = parentInode->i_block[parentInode->i_blocks/2 -1];
    struct ext2_dir_entry *position = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * block); 
    int memCheck = 0;
    if ((EXT2_BLOCK_SIZE - info.recLen) < totalEntrySize) {
        //New block is required
        block = allocDirBlock(parentInode);
        if (block <= 1) {
            return 0;
        }
        position = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * block); 
        memCheck = 1;
    }
    struct ext2_dir_entry *newEntry = position;
    if(memCheck == 0){
        struct ext2_dir_entry *pre_dir = findlastDir(block);
        unsigned int padding1 = (1 + ((pre_dir->name_len - 1)/4)) * 4 - pre_dir->name_len;
        unsigned int totalEntrySize1 = sizeof(struct ext2_dir_entry) + pre_dir->name_len + padding1;
        pre_dir->rec_len = totalEntrySize1;
        newEntry = (struct ext2_dir_entry *)((char*)position + info.recLen);
    }

    struct ext2_inode *itemInode = (struct ext2_inode *)(&inodeTable[info.itemInode-1]);
    newEntry->inode = info.itemInode;
    newEntry->file_type = determineFileMode(itemInode->i_mode); 
    newEntry->name_len = strlen(name);
    strncpy(newEntry->name, name, newEntry->name_len );
    if (memCheck == 0) {
        newEntry->rec_len =  EXT2_BLOCK_SIZE  - (info.recLen);
    }else {
       newEntry->rec_len =  EXT2_BLOCK_SIZE  - totalEntrySize; 
    }

    return 1;
}

/**
 * init_dir enters the . and .. directory entries in a new directory
 * @param block_num block that was allocated for the directory
 * @param new_inode inode of the new directory
 * @param par_inode inode of the parent directory
 */
void init_dir(int block_num, unsigned int new_inode, unsigned int par_inode){
    struct ext2_dir_entry *position = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * block_num); 
    position->inode = new_inode; 
    position->rec_len = 12;    
    position->name_len = 1;    
    position->file_type = EXT2_FT_DIR;    
    strncpy(position->name, ".", 1);
    position = (struct ext2_dir_entry*)(((char*)position)+12);
    position->inode = par_inode; 
    position->rec_len = EXT2_BLOCK_SIZE-12;    
    position->name_len = 2;    
    position->file_type = EXT2_FT_DIR;    
    strncpy(position->name, "..", 2);   
    struct ext2_inode *parentInode = (struct ext2_inode*)(&inodeTable[par_inode-1]);
    struct ext2_inode *current = (struct ext2_inode*)(&inodeTable[new_inode-1]);
    parentInode->i_links_count += 1;
    current->i_links_count += 1;
    current->i_size = EXT2_BLOCK_SIZE;
    groupUpdater(0,0,1);

}

/*
update the group descriptor by the certain among
*/
void groupUpdater(int inodeUsed, int blockUsed, int dir){
    groupDesc->bg_free_inodes_count += inodeUsed;
    groupDesc->bg_free_blocks_count += blockUsed;
    groupDesc->bg_used_dirs_count   += dir;
}

/*
update the group descriptor by the certain among
*/
void superUpdater(int inodeUsed, int blockUsed){
    sb->s_free_inodes_count += inodeUsed;
    sb->s_free_blocks_count += blockUsed;
}


/*
acquire lock and the inode bitmap block
*/
void acquiredBitmap() {
    pthread_mutex_lock(inode_bitmap_lock);
    pthread_mutex_lock(block_bitmap_lock);
}

/*
release the lock for block and inode bitmap
*/
void releaseBitmap() {
    pthread_mutex_unlock(inode_bitmap_lock);
    pthread_mutex_unlock(block_bitmap_lock);
}


/**
 * toggleBit toggles a bit in the bitmap
 * @param bitmap represents the bitmap in which the toggle will occur
 * @param bit valid bit position within the bitmap
 */
void toggleBit(unsigned char *bitmap, int bit) {
    int byteCount = bit/8;
    int bitCount  = bit % 8;
    unsigned char *byteSec = bitmap + byteCount;
    *byteSec ^= 1 << (bitCount);
}

/**
 * updateBitmaps updates the inodeBitmap and blockBitmap to be in use
 * @param space holds the bits in the inodeBitmap and blockBitmap that needs
 * to get toggled
 */
void updateBitmaps(struct bitmapSpace space) {
    toggleBit(inodeBitmap, space.inodeBit);
    toggleBit(blockBitmap, space.blockBit - 1);
}

/**
 * createInode, initializes a new inode on disk, and allocates the first block
 * inodeSize: 128B
 * @param fileMode (f)ile  d(irectory) l(ink)
 * @param space
 * @return the Inodetable index
 */
int createInode(unsigned short fileMode, struct bitmapSpace space) {
    struct ext2_inode *newInode = (struct ext2_inode *)(&inodeTable[space.inodeBit]);
    // Important items
    newInode->i_mode = fileMode;
    newInode->i_links_count = 1;
    newInode->i_blocks = 2;
    newInode->i_block[0] = space.blockBit;    
    //update bitmap
    updateBitmaps(space);
    superUpdater(-1,-1);
    groupUpdater(-1,-1,0);
    // Unimportant items
    newInode->i_uid = newInode->i_gid = 0;
    newInode->i_file_acl = newInode->i_dir_acl = newInode->i_faddr = 0;
    newInode->osd1 = 0;
    newInode->i_generation = 0;
    newInode->i_dtime = 0;
    return space.inodeBit + 1;
}

/**
 * checkBitmapSpace returns the bit count of the first available bit within the bitmaps.
 * The unreserved section of the inodeBitmap is used when determining the first space.
 * Assuming that disk is a global variable
 * @param inodeBitmap represents the inode bitmap
 * @param blockBitmap
 * @return
 */
struct bitmapSpace checkBitmapSpace(void) {
    struct bitmapSpace space;
    space.inodeBit = -1;
    space.blockBit = -1;
    unsigned int blockSpace = firstAvailableBlock();
    if (blockSpace == 0) {
        return space;
    }
    unsigned int inodeSpace = firstAvailableInode();
    if (inodeSpace == 2) {
        return space;
    }
    space.blockBit = blockSpace;
    space.inodeBit = inodeSpace;
    return space;
    // Writing to memory is essentially just writing to memory
    // ie dereferencing memory and assigning it.
}


/**
 * firstAvailableBlock returns the bit count of the first available block
 * @param blockBitmap extracted from disk
 * @param blockCount  extracted from the superblock
 * @return 0 if no block is found, other wise bitCount
 */
unsigned int firstAvailableBlock() {
    for (int byte = 0; byte < (sb->s_blocks_count) / 8; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            int in_uses = (blockBitmap[byte] >> bit) & 1;
            if (!in_uses) {
                //First available block found
                return (byte * 8) + bit + 1;
            }
        }
    }

    return 0;
}


/**
 * firstAvailableInode returns the bit count of the first available inodeTable index that
 * exists in the un-reserved section of the inodeTable
 * @param inodeBitmap extracted from disk
 * @param inodeCount  extracted from subperblock
 * @return 2 if table is full, count otherwise
 */
unsigned int firstAvailableInode() {

    int count = EXT2_GOOD_OLD_FIRST_INO;
    //Each byte is 8 bits
    //Count represents the bits from position 11
    while (count < sb->s_inodes_count) {
        int in_uses = inode_valid(inodeBitmap, sb->s_inodes_count, count);
        if (in_uses != 1) {
            return count;
        }

        count++;
    }

    return 0;
}


/**
 * checkDirBlockSpace determines if there is enough space to add a new directory entry.
 * @param name name that the directory entry will hold
 * @param par_inode parent inode
 * @param acc_rec_len accumulated recLen without the final padding
 * @return 0 if parent needs a new block, and there are atleast 2 blocks available on disk
 *         1 if parent still has space with the current allocated space
 *         2 if parent needs space, and the disk doesn't have space to accumulate atleast 2 blocks
 */
int checkDirBlockSpace(char *name, unsigned int par_inode, int acc_rec_len){
    unsigned int padding = ((1 + ((strlen(name) - 1)/4)) * 4) - strlen(name);
    unsigned int size = sizeof(struct ext2_dir_entry) + strlen(name) + padding;
    if(acc_rec_len == EXT2_BLOCK_SIZE || size+padding > (EXT2_BLOCK_SIZE - acc_rec_len)){
        if(sb->s_free_blocks_count >= 2){
            return 0;
        }
        return 2;
    }
    return 1;
}

/*
return
     2: found this path
     1: found the path but bar is file
     0: path before bar is valid, and no bar(file or dir) under that path
     -1: path not vaild before we reach the bar
*/
int check_vaild_path(const char *path, unsigned int *inode, char *new_name, unsigned int *acc_rec_len_ptr){
    char *rest = strdup(path);
    char *dir_name, *token;
    unsigned int curr_inode_index = EXT2_ROOT_INO-1;
    struct ext2_inode curr_inode;
    // decomposite the path
    token = strtok_r(rest, "/", &rest);
    dir_name = token;
    while(dir_name){
        token = strtok_r(rest, "/", &rest);
        
        // check inode bitmap for current inode
        if(inode_valid(inodeBitmap, sb->s_inodes_count, curr_inode_index)){
            curr_inode = inodeTable[curr_inode_index];
            if(curr_inode.i_mode&EXT2_S_IFDIR){
                //check name exist under the blocks of current inode 
                for(int i=0; curr_inode.i_block[i] != 0; i++){
                    // dir_name_valid will update *curr_inode_index
                    // 2 is dir; 1 is file
                    int valid = dir_name_valid(curr_inode.i_block[i], dir_name, acc_rec_len_ptr, &curr_inode_index, 2); 
                    // dir name not exist and this is new name
                    if(valid == -1 && token == NULL){
                        // token = NULL means we are on the last one
                        // dir_name is the directory name we gonna create
                        *inode = curr_inode_index + 1;
                        strcpy(new_name, dir_name);
                        return 0;
                    }
                    // dir name not exist and we still check the path(invalid path)
                    if(valid != 0 && token != NULL){
                        *inode = curr_inode_index + 1;
                        return -1;
                    }
                    // dir name exist and this is the new name(name already exist)
                    if(valid == 0 && token == NULL){
                        *inode = curr_inode_index + 1;
                        return 2;
                    }
                    if(valid == -2 && token == NULL){
                        *inode = curr_inode_index + 1;
                        return 1;
                    }
                }
            }
        }
        // inode not valid
        else{
            return -1;
        }
        // update the previous dir_name
        dir_name = token;
    }
    return 3;
}


/*
return non zero if inode is valid
return 0 otherwise
*/
int inode_valid(unsigned char *inodeBitmap, int total_inode, unsigned int inode){
    int total_inode_byte = total_inode/8;
    int inode_byte_position = inode/8;
    if(inode_byte_position > total_inode_byte){return 0;}
    int inode_bit_position = inode % 8;
    return (inodeBitmap[inode_byte_position] >> inode_bit_position) & 1;
}

/**
 * findlastDir finds the last directory entry in the given block.
 * The function assumes that the given block has been allocated for a directory
 * @param block_num block holding directory entries
 * @return pointer to the last directory entry
 */
struct ext2_dir_entry *findlastDir(int block_num){
    int count = 0;
    struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry*)(disk + 1024*block_num);
    int distance_to_next = dir_entry->rec_len;
    count += distance_to_next;
    if(count == EXT2_BLOCK_SIZE){
        return dir_entry;
    }
    struct ext2_dir_entry *next_dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry)+distance_to_next);
    while(count < EXT2_BLOCK_SIZE){
        distance_to_next = next_dir_entry->rec_len;
        count += next_dir_entry->rec_len;
        if(count == EXT2_BLOCK_SIZE){
            return next_dir_entry;
        }
        next_dir_entry = (struct ext2_dir_entry*)(((char*)next_dir_entry)+distance_to_next);
    }

    return dir_entry;
}

/**
 * findDirEntry finds the directory entry that holds the given name.
 * The function assumes that the entry exists in this directory entry
 * @param parentInode Directory inode in which the inode exists
 * @param name name of ther required dir-entry Name
 * @return a pointer to the directory-entry, NULL otherwise
 */
struct ext2_dir_entry *findDirEntry(struct ext2_inode *parentInode, char *name) {

    int i = 0;
    unsigned  int *recLen = 0;
    unsigned  int *currInodeIndex = 0;

    while (i < parentInode->i_blocks) {
        int verifyBlock = dir_name_valid(parentInode->i_block[i], name, recLen, currInodeIndex, EXT2_FT_REG_FILE);
        if (verifyBlock == 0) {
            struct ext2_dir_entry * entry = (struct ext2_dir_entry *)(disk + ((EXT2_BLOCK_SIZE * parentInode->i_block[i]) + *recLen ));
            return entry;
        }else {
            i += 1;
        }
    }
    return NULL;
}


// return 0 if dirctory exist;
// -2 name is same but type is different 
// -1 could not find any
int dir_name_valid(int block_num, char *name, unsigned int *acc_rec_len, unsigned int *curr_inode_index, int type){
    int count = 0; // total distance of a directory block
    unsigned int len, padding;
    *acc_rec_len = 0;
    struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry*)(disk + 1024*block_num);
    int distance_to_next = dir_entry->rec_len;
    // remove name padding
    char entry_name[EXT2_NAME_LEN];
    memset(entry_name, '\0', sizeof(entry_name));
    strncpy(entry_name, dir_entry->name, dir_entry->name_len);
    count += dir_entry->rec_len;
    if(count == EXT2_BLOCK_SIZE){
        len = dir_entry->name_len + (sizeof(struct ext2_dir_entry));
        padding =  (1 + ((dir_entry->name_len - 1) / 4)) * 4 - (dir_entry->name_len);
        *acc_rec_len += (len+padding);
    }
    else{
        *acc_rec_len = count;
    }
    if(strcmp(name, entry_name)==0){
        if(dir_entry->file_type != type){
            *curr_inode_index = dir_entry->inode - 1;
            return -2;
        }
        *curr_inode_index = dir_entry->inode - 1;
        return 0;
    }
    struct ext2_dir_entry *next_dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry)+distance_to_next);
    // print the rest entry in this block
    while(count < EXT2_BLOCK_SIZE){
        memset(entry_name, '\0', sizeof(entry_name));
        distance_to_next = next_dir_entry->rec_len;
        strncpy(entry_name, next_dir_entry->name, next_dir_entry->name_len);
        count += next_dir_entry->rec_len;
        if(count == EXT2_BLOCK_SIZE){
            len = next_dir_entry->name_len + (sizeof(struct ext2_dir_entry));
            padding =  (1 + ((next_dir_entry->name_len - 1) / 4)) * 4 - (next_dir_entry->name_len);
            *acc_rec_len += (len+padding);
        }
        else{
            *acc_rec_len = count;
        }
        if(strcmp(name,entry_name)==0){
            if(next_dir_entry->file_type != type){
                *curr_inode_index = next_dir_entry->inode - 1;
                return -2;
            }
            *curr_inode_index = next_dir_entry->inode - 1;
            return 0;
        }
        next_dir_entry = (struct ext2_dir_entry*)(((char*)next_dir_entry)+distance_to_next);
    }
    return -1;
}

/**
 * inode_print, prints the content of a specific inode
 * @param inodet inodeTable
 * @param index  index of the required inode within the inodetable
 */
void inode_print(struct ext2_inode *inodet, int index){
    char type = inode_type(inodet, index);
    int size = inodet[index].i_size;
    int links = inodet[index].i_links_count;
    int blocks = inodet[index].i_blocks;
    // the first line "blocks" refers to the number of blocks (which is 1 but since disk sectors are 512B and FS blocks are 1k then it's always double the count - - see the ext2 specs), while the second line lists all disk block numbers, separated by spaces. Only print up until you hit a 0, because the block pointers get filled in order. 
    printf("[%d] type: %c size: %u links: %d blocks: %d\n", index+1, type, size, links, blocks);
    printf("[%d] Blocks: ", index+1);
    for(int i = 0; inodet[index].i_block[i] != 0; i++){
        printf(" %d", inodet[index].i_block[i]);
    }
    printf("\n");

}

/**
 * inode_type the char representation of the inode type
 * @param inodet inodeTable
 * @param index  index at which the required inode exists
 * @return f - regular file, d - directory, s - link
 */
char inode_type(struct ext2_inode *inodet, int index){
    if(inodet[index].i_mode & EXT2_S_IFREG){
        return 'f';
    }
    else if(inodet[index].i_mode & EXT2_S_IFDIR){
        return 'd';
    }
    else{
        return 's';
    }
}

/**
 * print_bitmap prints the bits of the inodoBitmap and the blockBitmap
 */
void print_bitmap(){
    printf("Block bitmap: ");
    unsigned char *block_bits = (unsigned char*)(disk + 1024*groupDesc->bg_block_bitmap);
    for(int byte = 0; byte <(sb->s_blocks_count)/8; byte++){
        for(int bit = 0; bit < 8; bit ++){
            printf("%d", block_bits[byte] >> bit & 1);
        }
        printf(" ");
    }

    printf("\nInode bitmap: ");
    unsigned char *inode_bits = (unsigned char*)(disk + 1024*groupDesc->bg_inode_bitmap);
    for(int byte = 0; byte <(sb->s_inodes_count)/8; byte++){
        for(int bit = 0; bit < 8; bit ++){
            printf("%d", inode_bits[byte] >> bit & 1);
        }
        printf(" ");
    }

}

/*
calculate the total lenghth of a dir_entry based on the name
*/
int totallen(char *name){
    unsigned int len = strlen(name) + (sizeof(struct ext2_dir_entry));
    unsigned int padding =  (1 + ((strlen(name) - 1) / 4)) * 4 - (strlen(name));
    return len+padding;
}


/*
return the error status based on the path
*/
int cpPathfilter(const char *dst, int error_status, char *last_src_name, unsigned int inode, unsigned int *acc_rec_len_ptr, unsigned int *existing_inode_index, int *overwrite, int *block_num){

    struct ext2_inode par_inode = inodeTable[inode-1];
    if(error_status == -1){
       return ENOENT;
    }
    if(error_status == 2 || error_status == 3){
            int state;
           // check under the last dir
           for(int i=0;par_inode.i_block[i] != 0; i++){
               // state:
               // 0 found same type with same name
               // -2 name same but type different
               // -1 could not find any
               state = dir_name_valid(par_inode.i_block[i], last_src_name, acc_rec_len_ptr, existing_inode_index, EXT2_FT_REG_FILE);
               *block_num = par_inode.i_block[i];
               if(state == 0){
                   // overwrite
                   // existing_inode_index contain the dup inode
                   *overwrite = 1;
                   break;
               }
               if(state == -2){
                   // error
                   return ENOENT;
               }
           }
           if(state != 0){
           // new
           // not find anything at all under parent dir block
           // acc_rec_len_ptr represent the last block of parent
           // inode is parent
           *overwrite = 0;
            }
       }
    else if(dst[strlen(dst)-1] != '/' && error_status == 1){
       *overwrite = 1;
    }
    else if(dst[strlen(dst)-1] != '/' && error_status == 0){
       *overwrite = 0;
    }
    else{
       return ENOENT;
    }
    if(*overwrite){
        *acc_rec_len_ptr -= totallen(last_src_name);
    }
    return 0;
    

}

/**
 * checkFileReqSpace checks if the file that is getting overridden has enough space allocated, or do we need less space
 * or do we need more space
 * @param fileSize size of the file that is being copied in bytes
 * @param fileName name of the file
 * @param pInode   inode of the directory that contains this file dir-entry
 * @return 0 - Same amount of space required,
 *         1 - less space is required,
 *         requriedBlocks - if more blocks are required
 */
int checkFileReqSpace(int fileSize, char * fileName, unsigned int pInode, struct ext2_dir_entry * fileEntry) {
    // struct ext2_dir_entry * fileEntry = findDirEntry(parentInode, fileName);
    struct ext2_inode * fileInode = &inodeTable[fileEntry->inode-1];

    //Take the floor of the total blocks required
    unsigned int totalBlocksReq = ((fileSize - 1) / EXT2_BLOCK_SIZE) + 1;
    
    unsigned int currentAllocBlocks = fileInode->i_blocks/2;
    if (currentAllocBlocks > 12){
        currentAllocBlocks -= 1;
    }
    if (totalBlocksReq == currentAllocBlocks) {
        //requires the same amount of blocks
        return 0;
    }else if (totalBlocksReq < currentAllocBlocks) {
        //requires the less blocks than the allocated blocks
        return 1;
    }else {
        //requires more blocks
        if (sb->s_free_blocks_count > totalBlocksReq) {
            return totalBlocksReq;
        }else {
            return -1;
        }
    }
}

/**
 * writeFileToDisk writes the content of a file in the allocated blocks stored within the file inode
 * This function assumes that the fileInode holds enough blocks such that all of the file-content can
 * be written to it
 * @param fileMap Pointer mapping of the content(file) that is going to be written to the blocks
 * @param fileInode inode of the file that will hold this data
 * @param fileSize  size of the file that holds the new content
 * @return 0 on success
 */
int writeFileToDisk(const char*fileMap, struct ext2_inode * fileInode, int fileSize) {

    if (fileSize <= EXT2_BLOCK_SIZE) {
        //Single block is needed
        unsigned char *blocks = (disk + (EXT2_BLOCK_SIZE * fileInode->i_block[0]));
        memcpy(blocks, fileMap, fileSize);
        fileInode->i_size = fileSize;

    }else {

        int blockLst[(fileInode->i_blocks/2)  - 1];
        int *blockList;
        for (int i = 0; i < fileInode->i_blocks/2; i ++) {
            if (i < 12) {
                blockLst[i] = fileInode->i_block[i];
            }else if (i == 12){
                int block = fileInode->i_block[12];
                blockList = (int *)(disk + (EXT2_BLOCK_SIZE * block));
            }else {
                blockLst[i-1] = blockList[i-13];
            }
        }
        int totalBlocks = fileSize / EXT2_BLOCK_SIZE;
        int i = 0;
    
        while(i < totalBlocks){
            unsigned char *blocks = (disk + (EXT2_BLOCK_SIZE * blockLst[i]));
            memcpy(blocks, fileMap, EXT2_BLOCK_SIZE);
            fileMap = fileMap + EXT2_BLOCK_SIZE;
            i ++;
        }
        unsigned char *blocks = (disk + (EXT2_BLOCK_SIZE * blockLst[fileInode->i_blocks/2  - 2]));
        memcpy(blocks, fileMap, fileSize % EXT2_BLOCK_SIZE);
        fileInode->i_size = fileSize;
    }

    return 0;
}


/**
 * overwrite over writes a preexisting file within the image with the contents of the new file that
 * is being copied. The function modifies the the exsiting the directoryEntry and the inode of the file
 * that is being copied
 * @param cnf mapping of the newFile and the preexisting configuration of the existing file
 * @return -1 if there is not enough space, 0 on success
 */
int overwriteFile(struct newFileConfig cnf) {

    //have access to the fileInode
    
    struct ext2_dir_entry * fileEntry = (struct ext2_dir_entry *)(((char * )(struct ext2_dir_entry *)(disk + (EXT2_BLOCK_SIZE * cnf.blkNum))) + cnf.recLen);

    int spaceCheck = checkFileReqSpace(cnf.fileSize, cnf.fileName, cnf.pInode, fileEntry);
    if (spaceCheck == -1) {
        return -1;
    }else {
        //We already know that there is enough space in disk if allocation is required
        struct ext2_inode * fileInode = &inodeTable[fileEntry->inode - 1];
        //256 + 12
        if (spaceCheck == 0) {
            //Same amount of blocks are required
            writeFileToDisk(cnf.fileMap, fileInode, cnf.fileSize);
        }else if (spaceCheck == 1) {
            //De-allocate blocks
            int deBlocks = (fileInode->i_blocks/2) - (((cnf.fileSize - 1) / EXT2_BLOCK_SIZE) + 1);
            if ((fileInode->i_blocks/2) > 12) {
                deBlocks -= 1;
            }
            deallocFileBlock(fileInode, deBlocks);
            writeFileToDisk(cnf.fileMap, fileInode, cnf.fileSize);
        }else {
            //Need more blocks
            int nodeBlocks = fileInode->i_blocks/2;
            if (nodeBlocks > 12) {
                nodeBlocks -= 1;
            }
            int requiredBlocks = spaceCheck - nodeBlocks;

            if ((fileInode->i_blocks/2) > 12){
                requiredBlocks += 1;
            }
            for (int i = 0; i < requiredBlocks; i ++) {
                allocFileBlock(fileInode);
            }
            writeFileToDisk(cnf.fileMap,fileInode, cnf.fileSize);
        }
        return 0;
    }
}

/**
 * newFile is used when we are writing a new file onto disk
 * @param cnf holds required configuration to create a new dirEntry in parent and the file parameters
 * @return -1 if there is not enough space, 0 for success
 */
int newFile (struct newFileConfig cnf) {
    //Follow the same procedure with making a directory entry
    int dirSpaceCheck = checkDirBlockSpace(cnf.fileName, cnf.pInode, cnf.recLen);
    unsigned int totalBlockReq = 0;
    unsigned int fileBlocksReq = ((cnf.fileSize - 1) / EXT2_BLOCK_SIZE) + 1;
    if (dirSpaceCheck == 2){
        //Not enough space
        return -1;
    }else if (dirSpaceCheck == 0) {
        //Parent requires a block for an entry
        totalBlockReq += 1;
    }
    totalBlockReq += fileBlocksReq;
    if (sb->s_free_blocks_count >= totalBlockReq) {
        //Create iNode
        struct bitmapSpace space = {firstAvailableBlock(), firstAvailableInode()};
        int inodeNum = createInode(EXT2_S_IFREG, space);

        //Add the directory entry
        struct dirEntryInfo entryInfo = {
                cnf.pInode,
                inodeNum,
                cnf.recLen
        };
        addDirectoryEntry(entryInfo, cnf.fileName);

        //Allocate enough blocks for the file
        struct ext2_inode * fileInode = (struct ext2_inode *)(&inodeTable[inodeNum - 1]);
        for (int i = 0; i < (fileBlocksReq-1); i ++) {
            allocFileBlock(fileInode);
        }
        writeFileToDisk(cnf.fileMap, fileInode, cnf.fileSize);
        return 0;
    }

    return -1;
}


/*
Function that create a new symbolic link
*/
int newSymbolicLink(unsigned int pInode, const char *path, unsigned int recLen, char *name){
    
    unsigned int spaceValidation = checkDirBlockSpace(name, pInode, recLen);
    if (spaceValidation == 2) {
        return ENOSPC;
    }
    
    unsigned int inodeSpace = firstAvailableInode();
    unsigned int blockSpace = firstAvailableBlock();
    
    if (inodeSpace == -1 || blockSpace == -1) {
        return ENOSPC;
    }

    struct bitmapSpace space = {
        blockSpace,
        inodeSpace
    };
    unsigned int newInode = createInode(EXT2_S_IFLNK, space);
    struct dirEntryInfo info = {
        pInode,
        newInode,
        recLen
    };

    addDirectoryEntry(info,name);
    struct ext2_inode *fileInode = (struct ext2_inode *)(&inodeTable[newInode -1]);
    char *block = (char *)(disk + (EXT2_BLOCK_SIZE * fileInode->i_block[0]));
    
    memcpy(block, path, strlen(path));

    fileInode->i_size = (unsigned int) strlen(path);
    return 0;
}

/*
Function that remove a file based the inode given
*/
int removeFile(unsigned int pInode, unsigned int recLen, struct ext2_dir_entry * fileEntry ,unsigned int blockNum, struct ext2_dir_entry * prevEntry) {
    unsigned int fInode = fileEntry->inode - 1;

    struct ext2_inode * fileInode = (struct ext2_inode *)(&inodeTable[fInode]);
    //deallocate Blocks
    if (fileInode->i_links_count == 1){
        // v this should also take care of the indirect block
        if (fileInode->i_blocks > 12){
            deallocFileBlock(fileInode, (fileInode->i_blocks/2)-1);
        }else {
            deallocFileBlock(fileInode, (fileInode->i_blocks/2));
        }
        //Update the inode Bitmap
        fileInode->i_links_count -= 1;
        toggleBit(inodeBitmap,(int)fInode);
        superUpdater(1,0);
        groupUpdater(1,0,0);
    }else{
        fileInode->i_links_count -= 1;
    }
    if (recLen == 0){
        fileEntry->inode = 0;
    }else {
        prevEntry->rec_len += fileEntry->rec_len;

    }
    return 0;
}

/*
Prints super block info
*/
void printSuperBlockInfo(unsigned int blocks) {
    printf("------------ SuperBlock Info ----------------\n");
    printf("freeInodes: %d\n", sb->s_free_inodes_count);
    printf("freeBlocks: %d\n", sb->s_free_blocks_count);
    printf("TotalBlocks Allocated: %d\n", blocks);
}

/*
Helper function for checking validation for rm case
*/
int rm_path_validation(const char *path, int error_status, char *last_src_name, unsigned int inode, unsigned int *acc_rec_len_ptr, int *block_num, unsigned int *existing_inode_index){

    struct ext2_inode par_inode = inodeTable[inode-1];
    // error status:
    // /foo/bar
    // 2: found this path
    // 1: found the path but bar is file
    // 0: path before bar is valid, and no bar(file or dir) under that path
    // -1: path not vaild before we reach the bar
    if(error_status == -1 || error_status == 0 || (error_status == 1 && path[strlen(path)-1] == '/') ){
       return ENOENT;
    }
    if(error_status == 2 || path[strlen(path)-1] == '/'){
        return EISDIR;
    }

    for(int i=0;par_inode.i_block[i] != 0; i++){
        int state;
        state = dir_name_valid(par_inode.i_block[i], last_src_name, acc_rec_len_ptr, existing_inode_index, EXT2_FT_REG_FILE);
        *block_num = par_inode.i_block[i];
        if(state == 0){
        // found it
            break;
        }
    }
    return 0;
}
