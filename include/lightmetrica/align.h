/*
    Lightmetrica - A modern, research-oriented renderer

    Copyright (c) 2015 Hisanari Otsu

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#pragma once

#include <lightmetrica/macros.h>
#include <lightmetrica/math.h>
#if LM_PLATFORM_WINDOWS
#include <Windows.h>
#endif
#include <cstddef>

LM_NAMESPACE_BEGIN

namespace
{
    /*!
        \brief Platform independent aligned malloc.

        \param size Size to be allocated.
        \param align Alignment in bytes.
        \return Allocated pointer.
    */
    void* aligned_malloc(size_t size, size_t align)
    {
        void* p;
        
        #if LM_PLATFORM_WINDOWS
        
        p = _aligned_malloc(size, align);
        
        #elif LM_PLATFORM_LINUX

        if ((align & (align - 1)) == 0 && align % sizeof(void*) == 0)
        {
            int result = posix_memalign(&p, align, size);
            if (result != 0)
            {
                p = nullptr;
                #if LM_DEBUG_MODE
                if (result == EINVAL)
                {
                    LM_LOG_WARN("Alignment parameter is not a power of two multiple of sizeof(void*) : align = " + std::to_string(align));
                }
                else if (result == ENOMEM)
                {
                    LM_LOG_DEBUG("Insufficient memory available");
                }
                #endif
            }
        }
        else
        {
            // TODO
            p = malloc(size);
        }

        #else

        // cf.
        // http://www.songho.ca/misc/alignment/dataalign.html
        // http://stackoverflow.com/questions/227897/solve-the-memory-alignment-in-c-interview-question-that-stumped-me
        // http://cottonvibes.blogspot.jp/2011/01/dynamically-allocate-aligned-memory.html
        uintptr_t r = (uintptr_t)malloc(size + --align + sizeof(uintptr_t));
        uintptr_t t = r + sizeof(uintptr_t);
        uintptr_t o = (t + align) & ~(uintptr_t)align;
        if (!r) return nullptr;
        ((uintptr_t*)o)[-1] = r;
        p = (void*)o;

        #endif

        return p;
    }

    /*!
        \brief Platform independent aligned free.

        \param p Pointer to aligned memory.
    */
    void aligned_free(void* p)
    {
        #if LM_PLATFORM_WINDOWS
        _aligned_free(p);
        #elif LM_PLATFORM_LINUX
        free(p);
        #else
        if (!p) return;
        free((void*)(((uintptr_t*)p)[-1]));
        #endif
    }

    /*!
        \brief Check alignment.

        \param p Pointer.
        \param align Alignment in bytes.
    */
    LM_INLINE bool is_aligned(const void* p, size_t align)
    {
        return (uintptr_t)p % align == 0;
    }
}

/*!
    Aligned type.
    \tparam Align Alignment.
*/
template <size_t Align>
class Aligned
{
public:

    void* operator new(std::size_t size) throw (std::bad_alloc)
    {
        void* p = aligned_malloc(size, Align);
        if (!p) throw std::bad_alloc();
        return p;
    }

    void operator delete(void* p)
    {
        aligned_free(p);
    }

};

/*!
    \brief SIMD aligned type.
    Inherited class automatically supports aligned version of
    operator new and delete for SIMD types.
*/
class SIMDAlignedType
{
public:

    void* operator new(std::size_t size) throw (std::bad_alloc)
    {
        #if LM_SINGLE_PRECISION && LM_SSE2
        void* p = aligned_malloc(size, 16);
        #elif LM_DOUBLE_PRECISION && LM_AVX
        void* p = aligned_malloc(size, 32);
        #else
        void* p = malloc(size);
        #endif
        if (!p) throw std::bad_alloc();
        return p;
    }

    void operator delete(void* p)
    {
        #if (LM_SINGLE_PRECISION && LM_SSE2) || (LM_DOUBLE_PRECISION && LM_AVX)
        aligned_free(p);
        #else
        free(p);
        #endif
    }

};

