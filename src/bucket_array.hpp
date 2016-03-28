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

/** @file bucket_array.hpp
 * @brief Header that defines the bucket_array representation class.
 *
 *
 */


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

namespace ssdmap {
    
    
/** @class bucket_array
 *  @brief An array of bucket representation of memory.
 *
 *  The bucket_array is an abstraction layer that allows representation of a memory range as an array of page resident buckets.
 *  In particular, the bucket_array class does not allocate memory.
 *
 *  @tparam T   Type of the content values.  Aliased as member type bucket_map::value_type.
 *  @tparam C   Type of the counter values.  It must be a PoD type. This defaults to uint16_t, so the buckets can store 2^16-1 elements. Aliased as member type bucket_map::counter_type.
 */
    
template <class T, class C = uint16_t>
class bucket_array {
public:
    typedef T                                 value_type;       /**< @brief The first template parameter (T)	*/
    typedef T&                                reference;        /**< @brief value_type&	*/
    typedef const T&                          const_reference;  /**< @brief const value_type&	*/
    typedef T*                                pointer;          /**< @brief value_type*	*/
    typedef const T*                          const_pointer;    /**< @brief const value_type*	*/
    typedef size_t                            size_type;        /**< @brief size_t	*/

    typedef value_type*                           iterator;     /**< @brief a forward iterator to value_type	*/
    typedef const value_type*                     const_iterator; /**< @brief a forward iterator to const value_type	*/
    typedef std::reverse_iterator<iterator>       reverse_iterator; /**< @brief reverse_iterator<iterator>	*/
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator; /**< @brief reverse_iterator<const_iterator>	*/


    typedef C                                 counter_type;     /**< @brief The second template parameter (C)	*/
    typedef C&                                counter_ref;      /**< @brief counter_type&	*/
    typedef const C&                          const_counter_ref;/**< @brief const counter_type&	*/
    typedef C*                                counter_ptr;      /**< @brief counter_type*	*/
    typedef const C*                          const_counter_ptr;/**< @brief const counter_type*	*/


    /** @class bucket
     *  @brief A bucket representation of memory.
     *
     *  The bucket class allows for easy manipulation of the buckets of the bucket_array class, in particular using iterators.
     *
     */
    
    class bucket
    {
    public:
        /**
         *  @brief Constructor
         *
         *  Initialize a bucket of bucket array @a a with address @a ptr.
         *
         *  @param  ptr The memory address of the bucket.
         *  @param  a   The bucket_array the bucket belongs to.
         */
        bucket(unsigned char* ptr, bucket_array* a): addr_(ptr), array_(a)
        {}
        
        /**
         *  @brief Return the bucket size.
         *
         *  Returns the number of elements in the bucket.
         *
         *  @return The number of elements in the bucket.
         */
        
        inline counter_type size() const
        {
            counter_ptr c_ptr = reinterpret_cast<counter_ptr>(addr_ + array_->page_size() - sizeof(counter_type));
            
            return c_ptr[0];
        }
        
        /**
         *  @brief Set the size counter.
         * 
         *  Sets the size counter to @a c.
         *
         *  @param  c   The new size counter.
         */
        inline void set_size(counter_type c)
        {
            counter_ptr c_ptr = reinterpret_cast<counter_ptr>(addr_ + array_->page_size() - sizeof(counter_type));
            *c_ptr = c;
        }

        //@{
        /**
         *  @brief Return iterator to beginning.
         *
         *  Returns an iterator pointing to the first element of the bucket.
         *  Member types iterator and const_iterator are forward iterator types (pointing to an element and to a const element, respectively).
         *  @return An iterator to the beginning of the bucket.
         */
        inline iterator begin()
        {
            return iterator(reinterpret_cast<pointer>(addr_));
        }
        
        const_iterator begin() const
        {
            return const_iterator(reinterpret_cast<pointer>(addr_));
        }
        //@}
        
        //@{
        /**
         *  @brief Return iterator to end.
         *
         *  Returns an iterator referring to the past-the-end element of the bucket.
         *  The past-the-end element is the theoretical element that would follow the last element in the bucket. It does not point to any element, and thus shall not be dereferenced.
         *  If the container is empty, this function returns the same as bucket::begin.
         *
         *  @return An iterator to the element past the end of the sequence.
         */

        inline iterator end()
        {
            return iterator(reinterpret_cast<pointer>(addr_) + size());
        }
        
