#pragma once
#include <atomic>
#include <cstdint>
#include <utility>
#include <sys/mman.h>
#include <cstdlib>
#include <cstdio>

namespace hft::memory {

template <typename T, size_t Capacity = 1024>
class ThreadLocalPool {
public:
    ThreadLocalPool() {
        
        size_t size = sizeof(Slot) * Capacity;

        _pool = (Slot*)mmap(nullptr, size, PROT_READ | PROT_WRITE, 
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

        if (_pool == MAP_FAILED) {
            _pool = (Slot*)aligned_alloc(256, size); 
            _used_mmap = false;
        } else {
            _used_mmap = true;
        }

        for (size_t i = 0; i < Capacity - 1; ++i) {
            _pool[i].next_index = static_cast<uint32_t>(i + 1);
        }
        _pool[Capacity - 1].next_index = END_OF_LIST;
        _head.store({0, 0}, std::memory_order_release);
    }

    ~ThreadLocalPool() {
        if (!_pool) return;
        
        size_t size = sizeof(Slot) * Capacity;
        
        if (_used_mmap) {
            munmap(_pool, size);
        } else {
            free(_pool);
        }
        
        _pool = nullptr;
    }

    template <typename... Args>
    T* allocate(Args&&... args) {
        if (local_cache.count == 0) [[unlikely]] {
            TaggedPointer current = _head.load(std::memory_order_acquire);
            int backoff = 0;

            while (current.index != END_OF_LIST) [[likely]] {
                uint32_t first = current.index;
                uint32_t last = first;
                size_t grabbed = 1;
                
                for (; grabbed < Magazine::BatchSize; ++grabbed) {
                    uint32_t next = _pool[last].next_index;
                    if (next == END_OF_LIST) [[unlikely]] {break;}
                    last = next;
                }

                uint32_t new_head_idx = _pool[last].next_index;
                TaggedPointer next_head = {new_head_idx, current.tag + 1};

                if (_head.compare_exchange_weak(current, next_head, std::memory_order_acquire, std::memory_order_acquire)) {
                    uint32_t return_index = first;
                    uint32_t fill_ptr = _pool[first].next_index;
                    for (size_t i = 1; i < grabbed; ++i) {
                        local_cache.indices[local_cache.count++] = fill_ptr;
                        fill_ptr = _pool[fill_ptr].next_index;
                    }
                    return new (&_pool[return_index].data) T{std::forward<Args>(args)...};
                }
                for (int i = 0; i < (1 << backoff); ++i) {
                    asm volatile("yield" ::: "memory"); 
                }
                if (backoff < 8) {backoff++;}
            }
            std::fprintf(stderr, "FATAL: HFTAllocator Pool Exhausted. Halting.\n");
            std::abort();
        }

        return new (&_pool[local_cache.indices[--local_cache.count]].data) T{std::forward<Args>(args)...};
        
    }

    void deallocate(T* ptr) {
        if (!ptr) return;
        
        ptr->~T();
        
        Slot* released_slot = reinterpret_cast<Slot*>(ptr);
        uint32_t released_index = static_cast<uint32_t>(released_slot - _pool);

        if (local_cache.count < Magazine::BatchSize) [[likely]] {
            local_cache.indices[local_cache.count++] = released_index;
            return;
        }
        
        size_t flush_count = Magazine::BatchSize / 2;
        uint32_t chain_head = released_index;
        uint32_t current_in_chain = chain_head;

        for (size_t i = 0; i < flush_count; ++i) {
            uint32_t next_from_cache = local_cache.indices[--local_cache.count];
            _pool[current_in_chain].next_index = next_from_cache;
            current_in_chain = next_from_cache;
        }

        TaggedPointer current_head = _head.load(std::memory_order_relaxed);
        int backoff = 0;
        while (true) {
            _pool[current_in_chain].next_index = current_head.index;
            TaggedPointer next_head = {chain_head, current_head.tag + 1};

            if (_head.compare_exchange_weak(current_head, next_head, std::memory_order_release, std::memory_order_acquire)) {
                break;
            }

            for (int i = 0; i < (1 << backoff); ++i) asm volatile("yield" ::: "memory");
            if (backoff < 8) backoff++;
        }
    }
private:
    static constexpr uint32_t END_OF_LIST = static_cast<uint32_t>(Capacity);

    struct alignas(16) Slot {
        union {
            T data;
            uint32_t next_index;
        };
    };

    struct TaggedPointer {
        uint32_t index;
        uint32_t tag;
    };

    struct alignas(64) Magazine {
        static constexpr size_t BatchSize = 64;
        uint32_t indices[BatchSize];
        size_t count = 0;
    };

    alignas(64) Slot* _pool;
    bool _used_mmap;
    alignas(256) std::atomic<TaggedPointer> _head;
    static thread_local Magazine local_cache;
};


template <typename T, size_t Capacity>
thread_local typename ThreadLocalPool<T, Capacity>::Magazine ThreadLocalPool<T, Capacity>::local_cache;
}