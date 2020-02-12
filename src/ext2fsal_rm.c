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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


int32_t ext2_fsal_rm(const char *path)
{

    // get file name
    char pre_path[EXT2_NAME_LEN];
    char *rest = strdup(path);
    char *last_src_name, *token;
    // decomposite the path
    token = strtok_r(rest, "/", &rest);
    last_src_name = token;
    strcpy(pre_path, "/");
    while((token = strtok_r(rest, "/", &rest))){
       strcat(pre_path, last_src_name);
       strcat(pre_path, "/");
       last_src_name = token;
    }
    if(path[strlen(path)-1] == '/'){
        strcat(pre_path, last_src_name);
    }
    unsigned int pa_inode = EXT2_ROOT_INO;
    char new_name[EXT2_NAME_LEN];
    unsigned int acc_rec_len_ptr = 0;
    struct ext2_inode root = inodeTable[pa_inode-1];
    unsigned int existing_inode_index = pa_inode;
    int block_num = root.i_block[root.i_blocks/2 - 1];
    // error status:
    // /foo/bar
    // 2: found this path
    // 1: found the path but bar is file
    // 0: path before bar is valid, and no bar(file or dir) under that path
    // -1: path not vaild before we reach the bar
    // check the pre_path
    acquiredBitmap();
    check_vaild_path(pre_path, &pa_inode, new_name, &acc_rec_len_ptr);
    // check the full_path
    int error_status = check_vaild_path(path, &existing_inode_index, new_name, &acc_rec_len_ptr);
    int err = rm_path_validation(path, error_status, last_src_name, pa_inode, &acc_rec_len_ptr, &block_num, &existing_inode_index);
    if(err != 0){
        releaseBitmap();
        return err;
    }
    struct ext2_dir_entry * nextEntry = (struct ext2_dir_entry *)(disk + (EXT2_BLOCK_SIZE * block_num));
    struct ext2_dir_entry * prevEntry = NULL;
    unsigned int dirEntryPos = 0;
    while (nextEntry->inode != (existing_inode_index+1) && (strncmp(nextEntry->name,last_src_name, strlen(last_src_name))) != 0) {
        dirEntryPos += nextEntry->rec_len;
        prevEntry = nextEntry;
        nextEntry = (struct ext2_dir_entry *)((char*)nextEntry + nextEntry->rec_len);
    }
    removeFile(pa_inode, dirEntryPos, nextEntry, block_num, prevEntry);
    releaseBitmap();
    return 0;
}
