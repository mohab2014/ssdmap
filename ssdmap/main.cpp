//
//  main.cpp
//  ssdmap
//
//  Created by Raphael Bost on 17/03/2016.
//  Copyright © 2016 Raphael Bost. All rights reserved.
//

#include <iostream>
#include <array>
#include <cstdint>

#include "mmap_util.h"
#include "bucket_array.hpp"

int main(int argc, const char * argv[]) {

    size_t length = 10  * 1024; // 10 kB
    
    mmap_st mmap = create_mmap("map.bin",length);
    
    uint32_t *array = (uint32_t*)mmap.mmap_addr;
    
    printf("array[0] = 0x%08x\n", array[0]);
    
    array[0] = 0x12345678;
    
    printf("array[0] = 0x%08x\n", array[0]);

    size_t page_size = 4096;
    bucket_array<uint64_t, 1000> ba(mmap.mmap_addr,page_size);
    
    uint64_t* sub_array = ba.get_bucket_pointer(0);
    sub_array[0] = UINT64_C(0x12345678a1a2a3a4);
    
    printf("array[0] = 0x%08x\n", array[0]);

    
    //    close_mmap(mmap);
    destroy_mmap(mmap);
    
    return 0;
}
