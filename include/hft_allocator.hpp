#pragma once
#include <cstddef>
#include <new>
#include "v4_thread_local_pool.hpp"

namespace hft::memory {

template <typename T>
class HFTAllocator {
    inline static ThreadLocalPool<T, 1000000> pool;
public:
    using value_type = T;

    HFTAllocator() noexcept = default;

    template <typename U>
    HFTAllocator(const HFTAllocator<U>&) noexcept {}

    template <typename U>
    struct rebind {
        using other = HFTAllocator<U>;
    };

    T* allocate(std::size_t n) {
        if (n == 1) {
            return pool.allocate(); 
        }
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t n) {
        if (n == 1) {
            pool.deallocate(p);
        } else {
            ::operator delete(p);
        }

    }

    template <typename U>
    bool operator==(const HFTAllocator<U>&) const noexcept {
        return true; 
    }

    template <typename U>
    bool operator!=(const HFTAllocator<U>&) const noexcept {
        return false; 
    }
};
}