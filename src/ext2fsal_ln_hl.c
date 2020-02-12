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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


int32_t ext2_fsal_ln_hl(const char *src,
                        const char *dst)
{
    acquiredBitmap();
    unsigned int src_inode = EXT2_ROOT_INO;
    char src_name[EXT2_NAME_LEN];
    unsigned int acc_rec_len_ptr = 0;
    // error status:
    // /foo/bar
    // 2: found this path
    // 1: found the path but bar is file
    // 0: path before bar is valid, and no bar(file or dir) under that path
    // -1: path not vaild before we reach the bar
    int error_status = check_vaild_path(src, &src_inode, src_name, &acc_rec_len_ptr);
    if(error_status != 1){
        releaseBitmap();
        if(error_status == 2){return EISDIR;}
        return ENOENT;
    }
    unsigned int dst_inode = EXT2_ROOT_INO;
    char dst_name[EXT2_NAME_LEN];
    unsigned int acc_rec_len_dst = 0;
    int error_status_dst = check_vaild_path(dst, &dst_inode, dst_name, &acc_rec_len_dst);
    if(error_status_dst == -1){
        releaseBitmap();
        return ENOENT;
    }
    if(error_status_dst != 0){
        releaseBitmap();
        return EEXIST;
    }
    struct dirEntryInfo info = {
        dst_inode,
        src_inode,
        acc_rec_len_dst
    };
    addDirectoryEntry(info, dst_name);
    struct ext2_inode *fileInode = (struct ext2_inode *)(&inodeTable[info.itemInode-1]);
    fileInode->i_links_count += 1;
    releaseBitmap();
    return 0;
}
