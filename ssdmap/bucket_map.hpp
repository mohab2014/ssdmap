//
//  bucket_map.hpp
//  ssdmap
//
//  Created by Raphael Bost on 20/03/2016.
//  Copyright © 2016 Raphael Bost. All rights reserved.
//

#pragma once

#include "bucket_array.hpp"
#include "mmap_util.h"

#include <utility>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <cassert>

uint8_t log2llu(size_t x)
{
    uint8_t c = 8*sizeof(size_t) - 1;
    size_t mask = (1 << (c));
    
    for ( ; c > 0; c--, mask >>= 1) {
        if ((x & mask) != 0) {
            return c;
        }
    }
    
    return 0;
}

constexpr float kBucketMapResizeThresholdLoad = 0.85;
constexpr size_t kBucketMapResizeThresholdOverflow = 1e5;
constexpr size_t kBucketMapResizeStepIterations = 4;

constexpr size_t kPageSize = 4096;

template <class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>>
class bucket_map {
public:
    typedef Key                                                        key_type;
    typedef T                                                          mapped_type;
    typedef Hash                                                       hasher;
    typedef Pred                                                       key_equal;
    typedef std::pair<const key_type, mapped_type>                     value_type;
    typedef value_type&                                                reference;
    typedef const value_type&                                          const_reference;

    typedef size_t          size_type;
    typedef ptrdiff_t       difference_type;
    
    
    typedef std::pair<key_type, mapped_type>                     bucket_value_type;
    typedef bucket_array<bucket_value_type> bucket_array_type;

    
private:
    std::unordered_map<key_type, mapped_type, hasher, key_equal> overflow_map_;
    
    std::vector<std::pair<bucket_array_type, mmap_st> > bucket_arrays_;
    
    
    uint8_t mask_size_;
    uint8_t original_mask_size_;
    
    // where to store the files
    std::string base_filename_;
    
    // to compute load
    size_t e_count_;
    size_t bucket_space_;
    
    // resize management
    bool is_resizing_;
    size_t resize_counter_;
    
public:
    
    bucket_map(const size_type setup_size, const hasher& hf = hasher(), const key_equal& eql = key_equal())
    : e_count_(0), bucket_arrays_(), is_resizing_(false)
    {

        
        size_t b_size = bucket_array_type::optimal_bucket_size(kPageSize);
        float target_load = 0.75;
        
        original_mask_size_ = ceilf(log2f(setup_size/(target_load*b_size)));
        
//        mask_size_ = original_mask_size_+1;
        mask_size_ = original_mask_size_;
        
        size_t N = 1 << mask_size_;

        size_t length = N  * kPageSize;
        
        base_filename_ = "bucket_map.bin";
        
        std::ostringstream string_stream;
        string_stream << base_filename_ << "." << std::dec << 0;
        
        mmap_st mmap = create_mmap(string_stream.str().data(),length);
        
        bucket_arrays_.push_back(std::make_pair(bucket_array_type(mmap.mmap_addr, N, kPageSize), mmap));
        bucket_space_ = bucket_arrays_[0].first.bucket_size() * bucket_arrays_[0].first.bucket_count();

//        N = 1 << mask_size_;
//        length = N  * kPageSize;
//        
//        mmap = create_mmap("bucket_map_1.bin",length);
//        
//        bucket_arrays_.push_back(std::make_pair(bucket_array_type(mmap.mmap_addr, N, kPageSize), mmap));
//        bucket_space_ += bucket_arrays_[1].first.bucket_size() * bucket_arrays_[1].first.bucket_count();

    }
    
    ~bucket_map()
    {
        for (auto it = bucket_arrays_.rbegin(); it != bucket_arrays_.rend(); ++it) {
            close_mmap(it->second);
        }
    }
    
    inline std::pair<uint8_t, size_t> bucket_coordinates(size_t h) const
    {
        if (is_resizing_) {
            // we must be careful here
            // the coordinates depend on the value of resize_counter_
            // if h & (1 << mask_size_)-1 is less than resize_counter_, it means that the
            // bucket, before rebuild, was splitted
            // otherwise, do as before
            
            if ((h & ((1 << mask_size_)-1)) < resize_counter_) {
                // if the mask_size_-th bit is 0, do as before,
                // otherwise, we know that the bucket is in the last array
                
                if ((h & ((1 << mask_size_))) != 0) {
                    return std::make_pair(mask_size_+1, (h & ((1 << mask_size_)-1)));
                }
            }
        }
        h &= (1 << mask_size_)-1;

        uint8_t c = mask_size_-1;
        size_t mask = (1 << c);
        
        for (; c >= original_mask_size_; c--, mask >>= 1) { // mask = 2^c
            if ((mask & h) != 0) {
                return std::make_pair(c - original_mask_size_+1, h ^ mask);
            }
        }
        
        return std::make_pair(0, h);
    }
    
