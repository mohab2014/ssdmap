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

void weak_correctness_check(size_t initial_size, size_t test_size, bool stop_fail = false)
{
    std::cout << "Weak correctness check:\n";
    std::cout << "Initial size: " << initial_size;
    std::cout << ", test size: " << test_size << std::endl;
    
    bucket_map<uint64_t,uint64_t> bm("w_correctness_map.dat",initial_size); // 700 => 4 buckets
    std::map<uint64_t, uint64_t> ref_map;
    
    std::cout << "Fill the map ..." << std::flush;

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
    
    size_t count = 0;
    size_t fail_count = 0;
    for(auto &x : ref_map)
    {
        uint64_t v;
        bool s = bm.get(x.first, v);
        
        if ((!s || v != x.second)) {
            if (stop_fail) {
                std::cout << "Weak correctness check failed\n";
                return;
            }else{
                fail_count++;
            }
        }
        
        count++;
    }
    
    if (fail_count > 0) {
        std::cout << "Weak correctness check failed, " << fail_count << "errors\n";
    }else{
        std::cout << "Weak correctness check passed\n\n";
    }
}

void persistency_check(size_t test_size, bool stop_fail = false)
{
    std::cout << "Persistency check:\n";
    std::cout << "Test size: " << test_size << std::endl;

    bucket_map<uint64_t,uint64_t> *bm = new bucket_map<uint64_t,uint64_t>("persistency_test.dat",700); // 700 => 4 buckets
    std::map<uint64_t, uint64_t> ref_map;
    
    std::cout << "Fill the map ..." << std::flush;
    for (size_t i = 0; i < test_size; i++) {
        uint64_t k = xorshift128();
//                uint64_t k = i;
        bm->add(k, k);
        ref_map[k] = k;
 
        
    }
    
    std::cout << " done" << std::endl;
    
    std::cout << "Flush to disk ..." << std::flush;
    
    delete bm;

    std::cout << " done" << std::endl;

    std::cout << "Read from disk ..." << std::flush;
    
    bm = new bucket_map<uint64_t,uint64_t>("persistency_test.dat",700); // 700 => 4 buckets

    std::cout << " done" << std::endl;

    std::cout << "Test consistency ..." << std::endl;

    size_t count = 0;
    size_t fail_count = 0;
    for(auto &x : ref_map)
    {
        uint64_t v;
        bool s = bm->get(x.first, v);
        
        if ((!s || v != x.second)) {
            if (stop_fail) {
                std::cout << "Weak correctness check failed\n";
                return;
            }else{
                fail_count++;
            }
        }
        
        count++;
    }
    
    if (fail_count > 0) {
        std::cout << "Persistency check failed, " << fail_count << "errors\n";
    }else{
        std::cout << "Persistency check passed\n\n";
    }
    
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
    rm_func( path );
    
//    if( status = rm_func( path ), status != 0 )
//        perror( path );
//    else
//        puts( path );

    return status;
}

void clean()
{
    if( nftw( "w_correctness_map.dat", rm, OPEN_MAX, FTW_DEPTH )) {
//        perror( "Failed to delete w_correctness_map.dat" );
    }
    if( nftw( "persistency_test.dat", rm, OPEN_MAX, FTW_DEPTH )) {
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
    
    std::cout << " done\n\n" << std::endl;
    

    weak_correctness_check(700, 1<<15);
//    weak_correctness_check(1 << 20, 1<<30);
    
    persistency_check(1 << 20);
    
    std::cout << "Post-cleaning..." << std::flush;
    
    clean();
    
    std::cout << " done" << std::endl;
    
    return 0;
}
