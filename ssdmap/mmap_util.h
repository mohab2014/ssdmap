//
//  mmap_util.h
//  ssdmap
//
//  Created by Raphael Bost on 17/03/2016.
//  Copyright © 2016 Raphael Bost. All rights reserved.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
    
typedef struct
{
    void *mmap_addr;
    size_t length;
    int fd;
}mmap_st;
    
mmap_st create_mmap(const char *pathname, size_t length);
int close_mmap(mmap_st);
int destroy_mmap(mmap_st map);

#ifdef __cplusplus
}
#endif