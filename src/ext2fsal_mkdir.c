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


//TODO update the number of links in the parent directory and the current directory
// Each . and .. represents a link
int32_t ext2_fsal_mkdir(const char *path)
{
    /**
     * TODO: implement the ext2_mkdir command here ...
     * the argument path is the path to the directory to be created.
     */
    acquiredBitmap();

    unsigned int inode = -1;
    char new_name[EXT2_NAME_LEN];
    unsigned int acc_rec_len_ptr = 0;
    int error_status = check_vaild_path(path, &inode, new_name, &acc_rec_len_ptr);
    if(error_status == 2){
        releaseBitmap();
    
        return EEXIST;
    }
    if(error_status == -1 || error_status == 1 || error_status == 3){
        releaseBitmap();
        return ENOENT;}

    int check = checkDirBlockSpace(new_name, inode, acc_rec_len_ptr);
    if(check == 2){
        releaseBitmap();
        return ENOSPC;
    }
    struct bitmapSpace av_space = checkBitmapSpace();
    if(av_space.inodeBit == -1 || av_space.blockBit == -1){
        releaseBitmap();
        return ENOSPC;
    }
    unsigned int new_inode = createInode(EXT2_S_IFDIR, av_space);
    struct dirEntryInfo dir_info = {inode, new_inode, acc_rec_len_ptr};
    if(addDirectoryEntry(dir_info, new_name) == 0){
        releaseBitmap();
        return ENOSPC;
    }
    init_dir(inodeTable[new_inode-1].i_block[0], new_inode, inode);
    

    releaseBitmap();

    return 0;
}
