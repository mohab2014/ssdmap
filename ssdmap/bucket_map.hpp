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
#include <map>
#include <vector>
#include <list>

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
constexpr size_t kBucketMapResizeMaxOverflowSize = 1e5; // no more than 1e5 elements in the overflow bucket
constexpr float kBucketMapResizeMaxOverflowRatio = 0.1; // no more than 10% of elements in the overflow bucket
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
//    std::unordered_map<key_type, mapped_type, hasher, key_equal> overflow_map_;
    std::map<size_t, std::map<size_t,value_type>> overflow_map_;
    
    std::vector<std::pair<bucket_array_type, mmap_st> > bucket_arrays_;
    
    
    uint8_t mask_size_;
    uint8_t original_mask_size_;
    
    // where to store the files
    std::string base_filename_;
    
    // to compute load
    size_t e_count_;
    size_t bucket_space_;
    size_t overflow_count_;
    
    // resize management
    bool is_resizing_;
    size_t resize_counter_;
    
public:
    
    bucket_map(const size_type setup_size, const hasher& hf = hasher(), const key_equal& eql = key_equal())
    : e_count_(0), overflow_count_(0), bucket_arrays_(), is_resizing_(false)
    {

        
        size_t b_size = bucket_array_type::optimal_bucket_size(kPageSize);
        float target_load = 0.75;
        size_t N;
        
        if(target_load*b_size >= setup_size)
        {
            original_mask_size_ = 1;
        }else{
            float f =setup_size/(target_load*b_size);
            original_mask_size_ = ceilf(log2f(f));
        }
        
        mask_size_ = original_mask_size_;
        N = 1 << mask_size_;

        size_t length = N  * kPageSize;
        
        base_filename_ = "bucket_map.bin";
        
        std::ostringstream string_stream;
        string_stream << base_filename_ << "." << std::dec << 0;
        
        mmap_st mmap = create_mmap(string_stream.str().data(),length);
        
        bucket_arrays_.push_back(std::make_pair(bucket_array_type(mmap.mmap_addr, N, kPageSize), mmap));
        bucket_space_ = bucket_arrays_[0].first.bucket_size() * bucket_arrays_[0].first.bucket_count();

    }
    
    ~bucket_map()
    {
        for (auto it = bucket_arrays_.rbegin(); it != bucket_arrays_.rend(); ++it) {
            close_mmap(it->second);
        }
    }
    
    inline size_t size() const
    {
        return e_count_;
    }
    
    inline float load() const
    {
        return ((float)e_count_)/(bucket_space_);
    }
    
    inline size_t overflow_size() const
    {
        return overflow_count_;
    }
    
    inline float overflow_ratio() const
    {
        return ((float)overflow_count_)/(e_count_);
    }
    
    inline std::pair<uint8_t, size_t> bucket_coordinates(size_t h) const
    {
        if (is_resizing_) {
            // we must be careful here
            // the coordinates depend on the value of resize_counter_
            // if h & (1 << mask_size_)-1 is less than resize_counter_, it means that the
            // bucket, before rebuild, was splitted
            // otherwise, do as before
            size_t masked_h = (h & ((1 << mask_size_)-1));
            if (masked_h < resize_counter_) {
                // if the mask_size_-th bit is 0, do as before,
                // otherwise, we know that the bucket is in the last array
                
                if ((h & ((1 << mask_size_))) != 0) {
                    return std::make_pair(mask_size_- original_mask_size_+1, masked_h);
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
        size_t h = hf(key);

        // first, look if it is not in the overflow map

        if (get_overflow_bucket(h, v)) {
            return true;
        }
        
        // otherwise, get the bucket index
        
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

        // get the appropriate coordinates
        std::pair<uint8_t, size_t> coords = bucket_coordinates(h);

        assert(coords.first < bucket_arrays_.size());
        // try to append the value to the bucket
        auto bucket = bucket_arrays_[coords.first].first.bucket(coords.second);
        
        bool success = bucket.append(value);
        
        if (!success) {
            // add to the overflow bucket
            append_overflow_bucket(h, value);
            
            
//            std::cout << "Full bucket. " << size() << " elements (load factor " << load() << ")\n size of overflow bucket: " << overflow_size() << ", overflow proportion: " << overflow_ratio() << "\n" << std::endl;
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
    
    bool get_overflow_bucket(size_t hkey, mapped_type& v) const
    {
        
        auto const it = overflow_map_.find(hkey&((1 << mask_size_)-1));
        
        if (it != overflow_map_.end()) {
            auto const map_it = it->second.find(hkey);
            
            if (map_it != it->second.end()) {
                v = map_it->second.second;
                return true;
            }
        }
        
        return false;
    }
    
    void append_overflow_bucket(size_t bucket_index, size_t hkey, const value_type& v)
    {
        auto const it = overflow_map_.find(bucket_index);
        
        if (it != overflow_map_.end()) {
            it->second.insert(std::make_pair(hkey, v));
        }else{
            std::map<size_t, value_type> m;

            m.insert(std::make_pair(hkey, v));
            
            overflow_map_.insert(std::make_pair( hkey&((1 << mask_size_)-1) , m)); // move ????

        }
        
        overflow_count_++;
    }

    void append_overflow_bucket(size_t hkey, const value_type& v)
    {
        size_t index = hkey&((1 << mask_size_)-1);
        append_overflow_bucket(index, hkey, v);
    }
    
    inline bool should_resize() const
    {
        // return yes if the map should be resized to reduce the load and/or the size of the overflow bucket
        if(e_count_ > kBucketMapResizeThresholdLoad*bucket_space_ )
        {
            if (overflow_count_ >= kBucketMapResizeMaxOverflowSize) {
                return true;
            }else if (overflow_count_ >= kBucketMapResizeMaxOverflowRatio * e_count_){
                return true;
            }
        }
        
        if(overflow_count_ >= 10*kBucketMapResizeMaxOverflowSize)
        {
            return true;
        }
        
        
        return false;
    }
    
    void start_resize()
    {
        assert(!is_resizing_);
        
//        std::cout << "Start resizing!" << std::endl;
        
        // create a new bucket_array of double the size of the previous one
        
        size_t ba_count = bucket_arrays_.size();
        
        size_t N = 1 << (mask_size_);
        
        size_t length = N  * kPageSize;
        
        std::ostringstream string_stream;
        string_stream << base_filename_ << "." << std::dec << ba_count;
        
        mmap_st mmap = create_mmap(string_stream.str().data(),length);
        bucket_arrays_.push_back(std::make_pair(bucket_array_type(mmap.mmap_addr, N, kPageSize), mmap));

        resize_counter_ = 0;
        is_resizing_ = true;
    }
    
    void finalize_resize()
    {
        mask_size_++;
        resize_counter_ = 0;
        is_resizing_ = false;
        
//        std::cout << "Done resizing!" << std::endl;

    }
    
    void online_resize()
    {
        for (size_t i = 0; i < kBucketMapResizeStepIterations && is_resizing_; i++) {
            resize_step();
        }
    }
    
    void full_resize()
    {
        if(!is_resizing_)
        {
            start_resize();
        }
        
        for (; is_resizing_; ) {
            resize_step();
        }
    }
    
    void resize_step()
    {
        // read a bucket and rewrite some of its content somewhere else
        
        // get the bucket pointed by resize_counter_
        std::pair<uint8_t, size_t> coords = bucket_coordinates(resize_counter_);
        auto b = bucket_arrays_[coords.first].first.bucket(coords.second);
        

        size_t mask = (1 << mask_size_);
        auto new_bucket = bucket_arrays_.back().first.bucket(resize_counter_);
        new_bucket.set_size(0);
        
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
                    // append the pair to the overflow bucket
                    size_t h = it->first;

                    append_overflow_bucket(h&((1 << (mask_size_+1))-1), h, *it);
                }
            }
        }
        // set the new size of the old bucket
        b.set_size(c_old);
        
        
        // now, try to put as many elements from the overflow bucket as possible
        auto const bucket_it = overflow_map_.find(resize_counter_);
        
        bool success;
        
        if (bucket_it != overflow_map_.end()) {
            // initialize the current bucket
            std::map<size_t,value_type> current_of_bucket(std::move(bucket_it->second));
            
            // erase the old value
            overflow_map_.erase(resize_counter_);
            
            overflow_count_ -= current_of_bucket.size();
            
            // enumerate the bucket's content and try to append the values to the buckets
            for (auto &elt: current_of_bucket) {
                if (((elt.first) & mask) == 0) { // high order bit of the key is 0
                    success = b.append(elt.second);
                    if (!success) { // add the overflow bucket
                        append_overflow_bucket(resize_counter_, elt.first, elt.second);
                    }
               }else{
                    success = new_bucket.append(elt.second);
                   if (!success) { // add the overflow bucket
                       append_overflow_bucket(mask ^ resize_counter_, elt.first, elt.second);
                   }
                }
                
            }

        }

        
        // check if we are done
        if (resize_counter_ == (mask -1)) {
            finalize_resize();
        }else{
            resize_counter_ ++;
        }
        
        bucket_space_ += bucket_arrays_.back().first.bucket_size();
    }
};