    bool get(key_type key, mapped_type& v, const hasher& hf = hasher(), const key_equal& eql = key_equal())
    {
        // first, look if it is not in the overflow map
        auto ob_it = overflow_map_.find(key);
        if (ob_it != overflow_map_.end()) {
            v = ob_it->second;
            return true;
        }
        
        // otherwise, get the bucket index
        size_t h = hf(key);
        
        // get the appropriate coordinates
        std::pair<uint8_t, size_t> coords = bucket_coordinates(h);
        
        // get the bucket and prefetch it
        auto bucket = bucket_arrays_[coords.first].first.bucket(coords.second);
        bucket.prefetch();
        
        // scan throught the bucket to find the element
        for (auto it = bucket.begin(); it != bucket.end(); ++it) {
            if(eql(it->first, key))
            {
                v = it->second;
                return true;
            }
        }
        
        return false;
    }
    
    void add(key_type key, const mapped_type& v, const hasher& hf = hasher())
    {
        value_type value(key,v);
        // get the bucket index
        size_t h = hf(key);
        h &= (1 << mask_size_)-1;

        // get the appropriate coordinates
        std::pair<uint8_t, size_t> coords = bucket_coordinates(h);

        // try to append the value to the bucket
        auto bucket = bucket_arrays_[coords.first].first.bucket(coords.second);
        bool success = bucket.append(value);
        
        if (!success) {
            // add to the overflow bucket
            overflow_map_.insert(value);
            
//            double load = ((double)e_count_)/(bucket_space_);
            double over_prop = ((double)overflow_map_.size())/(e_count_);
            
            std::cout << "Full bucket. " << e_count_ << " elements (load factor " << load() << ")\n size of overflow bucket: " << overflow_map_.size() << ", overflow proportion: " << over_prop << std::endl;
        }
        
        e_count_++;
        
        if (is_resizing_) {
            online_resize();
        }else{
            if (should_resize()) {
                start_resize();
            }
        }

    }
    
    inline float load() const
    {
        return ((float)e_count_)/(bucket_space_);
    }
    
    inline bool should_resize() const
    {
        // return yes if the map should be resized to reduce the load and/or the size of the overflow bucket
        if(e_count_ > kBucketMapResizeThresholdLoad*bucket_space_ && overflow_map_.size() >= kBucketMapResizeThresholdOverflow)
        {
            return true;
        }
        
        if(overflow_map_.size() >= 10*kBucketMapResizeThresholdOverflow)
        {
            return true;
        }
        
        
        return false;
    }
    
    void start_resize()
    {
        assert(!is_resizing_);
        // create a new bucket_array of double the size of the previous one
        
        size_t ba_count = bucket_arrays_.size();
        
        size_t N = 1 << (mask_size_ + 1);
        
        size_t length = N  * kPageSize;
        
        std::ostringstream string_stream;
        string_stream << base_filename_ << "." << std::dec << ba_count;
        
        mmap_st mmap = create_mmap(string_stream.str().data(),length);
        bucket_arrays_.push_back(std::make_pair(bucket_array_type(mmap.mmap_addr, N, kPageSize), mmap));
        bucket_space_ += bucket_arrays_[ba_count].first.bucket_size() * bucket_arrays_[ba_count].first.bucket_count();

        resize_counter_ = 0;
        is_resizing_ = true;
    }
    
    void finalize_resize()
    {
        mask_size_++;
        resize_counter_ = 0;
        is_resizing_ = false;
    }
    
    void online_resize()
    {
        for (size_t i = 0; i < kBucketMapResizeStepIterations && is_resizing_; i++) {
            resize_step();
        }
    }
    void resize_step()
    {
        // read a bucket and rewrite some of its content somewhere else
        
//        // get the one but last bucket array
//        size_t ba_count = bucket_arrays_.size();
//        bucket_array_type ba = bucket_arrays_[ba_count-2];
        
        // get the bucket pointed by resize_counter_
        std::pair<uint8_t, size_t> coords = bucket_coordinates(resize_counter_);
        auto b = bucket_arrays_[coords.first].first.bucket(coords.second);
        

        size_t mask = (1 << mask_size_);
        auto new_bucket = bucket_arrays_.back().first.bucket(resize_counter_);
        
        size_t c_old = 0;
        auto it_old = b.begin();
        
        for (auto it = b.begin(); it != b.end(); ++it) {
            if (((it->first) & mask) == 0) { // high order bit of the key is 0
                // keep it here
                *it_old = *it;
                ++it_old;
                c_old++;
            }else{
                // append it to the other bucket
                bool success = new_bucket.append(*it);
                
                if (!success) {
                    printf("PROBLEM!\n");
                }
            }
        }
        // set the new size of the old bucket
        b.set_size(c_old);
        
        
        // check if we are done
        if (resize_counter_ == ((mask<<1) -1)) {
            finalize_resize();
        }else{
            resize_counter_ ++;
        }
    }
};