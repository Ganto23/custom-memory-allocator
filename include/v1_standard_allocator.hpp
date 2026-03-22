#pragma once
template <typename T>
class HeapAllocator {
public:
    template <typename... Args>
    T* allocate(Args&&... args){
        return new T{std::forward<Args>(args)...};
    }
    void deallocate(T* ptr) {
        delete ptr;
    }
};