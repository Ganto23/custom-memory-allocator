#include <atomic>
#include <cstdint>
#include <utility>

template <typename T, size_t Capacity = 1024>
class LockFreePool {
public:
    LockFreePool() {
        for (size_t i = 0; i < Capacity - 1; ++i) {
            _pool[i].next_index = static_cast<uint32_t>(i + 1);
        }
        _pool[Capacity - 1].next_index = END_OF_LIST;
        _head.store({0, 0}, std::memory_order_release);
    }

    template <typename... Args>
    T* allocate(Args&&... args) {
        TaggedPointer current = _head.load(std::memory_order_acquire);
        while (current.index != END_OF_LIST) {
            uint32_t next_idx = _pool[current.index].next_index;
            TaggedPointer next = {next_idx, current.tag + 1};
            
            if (_head.compare_exchange_weak(current, next, std::memory_order_release, std::memory_order_acquire)) {
                return new (&_pool[current.index].data) T{std::forward<Args>(args)...};
            }
        }
        return nullptr;
    }

    void deallocate(T* ptr) {
        if (!ptr) return;
        
        ptr->~T();
        
        Slot* released_slot = reinterpret_cast<Slot*>(ptr);
        uint32_t released_index = static_cast<uint32_t>(released_slot - _pool);
        
        TaggedPointer current = _head.load(std::memory_order_relaxed);
        TaggedPointer next;
        do {
            released_slot->next_index = current.index;
            next = {released_index, current.tag + 1};
        } while (!_head.compare_exchange_weak(current, next, std::memory_order_release, std::memory_order_acquire));
    }
private:
    static constexpr uint32_t END_OF_LIST = static_cast<uint32_t>(Capacity);

    struct Slot {
        union {
            T data;
            uint32_t next_index;
        };
    };

    struct TaggedPointer {
        uint32_t index;
        uint32_t tag;
    };

    alignas(256) Slot _pool[Capacity];
    alignas(256) std::atomic<TaggedPointer> _head;
};