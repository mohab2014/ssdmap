//
//  main.cpp
//  ssdmap
//
//  Created by Raphael Bost on 17/03/2016.
//  Copyright © 2016 Raphael Bost. All rights reserved.
//

#include <iostream>
#include <fstream>

#include <ftw.h>
#include <chrono>
#include <vector>

#include "mmap_util.h"
#include "bucket_array.hpp"
#include "bucket_map.hpp"

using namespace ssdmap;

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

void benchmark(const std::string &filename, const std::string &write_bench_file, const std::string &read_bench_file, size_t initial_size, size_t test_size)
{
    std::cout << "Start benchmark\n";
    std::cout << "Initial size: " << initial_size;
    std::cout << ", test size: " << test_size << std::endl;
    
    bucket_map<uint64_t,uint64_t> bm(filename,initial_size); // 700 => 4 buckets
//    std::map<uint64_t, uint64_t> ref_map;
    
    std::vector<size_t> timings(test_size);
    
    std::cout << "Fill the map ..." << std::flush;


    for (size_t i = 0; i < test_size; i++) {
        uint64_t k = xorshift128();

        auto begin = std::chrono::high_resolution_clock::now();

        bm.add(k, k);

        auto end = std::chrono::high_resolution_clock::now();
        
        timings[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count();
        

//        ref_map[k] = k;
    }
    
    std::cout << " done\n";

    std::ofstream write_timings_file;
    write_timings_file.open (write_bench_file);

    for (auto it = timings.begin(); it != timings.end(); ++it) {
//        timings_file << ((double)(*it))/(1e6) << std::endl;
        write_timings_file << (*it) << std::endl;
    }

    write_timings_file.close();

    uint64_t v;
    for (size_t i = 0; i < test_size; i++) {

        auto begin = std::chrono::high_resolution_clock::now();
        bm.get(i, v);
        auto end = std::chrono::high_resolution_clock::now();
        
        timings[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count();
    }
    
    std::ofstream read_timings_file;
    read_timings_file.open (read_bench_file);
    
    for (auto it = timings.begin(); it != timings.end(); ++it) {
        //        timings_file << ((double)(*it))/(1e6) << std::endl;
        read_timings_file << (*it) << std::endl;
    }
    read_timings_file.close();

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
    status = rm_func( path );
    
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
    
    std::cout << "Pre-cleaning ..." << std::flush;
    
    clean({"bench.dat","bench"});
    
    std::cout << " done\n\n" << std::endl;
    
    if (mkdir("bench",(mode_t)0700) != 0) {
        throw std::runtime_error("Unable to create the bench directory");
    }


    benchmark("bench.dat", "bench/write_bench.out", "bench/read_bench.out", 1<<15, 1<<20);
    
    std::cout << "Post-cleaning ..." << std::flush;
    
    clean({"bench.dat"});
    
    std::cout << " done" << std::endl;
    
    return 0;
}
