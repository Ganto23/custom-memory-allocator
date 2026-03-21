#include <memory>
#include <cstddef>

template <typename T, size_t Capacity = 1024>
class LockFreePool {
public:
    LockFreePool() {
        _head.store(_pool, std::memory_order_relaxed);
        for (size_t i = 0; i < Capacity-1; ++i) {
            _pool[i].ptr = &_pool[i+1];
        }
        _pool[Capacity - 1].ptr = nullptr;
    }

    template <typename... Args>
    T* allocate(Args&&... args) {
        Slot* current_slot = _head.load(std::memory_order_acquire);
        while (current_slot && !_head.compare_exchange_weak(current_slot, current_slot->ptr, std::memory_order_release, std::memory_order_acquire)){}
        if (!current_slot) [[unlikely]] { return nullptr;}
        return new (&current_slot->data) T{std::forward<Args>(args)...};
    }

    void deallocate(T* ptr) {
        if (!ptr) return;
        ptr->~T();
        Slot* new_head = reinterpret_cast<Slot*>(ptr);
        Slot* old_head = _head.load(std::memory_order_relaxed);

        do {
            new_head->ptr = old_head;
        } while (!_head.compare_exchange_weak(old_head, new_head, std::memory_order_release, std::memory_order_acquire)){}
    }

private:
    union alignas(alignof(T)) Slot {
        std::byte data[sizeof(T)]; 
        Slot* ptr;
    };

    Slot _pool[Capacity];
    std::atomic<Slot*> _head;
};