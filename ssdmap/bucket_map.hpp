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
    mmap_st mmap_;
    bucket_array_type *b_array_;
    
    uint8_t mask_size_;
    
public:
    bucket_map(const hasher& hf = hasher(), const key_equal& eql = key_equal())
    {
        
        mask_size_ = 16; // first 16 bits
        size_t N = 1 << mask_size_;
        size_t page_size = 4096;

        size_t length = N  * page_size;

        mmap_ = create_mmap("bucket_map.bin",length);

        b_array_ = new bucket_array_type(mmap_.mmap_addr, N, page_size);
    }
    
    ~bucket_map()
    {
        delete b_array_;
        close_mmap(mmap_);
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
        
        // get the bucket and prefetch it
        auto bucket = b_array_->bucket(h);
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

        // try to append the value to the bucket
        auto bucket = b_array_->bucket(h);
        bool success = bucket.append(value);
        
        if (!success) {
            // add to the overflow bucket
            overflow_map_.insert(value);
        }
    }
};