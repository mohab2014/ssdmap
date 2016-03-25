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
#include <string>
#include <sstream>

#include <cmath>
#include <cassert>
#include <sys/stat.h>

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

constexpr size_t kPageSize = 512;

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
    
    
    typedef std::pair<key_type, mapped_type>                        bucket_value_type;
    typedef bucket_array<bucket_value_type>                         bucket_array_type;
    typedef typename bucket_array_type::bucket_type                 bucket_type;
    
    typedef std::unordered_map<size_t, value_type>                  overflow_submap_type;
    typedef std::unordered_map<size_t, overflow_submap_type>           overflow_map_type;
private:
//    std::unordered_map<key_type, mapped_type, hasher, key_equal> overflow_map_;
    overflow_map_type overflow_map_;
    
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
    
    typedef struct
    {
        uint8_t original_mask_size;
        uint8_t bucket_arrays_count;
        bool is_resizing;
        size_t resize_counter;
        size_t e_count;
        size_t overflow_count;
    } metadata_type;

public:
    
    bucket_map(const std::string &path, const size_type setup_size, const hasher& hf = hasher(), const key_equal& eql = key_equal())
    : base_filename_(path), e_count_(0), overflow_count_(0), overflow_map_(), bucket_arrays_(), is_resizing_(false)
    {

        // check is there already is a directory at path
        struct stat buffer;
        if(stat (base_filename_.data(), &buffer) == 0) // there is something at path
        {
            if (!S_ISDIR(buffer.st_mode)) {
                throw std::runtime_error("bucket_map constructor: Invalid path");
            }
            
            init_from_file();
        }else{
            // create a new directory
            if (mkdir(base_filename_.data(),(mode_t)0700) != 0) {
                throw std::runtime_error("bucket_map constructor: Unable to create the data directory");
            }
            
            
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
            
    //        base_filename_ = "bucket_map.bin";
            
            std::ostringstream string_stream;
            string_stream << base_filename_ << "/data." << std::dec << 0;
            
            mmap_st mmap = create_mmap(string_stream.str().data(),length);
            
            bucket_arrays_.push_back(std::make_pair(bucket_array_type(mmap.mmap_addr, N, kPageSize), mmap));
            bucket_space_ = bucket_arrays_[0].first.bucket_size() * bucket_arrays_[0].first.bucket_count();
        }
    }
    
    ~bucket_map()
    {
        flush();
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
    
    inline size_t get_overflow_bucket_index(size_t h) const
    {
        size_t index = (h&((1 << mask_size_)-1));
        
        if (is_resizing_) {
            // we must be careful here
            // the coordinates depend on the value of resize_counter_
            // if index is less than resize_counter_, it means that the
            // bucket, before rebuild, was splitted
            // otherwise, do as before
            if (index < resize_counter_) {
                // if the mask_size_-th bit is 0, do as before,
                // otherwise, recompute the index accordingly
                
                if ((h & ((1 << mask_size_))) != 0) {
                    return (h&((1 << (mask_size_+1))-1));
                }
            }
        }

        return index;
    }
    
    inline bucket_type get_bucket(uint8_t ba_index, size_t b_pos)
    {
        if (ba_index >= bucket_arrays_.size()) {
            throw std::out_of_range("bucket_map::get_bucket");
        }
        
        return bucket_arrays_[ba_index].first.bucket(b_pos);
    }

    inline bucket_type get_bucket(const std::pair<uint8_t, size_t> &p)
    {
        return get_bucket(p.first, p.second);
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
        auto bucket = get_bucket(coords);
//        std::cout << "bucket size: " << bucket.size() << std::endl;
//        bucket.prefetch();
        
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
        
        if (v == 1130534054905073795) {
//            std::cout << "Insert our element\n";
        }

        // get the bucket index
        size_t h = hf(key);

        // get the appropriate coordinates
        std::pair<uint8_t, size_t> coords = bucket_coordinates(h);

        // try to append the value to the bucket
        auto bucket = get_bucket(coords);
        
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
//#warning Should take into account the resize counter
        
        if (hkey == 1130534054905073795) {
//            std::cout << "Overflow bucket search key: " << (hkey&((1 << mask_size_)-1)) << std::endl;
        }
        
        size_t index = get_overflow_bucket_index(hkey);
                
        auto const it = overflow_map_.find(index);
        
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
            overflow_submap_type m;

            m.insert(std::make_pair(hkey, v));
            
            overflow_map_.insert(std::make_pair( bucket_index , m)); // move ????

        }
        
        overflow_count_++;
    }

    void append_overflow_bucket(size_t hkey, const value_type& v)
    {        
        size_t index = get_overflow_bucket_index(hkey);
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
        string_stream << base_filename_ << "/data." << std::dec << ba_count;

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
        auto b = get_bucket(coords);

        size_t mask = (1 << mask_size_);
        auto new_bucket = bucket_arrays_.back().first.bucket(resize_counter_);
        new_bucket.set_size(0);
        
        size_t c_old = 0;
        auto it_old = b.begin();
        
        for (auto it = b.begin(); it != b.end(); ++it) {
            if (it->first == 1130534054905073795) {
//                std::cout << "Treat our element\n";
            }
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
            overflow_submap_type current_of_bucket(std::move(bucket_it->second));
            
            // erase the old value
            overflow_map_.erase(resize_counter_);
            
            overflow_count_ -= current_of_bucket.size();
            
            // enumerate the bucket's content and try to append the values to the buckets
            for (auto &elt: current_of_bucket) {
                if (elt.first == 1130534054905073795) {
//                    std::cout << "Treat our element\n";
                }

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
    
    void flush() const
    {
        // flush the data to the disk
        
        // start by syncing the bucket arrays
        // do it asynchronously for now
        for (auto it = bucket_arrays_.rbegin(); it != bucket_arrays_.rend(); ++it) {
            flush_mmap(it->second,ASYNCFLAG);
        }
        
        // create a new memory map for the overflow bucket
        typedef std::pair<size_t, std::pair<size_t,value_type>> pair_type;
        
        std::string overflow_temp_path = base_filename_ + "/overflow.tmp";
        
        if(overflow_count_ > 0){
            mmap_st over_mmap = create_mmap(overflow_temp_path.data(), overflow_count_*sizeof(pair_type));
            

            pair_type* elt_ptr = (pair_type*) over_mmap.mmap_addr;
            size_t i = 0;
            
            for (auto &sub_map : overflow_map_) {
                // submap is a map
                
                for (auto &x : sub_map.second) {
                    pair_type tmp = std::make_pair(sub_map.first, x);
                    memcpy(elt_ptr+i, &tmp, sizeof(pair_type));
                    i++;
                }
            }
            
            // flush it to the disk
            close_mmap(over_mmap, 1);
        }
        // erase the old overflow file and replace it by the temp file
        std::string overflow_path = base_filename_ + "/overflow.bin";
        remove(overflow_path.data());
        
        if(overflow_count_ > 0){
            if (rename(overflow_temp_path.data(), overflow_path.data()) != 0) {
                throw std::runtime_error("Unable to rename overflow.tmp to overflow.bin");
            }
        }
        
        std::string meta_path = base_filename_ + "/meta.bin";
        mmap_st meta_mmap = create_mmap(meta_path.data(), sizeof(metadata_type));
        metadata_type *meta_ptr = (metadata_type *)meta_mmap.mmap_addr;

        meta_ptr->original_mask_size = original_mask_size_;
        meta_ptr->is_resizing = is_resizing_;
        meta_ptr->resize_counter = resize_counter_;
        meta_ptr->overflow_count = overflow_count_;
        meta_ptr->e_count = e_count_;
        meta_ptr->bucket_arrays_count = bucket_arrays_.size();
                
        close_mmap(meta_mmap,1);

        for (auto it = bucket_arrays_.rbegin(); it != bucket_arrays_.rend(); ++it) {
            close_mmap(it->second,1);
        }
    }
private:
    void init_from_file()
    {
        // start by reading the meta data
        struct stat buffer;
        
        std::string meta_path = base_filename_ + "/meta.bin";

        if (stat (meta_path.data(), &buffer) != 0) { // the meta data file is not there
            throw std::runtime_error("bucket_map constructor: metadata file does not exist");
        }

        mmap_st meta_mmap = create_mmap(meta_path.data(), sizeof(metadata_type));
        
        metadata_type *meta_ptr = (metadata_type *)meta_mmap.mmap_addr;
        
        original_mask_size_     = meta_ptr->original_mask_size;
        is_resizing_            = meta_ptr->is_resizing;
        resize_counter_         = meta_ptr->resize_counter;
        e_count_                = meta_ptr->e_count;
        
        mask_size_ = original_mask_size_ + meta_ptr->bucket_arrays_count -1;
        
        size_t N = 1 << (original_mask_size_);
        
        bucket_space_ = 0;

        for (uint8_t i = 0; i < meta_ptr->bucket_arrays_count; i++) {
            size_t length = N  * kPageSize;
            
            std::string fn = base_filename_ + "/data." + std::to_string(i);
            
            if (stat (fn.data(), &buffer) != 0) { // the file is not there
                throw std::runtime_error("bucket_map constructor: " + std::to_string(i) + "-th data file does not exist.");
            }

            mmap_st mmap = create_mmap(fn.data(),length);
            
            bucket_arrays_.push_back(std::make_pair(bucket_array_type(mmap.mmap_addr, N, kPageSize), mmap));
            
            if (is_resizing_ == false || i < meta_ptr->bucket_arrays_count-1) {
                bucket_space_ += bucket_arrays_[i].first.bucket_size() * bucket_arrays_[i].first.bucket_count();
            }else{
                bucket_space_ += resize_counter_*bucket_arrays_[i].first.bucket_size() * bucket_arrays_[i].first.bucket_count();
            }

            if (i > 0) {
                N <<= 1;
            }
        }

        // read the overflow bucket
        std::string overflow_path = base_filename_ + "/overflow.bin";
        
        if (stat (overflow_path.data(), &buffer) != 0) { // the overflow file is not there
            throw std::runtime_error("bucket_map constructor: Overflow file does not exist.");
        }

        mmap_st over_mmap = create_mmap(overflow_path.data(), (meta_ptr->overflow_count)*sizeof(typename overflow_map_type::value_type));
        
        typedef std::pair<size_t, std::pair<size_t,value_type>> pair_type;
        pair_type* elt_ptr = (pair_type*) over_mmap.mmap_addr;
        
        for (size_t i = 0; i < meta_ptr->overflow_count; i++) {
            append_overflow_bucket(elt_ptr[i].first, elt_ptr[i].second.first, elt_ptr[i].second.second);
        }
        
        overflow_count_         = meta_ptr->overflow_count;

        // close the overflow mmap, no need to flush (we only read it)
        close_mmap(over_mmap,0);
        
        // close the metadata map, no need to flush (we only read it)
        close_mmap(meta_mmap,0);
        
        std::cout << "Read bucket space: " << bucket_space_ << std::endl;

    }
    
};