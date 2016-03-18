//
//  bucket_array.hpp
//  ssdmap
//
//  Created by Raphael Bost on 18/03/2016.
//  Copyright © 2016 Raphael Bost. All rights reserved.
//

#pragma once

#include <stdint.h>
#include <sys/mman.h>

#include <exception>

/*
 * Architecture of a bucket
 *
 
 						Page (sector) size
 <------------------------------------------------------------------------------------>

 |==========|==========|==========|==========|==========|==========|=======|==========|
 |          |          |          |          |          |          |       |          |
 |  Data 0  |  Data 1  |  Data 3  |   ...    |   ...    |  Data k  | Empty |  Counter |
 |          |          |          |          |          |          |       |          |
 |==========|==========|==========|==========|==========|==========|=======|==========|
 
 <---------->															   <---------->
  sizeof(T)															 		 sizeof(C)
 */

template <class T, size_t N, class C = uint16_t>
class bucket_array {
public:
    typedef T                                 value_type;
    typedef T&                                reference;
    typedef const T&                          const_reference;
    typedef T*                                pointer;
    typedef const T*                          const_pointer;
    typedef size_t                            size_type;


    typedef C                                 counter_type;
    typedef C&                                counter_ref;
    typedef const C&                          const_counter_ref;
    typedef C*                                counter_ptr;
    typedef const C*                          const_counter_ptr;


    inline bucket_array(void* ptr, const_counter_ref bucket_size, const size_t& page_size) :
    mem_(static_cast<unsigned char*>(ptr)), bucket_size_(bucket_size), page_size_(page_size)
    {
        // check that the page can contain bucket_size_ elements plus a counter
        if(bucket_size_*sizeof(value_type)+sizeof(counter_type) >  page_size_)
        {
            throw std::runtime_error("Invalid page size.");
        }
    };
    
    inline bucket_array(void* ptr, const size_t& page_size) :
    mem_(static_cast<unsigned char*>(ptr)), bucket_size_((page_size - sizeof(counter_type))/sizeof(value_type)), page_size_(page_size)
    {
        
        // check that the page can contain bucket_size_ elements plus a counter
        if(bucket_size_*sizeof(value_type)+sizeof(counter_type) >  page_size_)
        {
            throw std::runtime_error("Invalid page size.");
        }
    };
    
    counter_type bucket_size() const
    {
        return bucket_size_;
    }

    inline pointer get_bucket_pointer(size_type n)
    {
        if (n >= N) {
            throw std::out_of_range("bucket_array::get_bucket_pointer");
        }
        return reinterpret_cast<pointer>(mem_ + (n*page_size_));
    }

    inline const_pointer get_bucket_pointer(size_type n) const
    {
        if (n >= N) {
            throw std::out_of_range("bucket_array::get_bucket_pointer");
        }
        return reinterpret_cast<pointer>(mem_ + (n*page_size_));
    }
    
    inline counter_type get_bucket_counter(size_type n) const
    {
        if (n >= N) {
            throw std::out_of_range("bucket_array::get_bucket_counter");
        }
        
        counter_ptr c_ptr = reinterpret_cast<counter_ptr>(mem_ + ((n+1)*page_size_) - sizeof(counter_type));
        
        return c_ptr[0];
    }
    
    inline void prefetch_bucket(size_type n) const
    {
        if (n >= N) {
            throw std::out_of_range("bucket_array::prefetch_bucket");
        }
        void* ptr = mem_ + (n*page_size_);
        if(madvise(ptr, page_size_, MADV_WILLNEED) == -1)
        {
            printf("Bad advice ...\n");
        }
    }

    
private:
    unsigned char* mem_;
    const counter_type bucket_size_;
    const size_type page_size_;
};