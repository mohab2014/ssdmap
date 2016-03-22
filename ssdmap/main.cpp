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

static uint64_t x=123456789, y=362436069, z=521288629, w=314159;
//static uint64_t x, y, z, w;

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
    
    remove("bucket_map.bin.0");
    remove("bucket_map.bin.1");
    remove("bucket_map.bin.2");
    remove("bucket_map.bin.3");
    remove("bucket_map.bin.4");
    remove("bucket_map.bin.5");
    remove("bucket_map.bin.6");
    remove("bucket_map.bin.7");
    remove("bucket_map.bin.8");
    remove("bucket_map.bin.9");
    remove("bucket_map.bin.10");
    remove("bucket_map.bin.11");
    remove("bucket_map.bin.12");
    remove("bucket_map.bin.13");
    
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
    
    
    bucket_map<uint64_t,uint64_t> bm(700); // 700 => 4 buckets
    std::map<uint64_t, uint64_t> ref_map;
//
//    bm.add(0, 0);
//    bm.add(1, 1);
//    bm.add(2, 2);
//    bm.add((1<<16)+100, 16);
//    
//    uint64_t v;
//    
//    bm.get(1, v);
//    
//    printf("map[1] = 0x%08llu\n", v);
//
//    bm.get((1<<16)+100, v);
//    printf("map[(1<<16)+100] = 0x%08llu\n", v);
    
//    size_t MAX_REP = 20;
//    size_t MAX_REP = 1e6;
    size_t MAX_REP = 1 << 23;
    
    for (size_t i = 0; i < MAX_REP; i++) {
        uint64_t k = xorshift128();
//        uint64_t k = i;
        bm.add(k, k);
        ref_map[k] = k;
//        std::cout << "Added\n";
        
        
//        for(auto &x : ref_map)
//        {
//            uint64_t v;
//            bool s = bm.get(x.first, v);
//            
//            assert(s);
//            assert(v == x.second);
//            
//        }

    }
    
    std::cout << "Done ...\n";
    
//    bm.full_resize();
    
    // tests
    
    // local
    // 1520619521903936
    
    // moved
    // 1708732815525741
    // 1521501166049613
    // 2580465799119907660

//    uint64_t v;
//    bool s = bm.get(16, v);
//    assert(s);
//    assert(v == 16);
    

    size_t count = 0;
    for(auto &x : ref_map)
    {
        uint64_t v;
        bool s = bm.get(x.first, v);
        
        assert(s);
        assert(v == x.second);
        
        count++;
    }
    
    return 0;
}