        const_iterator end() const
        {
            return const_iterator(reinterpret_cast<pointer>(addr_) + size());
        }
        
        //@}

        //@{
        /**
         *  @brief Return reverse iterator to reverse beginning.
         *
         *  Returns a reverse iterator pointing to the last element in the container (i.e., its reverse beginning).
         *  Reverse iterators iterate backwards: increasing them moves them towards the beginning of the container.
         *  rbegin points to the element right before the one that would be pointed to by member end.
         *  Member types reverse_iterator and const_reverse_iterator are reverse forward iterator types (pointing to an element and to a const element, respectively)
         
         *  @return A reverse iterator to the reverse beginning of the sequence container.
         */
        reverse_iterator rbegin() {return reverse_iterator(end()-1);}
        const_reverse_iterator rbegin() const {return const_reverse_iterator(end()-1);}
        
        //@}

        //@{
        /**
         *  @brief Return reverse iterator to reverse end.
         *
         *  Returns a reverse iterator pointing to the theoretical element preceding the first element in the list container (which is considered its reverse end).
         * The range between bucket::rbegin and bucket::rend contains all the elements of the bucket (in reverse order).
         *
         *  @return A reverse iterator to the reverse end of the sequence container.
         */
        reverse_iterator rend() {return reverse_iterator(begin()-1);}
        const_reverse_iterator rend() const {return const_reverse_iterator(begin()-1);}
        //@}

        /**
         *  @brief Append an element to the bucket.
         *
         *  Appends the value @a v at the end of the bucket if possible (i.e. if the bucket is not full).
         *
         *  @param  v   The value to be appended.
         *
         *  @retval true    if the value was successfully appended.
         *  @retval false   if the bucket was full.
         */
        inline bool append(const value_type &v)
        {
            counter_ptr c_ptr = reinterpret_cast<counter_ptr>(addr_ + array_->page_size() - sizeof(counter_type));

            if (*c_ptr == array_->bucket_size()) {
                return false;
            }

            value_type *ptr = reinterpret_cast<pointer>(addr_) + size();
            *ptr = v;
            *c_ptr = (*c_ptr) + 1;

            return true;
        }
        
        /**
         *  @brief Prefetch the bucket in memory.
         *
         *  Prefetches the content of the bucket in memory using madvise().
         *
         */
        inline void prefetch() const
        {
            if(madvise(addr_, array_->page_size(), MADV_WILLNEED) == -1)
            {
                printf("Bad advice ...\n");
            }
        }
    private:
        unsigned char* addr_;
        bucket_array* array_;
    };
    
    typedef bucket bucket_type; /**< @brief bucket	*/

    /**
     *  @brief Return the optimal size of buckets
     *
     *  Returns the maximal size of a bucket so that it fully resides in a single page.
     *
     *  @param  page_size   The page size (in bytes) for which the optimal bucket size will be computed.
     *
     *  @return The optimal bucket size.
     */
    inline static size_t optimal_bucket_size(const size_t page_size)
    {
        return (page_size - sizeof(counter_type))/sizeof(value_type);
    }

    /**
     *  @brief Constructor
     *
     *  Constructs a new bucket array representation of the memory at address @a ptr.
     *  The constructor will raise exceptions if the buckets cannot fit in a single page (i.e. if bucket_size*sizeof(value_type)+sizeof(counter_type) >  page_size) or if the counter type is too small to address the bucket (i.e. if bucket_size_ > 2^(8*sizeof(counter_type)).
     *
     *  @param  ptr         The memory address that will be represented as a bucket array.
     *  @param  N           The number of buckets.
     *  @param  bucket_size The size of bucket.
     *  @param  page_size   The size of a memory page.
     *
     *  @exception std::runtime_error("Invalid page size.") With the given bucket_size, value_type, counter_type and page_size, a bucket cannot fit in a single page.
     *  @exception std::runtime_error("Invalid bucket size.") The range of the bucket cannot be addressed with the counter_type.
     */
    inline bucket_array(void* ptr, const size_type N, const_counter_ref bucket_size, const size_t& page_size) :
     N_(N), mem_(static_cast<unsigned char*>(ptr)), bucket_size_(bucket_size), page_size_(page_size)
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
    
