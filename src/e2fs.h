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

#ifndef CSC369_E2FS_H
#define CSC369_E2FS_H

#include "ext2.h"
#include <string.h>
#include <errno.h>
#include <pthread.h>

/**
 * TODO: add in here prototypes for any helpers you might need.
 * Implement the helpers in e2fs.c
 */

struct bitmapSpace {
    unsigned int blockBit;
    unsigned int inodeBit;
};

struct dirEntryInfo {
    unsigned int parentInode;
    unsigned int itemInode;
    unsigned int recLen;
};

struct newFileConfig {
    const char *fileMap;
    int fileSize;
    char * fileName;
    unsigned int pInode;
    unsigned int blkNum;
    unsigned int recLen;
};



//------------- Helpers For Determing Space in Disk -------------------------
struct bitmapSpace checkBitmapSpace(void);
unsigned int firstAvailableBlock();
unsigned int firstAvailableInode();


 // .....
extern unsigned char *disk;
extern struct ext2_super_block *sb;
extern struct ext2_group_desc *groupDesc;
extern unsigned char *inodeBitmap;
extern unsigned char *blockBitmap;
extern struct ext2_inode *inodeTable;
// extern pthread_mutex_t *inode_locks;
extern pthread_mutex_t * inode_bitmap_lock;
extern pthread_mutex_t * block_bitmap_lock;

//----------- Path validation helpers
int check_vaild_path(const char *path, unsigned int *inode, char *new_name, unsigned int *acc_rec_len_ptr);
int inode_valid(unsigned char *inodeBitmap, int total_inode, unsigned int inode);
int dir_name_valid(int block_num, char *name, unsigned int *acc_rec_len_ptr, unsigned int *curr_inode_index, int type);
int rm_path_validation(const char *path, int error_status, char *last_src_name, unsigned int inode, unsigned int *acc_rec_len_ptr, int *block_num, unsigned int *existing_inode_index);

//----------- Directory entry helpers
int allocDirBlock(struct ext2_inode *inode);
int addDirectoryEntry(struct dirEntryInfo info, char *name);
int checkDirBlockSpace(char *name, unsigned int par_inode, int acc_rec_len);
struct ext2_dir_entry *findlastDir(int block_num);
void init_dir(int block_num, unsigned int new_inode, unsigned int par_inode);


//----------- Bitmap helpers
void toggleBit(unsigned char *bitmap, int bit);
void updateBitmaps(struct bitmapSpace space);
struct bitmapSpace checkBitmapSpace(void);

//------------ Lock Helpers
void acquiredBitmap();
void releaseBitmap();

//----------- updateer
void groupUpdater(int inodeUsed, int blockUsed, int dir);
void superUpdater(int inodeUsed, int blockUsed);

//----------- inode helpers
int createInode(unsigned short fileMode, struct bitmapSpace space);
char inode_type(struct ext2_inode *inodet, int index);

//----------- File helpers
int determineFileMode(unsigned short mode);
int newFile (struct newFileConfig cnf);
int overwriteFile(struct newFileConfig cnf);
int removeFile(unsigned int pInode, unsigned int recLen, struct ext2_dir_entry * fileEntry ,unsigned int blockNum, struct ext2_dir_entry * prevEntry);


//----------- Linker Helpers
int newSymbolicLink(unsigned int pInode, const char *path, unsigned int recLen, char *name);


//----------- Debugger printers
void printSuperBlockInfo(unsigned int blocks);
void print_bitmap();
void inode_print(struct ext2_inode *inodet, int index);

int totallen(char *name);
int cpPathfilter(const char *dst, int error_status, char *last_src_name, unsigned int inode, unsigned int *acc_rec_len_ptr, unsigned int *existing_inode_index, int *overwrite, int *block_num);



#endif
