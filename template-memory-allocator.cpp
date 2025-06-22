#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <string>
#include <new>

// --- 1. CONFIGURATION CONSTANT ---
// Using a power of 2 allows for ultra-fast bitwise indexing.
const size_t TRADE_BUFFER_CAPACITY = 8192;

// --- 2. THE TRADE STRUCTURE ---
struct Trade {
    uint64_t timestamp = 0;
    uint64_t trade_id = 0;
    char symbol[16] = {0};
    double price = 0.0;
    uint32_t size = 0;
    bool side = false;

    Trade() = default;
    Trade(const Trade& other) = default;
};

// --- 3. THE GENERIC POOL ALLOCATOR ---
template <typename T, size_t N>
class PoolAllocator {
public:
    PoolAllocator() {
        m_pool = new char[N * sizeof(T)];
        for (size_t i = 0; i < N - 1; ++i) {
            T* current = reinterpret_cast<T*>(&m_pool[i * sizeof(T)]);
            T* next = reinterpret_cast<T*>(&m_pool[(i + 1) * sizeof(T)]);
            *reinterpret_cast<T**>(current) = next;
        }
        T* last = reinterpret_cast<T*>(&m_pool[(N - 1) * sizeof(T)]);
        *reinterpret_cast<T**>(last) = nullptr;
        m_head_of_free_list = reinterpret_cast<T*>(m_pool);
    }

    ~PoolAllocator() {
        delete[] m_pool;
    }

    // Deleted copy constructor and assignment operator to prevent copying.
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    T* allocate() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_head_of_free_list) {
            throw std::bad_alloc();
        }
        T* block = m_head_of_free_list;
        m_head_of_free_list = *reinterpret_cast<T**>(block);
        return block;
    }

    void deallocate(T* ptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        *reinterpret_cast<T**>(ptr) = m_head_of_free_list;
        m_head_of_free_list = ptr;
    }

private:
    char* m_pool;
    T* m_head_of_free_list;
    std::mutex m_mutex;
};

// --- 4. THE GENERIC CIRCULAR BUFFER ---
template <typename T, size_t N>
class CircularBuffer {
private:
    std::vector<T*> m_buffer;
    const size_t m_capacity;
    size_t m_size;
    size_t m_head;
    mutable std::mutex m_mutex;
    PoolAllocator<T, N> m_allocator;

public:
    CircularBuffer() :
        m_buffer(N, nullptr),
        m_capacity(N),
        m_size(0),
        m_head(0) {}

    ~CircularBuffer() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (size_t i = 0; i < m_size; ++i) {
            m_allocator.deallocate(m_buffer[i]);
        }
    }

    void push(const T& element) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        T* new_element_ptr = m_allocator.allocate();
        *new_element_ptr = element; 

        if (m_size < m_capacity) {
            m_buffer[m_head] = new_element_ptr;
            m_size++;
        } else {
            m_allocator.deallocate(m_buffer[m_head]);
            m_buffer[m_head] = new_element_ptr;
        }
        m_head = (m_head + 1) & (m_capacity - 1);
    }

    void get_all(std::vector<T>& elements) const {
        elements.clear();
        std::lock_guard<std::mutex> lock(m_mutex);
        elements.reserve(m_size);

        size_t start_index = (m_size == m_capacity) ? m_head : 0;
        for (size_t i = 0; i < m_size; ++i) {
            size_t index = (start_index + i) & (m_capacity - 1);
            elements.push_back(*m_buffer[index]);
        }
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size;
    }
};

// --- 5. SIMULATION AND BENCHMARKING ---
using TradeBuffer = CircularBuffer<Trade, TRADE_BUFFER_CAPACITY>;

void writer_thread_func(TradeBuffer& buffer) {
    for (int i = 0; i < 2'000'000; ++i) {
        Trade t;
        t.trade_id = i;
        buffer.push(t);
    }
}

void reader_thread_func(const TradeBuffer& buffer) {
    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::vector<Trade> trades;
        buffer.get_all(trades);
    }
}

void run_benchmark() {
    const int NUM_OPS = 5'000'000;
    std::cout << "\n--- Running Accurate Allocator Benchmark ---" << std::endl;
    std::cout << "Performing " << NUM_OPS << " allocation/deallocation cycles." << std::endl;

    // Test 1: Standard Allocator
    auto start1 = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < NUM_OPS; ++i) {
        Trade* t = new Trade();
        delete t;
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> standard_ms = end1 - start1;
    std::cout << "Standard new/delete Time: " << standard_ms.count() << " ms" << std::endl;

    // Test 2: Custom Pool Allocator
    PoolAllocator<Trade, TRADE_BUFFER_CAPACITY> pool;
    auto start2 = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < NUM_OPS; ++i) {
        Trade* t = pool.allocate();
        pool.deallocate(t);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> custom_ms = end2 - start2;
    std::cout << "Custom PoolAllocator Time: " << custom_ms.count() << " ms" << std::endl;
    
    std::cout << "Speedup Factor: " << standard_ms.count() / custom_ms.count() << "x" << std::endl;
    std::cout << "------------------------------------------\n" << std::endl;
}


// --- 6. MAIN FUNCTION ---
int main() {
    std::cout << "--- Running Multi-Threaded Simulation ---" << std::endl;
    TradeBuffer sim_buffer;
    std::thread writer(writer_thread_func, std::ref(sim_buffer));
    std::thread reader(reader_thread_func, std::cref(sim_buffer));
    writer.join();
    reader.join();
    std::cout << "Simulation completed successfully." << std::endl;

    run_benchmark();

    return 0;
}
