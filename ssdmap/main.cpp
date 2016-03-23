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
#include <ftw.h>

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

void weak_correctness_check(size_t test_size)
{
    bucket_map<uint64_t,uint64_t> bm("w_correctness_map.dat",700); // 700 => 4 buckets
    std::map<uint64_t, uint64_t> ref_map;
    
    for (size_t i = 0; i < test_size; i++) {
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
}

void persistency_check(size_t test_size)
{
    bucket_map<uint64_t,uint64_t> *bm = new bucket_map<uint64_t,uint64_t>("persitency_test.dat",700); // 700 => 4 buckets
    std::map<uint64_t, uint64_t> ref_map;
    
    std::cout << "Fill the map ..." << std::flush;
    for (size_t i = 0; i < test_size; i++) {
//        uint64_t k = xorshift128();
                uint64_t k = i;
        bm->add(k, k);
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
    
    uint64_t v;
    bool s = bm->get(0, v);
    
    assert(s);
    assert(v == 0);

    std::cout << " done" << std::endl;
    
    std::cout << "Flush to disk ..." << std::flush;
    
    delete bm;

    std::cout << " done" << std::endl;

    std::cout << "Read from disk ..." << std::flush;
    
    bm = new bucket_map<uint64_t,uint64_t>("persitency_test.dat",700); // 700 => 4 buckets

    std::cout << " done" << std::endl;

    std::cout << "Test consistency ..." << std::flush;

    size_t count = 0;
    for(auto &x : ref_map)
    {
        uint64_t v;
        bool s = bm->get(x.first, v);
        
        assert(s);
        assert(v == x.second);
        
        count++;
    }
    std::cout << " done" << std::endl;
    
    delete bm;

}

/* Call unlink or rmdir on the path, as appropriate. */
int
rm( const char *path, const struct stat *s, int flag, struct FTW *f )
{
    int status;
    int (*rm_func)( const char * );
    
    switch( flag ) {
        default:     rm_func = unlink; break;
        case FTW_DP: rm_func = rmdir;
    }
    if( status = rm_func( path ), status != 0 )
        perror( path );
    else
        puts( path );
    return status;
}

void clean()
{
    if( nftw( "w_correctness_map.dat", rm, OPEN_MAX, FTW_DEPTH )) {
//        perror( "Failed to delete w_correctness_map.dat" );
    }
    if( nftw( "persitency_test.dat", rm, OPEN_MAX, FTW_DEPTH )) {
//        perror( "Failed to delete persitency_test.dat" );
    }
}

int main(int argc, const char * argv[]) {

    srand (time(NULL));
    x = rand();
    y = rand();
    z = rand();
    w = rand();
    
    std::cout << "Pre-cleaning..." << std::flush;
    
    clean();
    
    std::cout << " done" << std::endl;
    

//    weak_correctness_check(1 << 15);
    persistency_check(1 << 15);
    
    std::cout << "Post-cleaning..." << std::flush;
    
    clean();
    
    std::cout << " done" << std::endl;
    
    return 0;
}
