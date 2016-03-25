//
//  checks.cpp
//  ssdmap
//
//  Created by Raphael Bost on 23/03/2016.
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


static uint64_t x=123456789, y=362436069, z=521288629, w=314159;

uint64_t xorshift128(void) {
    uint64_t t = x;
    t ^= t << 11;
    t ^= t >> 8;
    x = y; y = z; z = w;
    w ^= w >> 19;
    w ^= t;
    return w;
}

void correctness_check(const std::string &filename, size_t initial_size, size_t test_size, bool systematic_test, bool stop_fail = false)
{
    if (systematic_test) {
        std::cout << "Systematic correctness check:\n";
    }else{
        std::cout << "Correctness check:\n";
    }
    std::cout << "Initial size: " << initial_size;
    std::cout << ", test size: " << test_size << std::endl;
    
    bucket_map<uint64_t,uint64_t> bm(filename,initial_size); // 700 => 4 buckets
    std::map<uint64_t, uint64_t> ref_map;
    
    std::cout << "Fill the map ..." << std::flush;
    
    size_t fail_count = 0;
    
    for (size_t i = 0; i < test_size; i++) {
        uint64_t k = xorshift128();
        
        bm.add(k, k);
        ref_map[k] = k;
        
        
        if(systematic_test){
            for(auto &x : ref_map)
            {
                uint64_t v;
                bool s = bm.get(x.first, v);
                
                if ((!s || v != x.second)) {
                    if (stop_fail) {
                        std::cout << "Correctness check failed\n";
                        return;
                    }else{
                        fail_count++;
                    }
                }
                
            }
        }
    }
    
    std::cout << " done\n";
   
//    const unsigned long long n = 1130534054905073795;
//    
//    uint64_t v;
//    bool s = bm.get(n, v);
//    
//    if ((!s || v != n)) {
//        if (stop_fail) {
//            std::cout << "Correctness check failed\n";
//            return;
//        }else{
//            fail_count++;
//        }
//    }

    size_t count = 0;
    
    if(!systematic_test){ // this test has already been run is systematic_test is set to true
        
        for(auto &x : ref_map)
        {
            uint64_t v;
            bool s = bm.get(x.first, v);
            
            if ((!s || v != x.second)) {
                if (stop_fail) {
                    std::cout << "Correctness check failed\n";
                    return;
                }else{
                    fail_count++;
                }
            }
            
            count++;
        }
    }
    if (fail_count > 0) {
        std::cout << "Correctness check failed, " << fail_count << "errors\n";
    }else{
        std::cout << "Correctness check passed\n\n";
    }
}

void persistency_check(const std::string &filename, size_t test_size, bool stop_fail = false)
{
    std::cout << "Persistency check:\n";
    std::cout << "Test size: " << test_size << std::endl;
    
    bucket_map<uint64_t,uint64_t> *bm = new bucket_map<uint64_t,uint64_t>(filename,700); // 700 => 4 buckets
    std::map<uint64_t, uint64_t> ref_map;
    
    std::cout << "Fill the map ..." << std::flush;
    for (size_t i = 0; i < test_size; i++) {
        uint64_t k = xorshift128();
        
        bm->add(k, k);
        ref_map[k] = k;
    }
    
    std::cout << " done" << std::endl;
    
    std::cout << "Flush to disk ..." << std::flush;
    
    delete bm;
    
    std::cout << " done" << std::endl;
    
    std::cout << "Read from disk ..." << std::flush;
    
    bm = new bucket_map<uint64_t,uint64_t>(filename,700); // 700 => 4 buckets
    
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
    
    return status;
}

void clean(const std::list<std::string> &file_list)
{
    for (auto &fn : file_list) {
        nftw( fn.data(), rm, OPEN_MAX, FTW_DEPTH );
    }
}

int main(int argc, const char * argv[]) {
    
    srand ((unsigned int)time(NULL));
    x = rand() + (((uint64_t)rand()) << 32);
    y = rand() + (((uint64_t)rand()) << 32);
    z = rand() + (((uint64_t)rand()) << 32);
    w = rand() + (((uint64_t)rand()) << 32);
    
    x = 4821604254758231733;
    y = 2889736185279303868;
    z = 8945159673490801361;
    w = 4491977415880625016;
    
    std::cout << "x = " << x << std::endl;
    std::cout << "y = " << y << std::endl;
    std::cout << "z = " << z << std::endl;
    std::cout << "w = " << w << std::endl;
    
    std::cout << "Pre-cleaning ..." << std::flush;
    
    clean({"correctness_map.dat", "systematic_correctness_map.dat", "persistency_test.dat"});
    
    std::cout << " done\n\n" << std::endl;
    
    
//    correctness_check("correctness_map.dat", 700, 1<<15, false, false);
    correctness_check("correctness_map.dat", 700, 1<<20, false, false);
    
//    correctness_check("systematic_correctness_map.dat", 700, 1<<14, true, true);
//    correctness_check("systematic_correctness_map.dat", 700, 4232, true, true);
    
//    persistency_check("persistency_test.dat", 1 << 20);
    
    std::cout << "Post-cleaning ..." << std::flush;
    
    clean({"correctness_map.dat", "systematic_correctness_map.dat", "persistency_test.dat"});
    
    std::cout << " done" << std::endl;
    
    return 0;
}