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
#include <unordered_map>

#include "mmap_util.h"
#include "bucket_array.hpp"
#include "bucket_map.hpp"

int main(int argc, const char * argv[]) {

//    size_t length = 10  * 1024; // 10 kB
//    
//    mmap_st mmap = create_mmap("map.bin",length);
//    
//    uint32_t *array = (uint32_t*)mmap.mmap_addr;
//    
//    printf("array[0] = 0x%08x\n", array[0]);
//    
//    array[0] = 0x12345678;
//    
//    printf("array[0] = 0x%08x\n", array[0]);
//
//    size_t page_size = 4096;
//    bucket_array<uint64_t> ba(mmap.mmap_addr,1000,page_size);
//    
//    uint64_t* sub_array = ba.get_bucket_pointer(0);
//    sub_array[0] = UINT64_C(0x12345678a1a2a3a4);
//    
//    
//    printf("array[0] = 0x%08x\n", array[0]);
//
//    auto bucket = ba.bucket(0);
//    bucket.set_size(1);
//    
//    bucket.append(UINT64_C(0xffffffffffffffff));
//    
//    for (auto it = bucket.begin(); it != bucket.end(); ++it) {
//        printf("val: 0x%08llx\n", *it);
//    }
//    
//    //    close_mmap(mmap);
//    destroy_mmap(mmap);
//    
    
    
    bucket_map<uint64_t,uint64_t> bm;
    
    bm.add(0, 0);
    bm.add(1, 1);
    bm.add(2, 1);
    bm.add(3, 1);
    
    uint64_t v = -1;

    bm.get(3, v);
    printf("map[1] = 0x%08llu\n", v);

    bool r = bm.get(4,v);
    if (!r) {
        printf("4 was not mapped\n");
    }
    
    return 0;
}
