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
        size_t page_size = 4096;

        
        size_t b_size = bucket_array_type::optimal_bucket_size(page_size);
        float target_load = 0.75;
        
        original_mask_size_ = ceilf(log2f(setup_size/(target_load*b_size)));
        
        mask_size_ = original_mask_size_+1;
        
        size_t N = 1 << original_mask_size_;

        size_t length = N  * page_size;

        mmap_st mmap = create_mmap("bucket_map_0.bin",length);
        
        bucket_arrays_.push_back(std::make_pair(bucket_array_type(mmap.mmap_addr, N, page_size), mmap));
        bucket_space_ = bucket_arrays_[0].first.bucket_size() * bucket_arrays_[0].first.bucket_count();

        N = 1 << mask_size_;
        length = N  * page_size;
        
        mmap = create_mmap("bucket_map_1.bin",length);
        
        bucket_arrays_.push_back(std::make_pair(bucket_array_type(mmap.mmap_addr, N, page_size), mmap));
        bucket_space_ += bucket_arrays_[1].first.bucket_size() * bucket_arrays_[1].first.bucket_count();

    }
    
    ~bucket_map()
    {
        for (auto it = bucket_arrays_.rbegin(); it != bucket_arrays_.rend(); ++it) {
            close_mmap(it->second);
        }
    }
    
    inline std::pair<uint8_t, size_t> bucket_coordinates(size_t h) const
    {
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
        h &= (1 << mask_size_)-1;
        
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
        
        }else{
            if (should_resize()) {
                resize_counter_ = 0;
                is_resizing_ = true;
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
    
    void resize_step()
    {
        // read a bucket and rewrite some of its content somewhere else
    }
};