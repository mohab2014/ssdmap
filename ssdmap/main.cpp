//
//  main.cpp
//  ssdmap
//
//  Created by Raphael Bost on 17/03/2016.
//  Copyright © 2016 Raphael Bost. All rights reserved.
//

#include <iostream>

#include "mmap_util.h"

int main(int argc, const char * argv[]) {

    size_t length = 10  * 1024; // 10 kB
    
    mmap_st mmap = create_mmap("map.bin",length);
    
    uint32_t *array = (uint32_t*)mmap.mmap_addr;
    
    printf("array[0] = %u\n", array[0]);
    
    array[0] = 0x12345678;
    
    printf("array[0] = %u\n", array[0]);

//    close_mmap(mmap);
    destroy_mmap(mmap);
    
    return 0;
}
