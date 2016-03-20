//
//  mmap_util.c
//  ssdmap
//
//  Created by Raphael Bost on 17/03/2016.
//  Copyright © 2016 Raphael Bost. All rights reserved.
//

#include "mmap_util.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>

#include <errno.h>

mmap_st create_mmap(const char *pathname, size_t length)
{
    off_t result;
    mmap_st map;
    
    map.length = 0;
    map.mmap_addr = NULL;
    
    map.fd = open(pathname, O_RDWR | O_CREAT, (mode_t)0600); // permissions set to rw-----
    if (map.fd == -1) {
        perror("Error opening file for writing");
        exit(EXIT_FAILURE);
    }

    result = lseek(map.fd, length-1, SEEK_SET);
    if (result == -1) {
        close(map.fd);
        perror("Error calling lseek() to 'stretch' the file");
        exit(EXIT_FAILURE);
    }
    

    // stretch the file if needed
    char buf[1]="";
    
    result = read(map.fd, buf,1);
    if (result != 1) {
        // the file was not streched
//        printf("Stretch!\n");

        result = write(map.fd, buf, 1);
        if (result != 1) {
            close(map.fd);
            perror("Error writing last byte of the file");
            exit(EXIT_FAILURE);
        }
    }

    
    // mmap the file
    map.mmap_addr = mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED, map.fd, 0);
    if (map.mmap_addr == MAP_FAILED) {
        close(map.fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }
    
    // we will use random access in our use case
    if(madvise(map.mmap_addr, length, MADV_RANDOM) == -1)
    {
        printf("Bad advice ...\n");
    }
    
    map.length = length;

    return map;
}

int close_mmap(mmap_st map)
{
    int ret;
    
    ret = msync(map.mmap_addr, map.length, MS_SYNC);

    if (ret == -1) {
        perror("Error syncing the map.");

        exit(EXIT_FAILURE);
    }
    
    if (munmap(map.mmap_addr, map.length) == -1) {
        perror("Error un-mmapping the file");
        /* Decide here whether to close(fd) and exit() or not. Depends... */
    }

    // close the file descriptor
    close(map.fd);
    
    return 0;
}

int destroy_mmap(mmap_st map)
{
    int ret;
    
    // unmap first
    if (munmap(map.mmap_addr, map.length) == -1) {
        perror("Error un-mmapping the file");
        /* Decide here whether to close(fd) and exit() or not. Depends... */
    }
    
    // get the path
    char file_path[MAXPATHLEN];
    
    ret = fcntl(map.fd, F_GETPATH, file_path);
    

    // close the file descriptor
    close(map.fd);
    
    // delete the file
    if (ret != -1) {
        if (remove(file_path)) {
            perror("Error deleting the mmap file.");
        }
    }else{
        perror("Unable to get the mmap file path.");
    }
    
    return 0;
}