    /**
     *  @brief Constructor
     *
     *  Constructs a new bucket array representation of the memory at address @a ptr, and choose the optimal bucket size for the input page size.
     *
     *  @param  ptr         The memory address that will be represented as a bucket array.
     *  @param  N           The number of buckets.
     *  @param  page_size   The size of a memory page.
     *
     *  @exception std::runtime_error("Invalid page size.") With the given bucket_size, value_type, counter_type and page_size, a bucket cannot fit in a single page.
     *  @exception std::runtime_error("Invalid bucket size.") The range of the bucket cannot be addressed with the counter_type.
     */
    inline bucket_array(void* ptr, const size_type N, const size_t& page_size) :
    N_(N), mem_(static_cast<unsigned char*>(ptr)), bucket_size_((page_size - sizeof(counter_type))/sizeof(value_type)), page_size_(page_size)
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
    
    /**
     *  @brief Return the bucket size.
     *
     *  Return the maximum number of elements that can be stored in a bucket.
     *
     *  @return The maximum number of elements of a bucket.
     */
    inline counter_type bucket_size() const
    {
        return bucket_size_;
    }
    
    /**
     *  @brief Return the page size.
     *
     *  Return the page size used for the bucket array.
     *
     *  @return The page size (in bytes).
     */
    inline size_type page_size() const
    {
        return page_size_;
    }

    /**
     *  @brief Return the number of buckets.
     *
     *  Return the number of buckets represented by the bucket array.
     *
     *  @return The number of buckets.
     */
    inline size_type bucket_count() const
    {
        return N_;
    }
    
    //@{
    /**
     *  @brief Get the memory address of a bucket.
     *
     *  Returns the address of the @a n -th bucket.
     *  
     *  @exception std::out_of_range @a n is out of range.
     *
     *  @return A pointer to the start of the @a n -th bucket.
     */
    
    inline pointer get_bucket_pointer(size_type n)
    {
        if (n >= N_) {
            throw std::out_of_range("bucket_array::get_bucket_pointer");
        }
        return reinterpret_cast<pointer>(mem_ + (n*page_size()));
    }

    inline const_pointer get_bucket_pointer(size_type n) const
    {
        if (n >= N_) {
            throw std::out_of_range("bucket_array::get_bucket_pointer");
        }
        return reinterpret_cast<pointer>(mem_ + (n*page_size()));
    }
    //@}

    
    /**
     *  @brief Return the size of a bucket.
     *
     *  Returns the number of elements in the @a n -th bucket.
     *
     *  @exception std::out_of_range @a n is out of range.
     *
     *  @return The number of elements in the @a n -th bucket bucket.
     */
    inline counter_type get_bucket_size(size_type n) const
    {
        if (n >= N_) {
            throw std::out_of_range("bucket_array::get_bucket_size");
        }
        
        counter_ptr c_ptr = reinterpret_cast<counter_ptr>(mem_ + ((n+1)*page_size()) - sizeof(counter_type));
        
        return c_ptr[0];
    }
    
    //@{
    /**
     *  @brief Access a bucket.
     *
     *  Returns the @a n -th bucket.
     *
     *  @exception std::out_of_range @a n is out of range.
     *
     *  @return A bucket instance representing the @a n -th bucket.
     */
    inline bucket_type bucket(size_type n)
    {
        if (n >= N_) {
            throw std::out_of_range("bucket_array::bucket");
        }
        
        return bucket_type(mem_ + (n*page_size()), this);
    }
    
    inline const bucket_type bucket(size_type n) const
    {
        if (n >= N_) {
            throw std::out_of_range("bucket_array::bucket");
        }
        
        return bucket_type(mem_ + (n*page_size()), this);
    }
    //@}
    
    /**
     *  @brief Prefetch a bucket in memory.
     *
     *  Prefetches the content of the @a n -th bucket in memory using madvise().
     *
     *  @exception std::out_of_range @a n is out of range.
     */
    inline void prefetch_bucket(size_type n)
    {
        if (n >= N_) {
            throw std::out_of_range("bucket_array::prefetch_bucket");
        }
        void* ptr = mem_ + (n*page_size());
        if(madvise(ptr, page_size_, MADV_WILLNEED) == -1)
        {
            printf("Bad advice ...\n");
        }
    }

    
private:
    const size_type N_;
    unsigned char* mem_;
    const counter_type bucket_size_;
    const size_type page_size_;
};

} // namespace ssdmap