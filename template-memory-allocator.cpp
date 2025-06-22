#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>  

const size_t NUM_ELEMENTS = 8192;

template <typename T, size_t NUM_ELEMENTS>
class PoolAllocator {
    public:
        PoolAllocator() {
            memory_pool = new char[NUM_ELEMENTS * sizeof(T)];

            for (size_t i = 0; i < NUM_ELEMENTS - 1; ++i) {
                T* current_chunk = reinterpret_cast<T*>(&memory_pool[i * sizeof(T)]);
                T* next_chunk = reinterpret_cast<T*>(&memory_pool[(i+1) * sizeof(T)]);
                *reinterpret_cast<T**>(current_chunk) = next_chunk;
            }

            T* last_chunk = reinterpret_cast<T*>(&memory_pool[(NUM_ELEMENTS - 1) * sizeof(T)]);
            *reinterpret_cast<T**>(last_chunk) = nullptr;

            head_of_free_list = reinterpret_cast<T*>(memory_pool);
        }

        ~PoolAllocator() {
            delete[] memory_pool;
        }

        T* allocate() {
            std::lock_guard<std::mutex> lock(mtx);
            if (head_of_free_list == nullptr) {
                throw std::bad_alloc(); 
            }
            T* return_ptr = reinterpret_cast<T*>(head_of_free_list);
            head_of_free_list = *reinterpret_cast<T**>(head_of_free_list);
            return return_ptr;
        }

        void deallocate(T* ptr) {
            std::lock_guard<std::mutex> lock(mtx);
            T* copy = head_of_free_list;
            head_of_free_list = ptr;
            *reinterpret_cast<T**>(head_of_free_list) = copy;
        }


    private:
        char* memory_pool;
        T* head_of_free_list;
        std::mutex mtx;
};

int main() {
    std::cout << "Memory Allocator Template Example" << std::endl;

    // Example usage of PoolAllocator
    PoolAllocator<int, NUM_ELEMENTS> intAllocator;
    PoolAllocator<double, NUM_ELEMENTS> doubleAllocator;

    // You can add more functionality to the PoolAllocator class as needed.

    return 0;
}

 