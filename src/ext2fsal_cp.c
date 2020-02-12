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
#
#include "ext2fsal.h"
#include "e2fs.h"
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

//TODO copying to a newFile is not getting named correctly /newFile.txt --> /checkFile should create a new file named checkFile with newFile.txt contents
int32_t ext2_fsal_cp(const char *src,
                     const char *dst)
{
    /**
     * TODO: implement the ext2_cp command here ...
     * src and dst are the ln command arguments described in the handout.
     */

     /* This is just to avoid compilation warnings, remove these 2 lines when you're done. */

    acquiredBitmap();

    struct stat s;
    size_t size;
    FILE *file;
    file = fopen(src, "r");
    if (file == NULL){
    }
    int fd = fileno(file);
    if(fd < 0){
        releaseBitmap();
       return ENOENT;
    }
    fstat(fd, &s);
    size = s.st_size;
    
    const char *src_array = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0); 
    
    char *rest = strdup(src);
    char *last_src_name, *token;
    // decomposite the src path
    token = strtok_r(rest, "/", &rest);
    last_src_name = token;
    while((token = strtok_r(rest, "/", &rest))){
       last_src_name = token;
    }
    
// decomposite the dst path
    rest = strdup(dst);
    token = strtok_r(rest, "/", &rest);
    char *last_dst_name = token;
    while((token = strtok_r(rest, "/", &rest))){
       last_dst_name = token;
    }

    unsigned int inode = EXT2_ROOT_INO;
    char new_name[EXT2_NAME_LEN];
    unsigned int acc_rec_len_ptr = 0;
    struct ext2_inode root = inodeTable[inode-1];
    int block_num = root.i_block[root.i_blocks/2 - 1];
    // error status:
    // /foo/bar
    // 2: found this path
    // 1: found the path but bar is file
    // 0: path before bar is valid, and no bar(file or dir) under that path
    // -1: path not vaild before we reach the bar
    int error_status = check_vaild_path(dst, &inode, new_name, &acc_rec_len_ptr);
    unsigned int existing_inode_index = -1;
    int overwrite = 0;
    int err = cpPathfilter(dst, error_status, last_src_name, inode, &acc_rec_len_ptr, &existing_inode_index, &overwrite, &block_num);
    if(err != 0){
        releaseBitmap();
        return err;
    }
    char *new_file_name;
    if(error_status == 2 && overwrite == 0){
        new_file_name = last_src_name; 
    }
    else{
        new_file_name = last_dst_name;
    }
    struct newFileConfig config = {
        src_array,
        size,
        new_file_name,
        inode,
        block_num,
        acc_rec_len_ptr
    };

    if (overwrite == 1) {
        overwriteFile(config);
    }else {
        newFile(config);
    }
       
    close(fd);
    releaseBitmap();
    return 0;
}
