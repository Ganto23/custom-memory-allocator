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



template <typename T, size_t NUM_ELEMENTS>
class CircularBuffer {
    private:
        std::vector<T*> m_buffer;
        const size_t m_capacity;
        size_t m_size;
        size_t m_head;
        mutable std::mutex m_mtx;
        PoolAllocator<T, NUM_ELEMENTS> g_allocator;
    
    public:
        CircularBuffer():
            m_buffer(NUM_ELEMENTS,nullptr),
            m_capacity(NUM_ELEMENTS),
            m_size(0),
            m_head(0)
        {}

        ~CircularBuffer() {
            std::lock_guard<std::mutex> lock(m_mtx);
            for (size_t i = 0; i < m_size; ++i) {
                g_allocator.deallocate(m_buffer[i]);
            }
        }

        void push(const T& element) {
            std::lock_guard<std::mutex> lock(m_mtx);

            if (m_size < m_capacity) {
                m_buffer[m_head] = g_allocator.allocate();
                *m_buffer[m_head] = element;
                m_size ++;
            } else if (m_size == m_capacity) {
                g_allocator.deallocate(m_buffer[m_head]);
                m_buffer[m_head] = g_allocator.allocate();
                *m_buffer[m_head] = element;
            }

            m_head = (m_head + 1) & (m_capacity - 1);
        }

        void get_all(std::vector<T>& elements) const{
            elements.clear();

            std::lock_guard<std::mutex> lock(m_mtx);
            elements.reserve(m_size);

            size_t start_index = 0;
            if (m_size == m_capacity) {
                start_index = m_head;
            }
            for (size_t i = 0; i < m_size; ++i) {
                size_t index = (start_index + i) & (m_capacity - 1);
                elements.push_back(*m_buffer[index]);
            }
        }

        size_t size() const {
            std::lock_guard<std::mutex> lock(m_mtx);
            return m_size;
        }

        size_t capacity() const {
            return m_capacity;
        }
};