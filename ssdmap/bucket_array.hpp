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

    typedef value_type*                           iterator;
    typedef const value_type*                     const_iterator;
    typedef std::reverse_iterator<iterator>       reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;


    typedef C                                 counter_type;
    typedef C&                                counter_ref;
    typedef const C&                          const_counter_ref;
    typedef C*                                counter_ptr;
    typedef const C*                          const_counter_ptr;


    class bucket
    {
    public:
        bucket(unsigned char* ptr, size_type page_size): addr_(ptr), page_size_(page_size)
        {}
        
        inline counter_type size() const
        {
            counter_ptr c_ptr = reinterpret_cast<counter_ptr>(addr_ + page_size_ - sizeof(counter_type));
            
            return c_ptr[0];
        }
        
        inline void set_size(counter_type c)
        {
            counter_ptr c_ptr = reinterpret_cast<counter_ptr>(addr_ + page_size_ - sizeof(counter_type));
            *c_ptr = c;
        }
        inline iterator begin()
        {
            return iterator(reinterpret_cast<pointer>(addr_));
        }
        
        const_iterator begin() const
        {
            return const_iterator(reinterpret_cast<pointer>(addr_));
        }

        
        inline iterator end()
        {
            return iterator(reinterpret_cast<pointer>(addr_) + size());
        }
        
        const_iterator end() const
        {
            return const_iterator(reinterpret_cast<pointer>(addr_) + size());
        }

        reverse_iterator rbegin() {return reverse_iterator(end());}
        const_reverse_iterator rbegin() const {return const_reverse_iterator(end());}
        reverse_iterator rend() {return reverse_iterator(begin());}
        const_reverse_iterator rend() const {return const_reverse_iterator(begin());}

    private:
        unsigned char* addr_;
        const size_type page_size_;
    };
    
    typedef bucket bucket_type;
    
    inline bucket_array(void* ptr, const_counter_ref bucket_size, const size_t& page_size) :
    mem_(static_cast<unsigned char*>(ptr)), bucket_size_(bucket_size), page_size_(page_size)
    {
        // check that the page can contain bucket_size_ elements plus a counter
        if(bucket_size_*sizeof(value_type)+sizeof(counter_type) >  page_size_)
        {
            throw std::runtime_error("Invalid page size.");
        }
        
        if(bucket_size_ > (1ULL<<(8*sizeof(counter_type))))
        {
            throw std::runtime_error("Invalid bucket size.");
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
        if(bucket_size_ > (1ULL<<(8*sizeof(counter_type))))
        {
            throw std::runtime_error("Invalid bucket size.");
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
    
    inline counter_type get_bucket_size(size_type n) const
    {
        if (n >= N) {
            throw std::out_of_range("bucket_array::get_bucket_size");
        }
        
        counter_ptr c_ptr = reinterpret_cast<counter_ptr>(mem_ + ((n+1)*page_size_) - sizeof(counter_type));
        
        return c_ptr[0];
    }
    
    inline bucket_type bucket(size_type n)
    {
        if (n >= N) {
            throw std::out_of_range("bucket_array::bucket");
        }
        
        return bucket_type(mem_ + (n*page_size_), page_size_);
    }
    
    inline const bucket_type bucket(size_type n) const
    {
        if (n >= N) {
            throw std::out_of_range("bucket_array::bucket");
        }
        
        return bucket_type(mem_ + (n*page_size_), page_size_);
    }
    
    inline void prefetch_bucket(size_type n)
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