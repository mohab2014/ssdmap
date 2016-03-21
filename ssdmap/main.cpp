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

//static uint64_t x=123456789, y=362436069, z=521288629;

//static uint64_t x=123456789, y=362436069, z=521288629, w=314159;
static uint64_t x, y, z, w;
uint64_t xorshift128(void) {
    uint64_t t = x;
    t ^= t << 11;
    t ^= t >> 8;
    x = y; y = z; z = w;
    w ^= w >> 19;
    w ^= t;
    return w;
}

int main(int argc, const char * argv[]) {

    srand (time(NULL));
    x = rand();
    y = rand();
    z = rand();
    w = rand();
    
    remove("bucket_map_0.bin");
    remove("bucket_map_1.bin");
    
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
    
    
    bucket_map<uint64_t,uint64_t> bm(1 << 16);

    bm.add(0, 0);
    bm.add(1, 1);
    bm.add(2, 2);
    bm.add((1<<16)+100, 16);
    
    uint64_t v;
    
    bm.get(1, v);
    
    printf("map[1] = 0x%08llu\n", v);

    bm.get((1<<16)+100, v);
    printf("map[(1<<16)+100] = 0x%08llu\n", v);
    
    size_t MAX_REP = 1 << 24;
    
//    for (size_t i = 0; i < MAX_REP; i++) {
//        uint64_t k = xorshift128();
//        bm.add(k, 0);
////        std::cout << "Added\n";
//    }
//    
    std::cout << "Done ...\n";
    
    return 0;
}