/*!
    \brief Aligned allocator for STL containers.
    \tparam T Data type.
    \tparam Alignment Alignment.
*/
template <typename T, std::size_t Alignment>
class aligned_allocator
{
public:

    // The following will be the same for virtually all allocators.
    typedef T * pointer;
    typedef const T * const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;
    typedef std::size_t size_type;
    typedef ptrdiff_t difference_type;

    T * address(T& r) const { return &r; }
    const T * address(const T& s) const { return &s; }

    std::size_t max_size() const
    {
        // The following has been carefully written to be independent of
        // the definition of size_t and to avoid signed/unsigned warnings.
        return (static_cast<std::size_t>(0) - static_cast<std::size_t>(1)) / sizeof(T);
    }

    // The following must be the same for all allocators.
    template <typename U>
    struct rebind
    {
        typedef aligned_allocator<U, Alignment> other;
    };

    bool operator!=(const aligned_allocator& other) const
    {
        return !(*this == other);
    }

    void construct(T * const p, const T& t) const
    {
        void * const pv = static_cast<void *>(p);
        new (pv) T(t);
    }

    void destroy(T * const p) const
    {
        p->~T();
    }

    // Returns true if and only if storage allocated from *this
    // can be deallocated from other, and vice versa.
    // Always returns true for stateless allocators.
    bool operator==(const aligned_allocator& other) const
    {
        return true;
    }

    // Default constructor, copy constructor, rebinding constructor, and destructor.
    // Empty for stateless allocators.
    aligned_allocator() {}
    aligned_allocator(const aligned_allocator&) {}
    template <typename U> aligned_allocator(const aligned_allocator<U, Alignment>&) {}
    ~aligned_allocator() {}


    // The following will be different for each allocator.
    T * allocate(const std::size_t n) const
    {
        // The return value of allocate(0) is unspecified.
        // Mallocator returns NULL in order to avoid depending
        // on malloc(0)'s implementation-defined behavior
        // (the implementation can define malloc(0) to return NULL,
        // in which case the bad_alloc check below would fire).
        // All allocators can return NULL in this case.
        if (n == 0) {
            return NULL;
        }

        // All allocators should contain an integer overflow check.
        // The Standardization Committee recommends that std::length_error
        // be thrown in the case of integer overflow.
        if (n > max_size())
        {
            throw std::length_error("aligned_allocator<T>::allocate() - Integer overflow.");
        }

        // Mallocator wraps malloc().
        void * const pv = aligned_malloc(n * sizeof(T), Alignment);

        // Allocators should throw std::bad_alloc in the case of memory allocation failure.
        if (pv == NULL)
        {
            throw std::bad_alloc();
        }

        return static_cast<T *>(pv);
    }

    void deallocate(T * const p, const std::size_t /*n*/) const
    {
        aligned_free(p);
    }

    // The following will be the same for all allocators that ignore hints.
    template <typename U>
    T * allocate(const std::size_t n, const U * /* const hint */) const
    {
        return allocate(n);
    }

    // Allocators are not required to be assignable, so
    // all allocators should have a private unimplemented
    // assignment operator. Note that this will trigger the
    // off-by-default (enabled under /Wall) warning C4626
    // "assignment operator could not be generated because a
    // base class assignment operator is inaccessible" within
    // the STL headers, but that warning is useless.
private:

    aligned_allocator& operator=(const aligned_allocator&);

};

/*!
    \brief Aligned allocator for boost::pool.
    \tparam Align Alignment.
*/
template <std::size_t Align>
struct boost_pool_aligned_allocator
{
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    static char* malloc(const size_type bytes) { return static_cast<char*>(aligned_malloc(bytes, Align)); }
    static void free(const char* block) { aligned_free((void*)block); }
};

LM_NAMESPACE_END
