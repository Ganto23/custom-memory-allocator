#include <mutex>
#include <cstddef>

template <typename T, size_t Capacity = 1024>
class PoolAllocator {
public:
    PoolAllocator() {
        _head = _pool;
        for (size_t i = 0; i < Capacity-1; ++i) {
            _pool[i].ptr = &_pool[i+1];
        }
        _pool[Capacity - 1].ptr = nullptr;
    }

    template <typename... Args>
    T* allocate(Args&&... args) {
        Slot* current_slot;
        {
            std::lock_guard<std::mutex> lock(_mtx);
            if (_head == nullptr) [[unlikely]] {
                return nullptr;
            }
            current_slot = _head;
            _head = _head->ptr;
        }
        return new (&current_slot->data) T(std::forward<Args>(args)...);
    }

    void deallocate(T* ptr) {
        if (!ptr) return;
        ptr->~T();
        Slot* new_head = reinterpret_cast<Slot*>(ptr);
        {
            std::lock_guard<std::mutex>> lock(_mtx);
            new_head->ptr = _head;
            _head = new_head;
        }

    }

private:
    union alignas(alignof(T)) Slot {
        std::byte data[sizeof(T)]; 
        Slot* ptr;
    };

    std::mutex _mtx;
    Slot _pool[Capacity];
    Slot* _head;
};