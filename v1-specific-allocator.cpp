#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>

const size_t NUM_TRADES = 8192;
const size_t BUFFER_SIZE = 8192;

// 1. THE TRADE STRUCTURE
struct Trade {
    uint64_t timestamp;
    uint64_t trade_id;
    char       symbol[16];
    double     price;
    uint32_t   size;
    bool       side;

    static void* operator new(size_t size);
    static void operator delete(void* ptr);
};


// 2. THE ALLOCATOR CLASS
class TradeAllocator {
    public:
        TradeAllocator() {
            memory_pool = new char[NUM_TRADES * sizeof(Trade)];

            for (size_t i = 0; i < NUM_TRADES - 1; ++i) {
                Trade* current_chunk = reinterpret_cast<Trade*>(&memory_pool[i * sizeof(Trade)]);
                Trade* next_chunk = reinterpret_cast<Trade*>(&memory_pool[(i+1) * sizeof(Trade)]);
                *reinterpret_cast<Trade**>(current_chunk) = next_chunk;
            }

            Trade* last_chunk = reinterpret_cast<Trade*>(&memory_pool[(NUM_TRADES - 1) * sizeof(Trade)]);
            *reinterpret_cast<Trade**>(last_chunk) = nullptr;

            head_of_free_list = reinterpret_cast<Trade*>(memory_pool);
        }

        ~TradeAllocator() {
            delete[] memory_pool;
        }

        Trade* allocate() {
            std::lock_guard<std::mutex> lock(mtx);
            if (head_of_free_list == nullptr) {
                throw std::bad_alloc(); 
            }
            Trade* return_ptr = reinterpret_cast<Trade*>(head_of_free_list);
            head_of_free_list = *reinterpret_cast<Trade**>(head_of_free_list);
            return return_ptr;
        }

        void deallocate(Trade* ptr) {
            std::lock_guard<std::mutex> lock(mtx);
            Trade* copy = head_of_free_list;
            head_of_free_list = ptr;
            *reinterpret_cast<Trade**>(head_of_free_list) = copy;
        }


    private:
        char* memory_pool;
        Trade* head_of_free_list;
        std::mutex mtx;
};

// Create the single global instance of the allocator.
TradeAllocator g_allocator;


// 3. OVERLOAD NEW AND DELETE FOR 'TRADE'
void* Trade::operator new(size_t size) {
    return g_allocator.allocate();
}

void Trade::operator delete(void* ptr) {
    g_allocator.deallocate(static_cast<Trade*>(ptr));
}


// 4. THE CIRCULAR BUFFER CLASS
class CircularBuffer {
    private:
        std::vector<Trade*> m_buffer;
        const size_t m_capacity;
        size_t m_size;
        size_t m_head;
        mutable std::mutex m_mtx;
    
    public:
        CircularBuffer():
            m_buffer(BUFFER_SIZE,nullptr),
            m_capacity(BUFFER_SIZE),
            m_size(0),
            m_head(0)
        {}

        ~CircularBuffer() {
            std::lock_guard<std::mutex> lock(m_mtx);
            for (size_t i = 0; i < m_size; ++i) {
                delete m_buffer[i];
            }
        }

        void push(const Trade& trade) {
            std::lock_guard<std::mutex> lock(m_mtx);

            if (m_size < m_capacity) {
                m_buffer[m_head] = new Trade(trade);
                m_size ++;
            } else if (m_size == m_capacity) {
                delete m_buffer[m_head];
                m_buffer[m_head] = new Trade(trade);
            }

            m_head = (m_head + 1) & (m_capacity - 1);
        }

        void get_all(std::vector<Trade>& trades) const{
            trades.clear();

            std::lock_guard<std::mutex> lock(m_mtx);
            trades.reserve(m_size);

            size_t start_index = 0;
            if (m_size == m_capacity) {
                start_index = m_head;
            }
            for (size_t i = 0; i < m_size; ++i) {
                size_t index = (start_index + i) & (m_capacity - 1);
                trades.push_back(*m_buffer[index]);
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

// 5. MEMORY ESCAPE FUNCTION
void escape(void* p) {
    asm volatile("" : : "g"(p) : "memory");
}


// 6. THE SIMULATION LOGIC
void writer_thread_func(CircularBuffer& buffer) {
    const int NUM_WRITES = 10000000;
    for (int i = 0; i < NUM_WRITES; ++i) {
        Trade trade;
        trade.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        trade.trade_id = i;
        snprintf(trade.symbol, sizeof(trade.symbol), "SYM%d", i % 100);
        trade.price = 100.0 + (i % 100) * 0.01;
        trade.size = 1 + (i % 100);
        trade.side = (i % 2 == 0);

        buffer.push(trade);
    }
}

void reader_thread_func(const CircularBuffer& buffer) {
    const int NUM_READS = 20;
    for (int i = 0; i < NUM_READS; ++i) {
        std::vector<Trade> trades;
        buffer.get_all(trades);
        for (const Trade& trade : trades) {
            escape((void*)&trade);
        }
    }
}

// --- 7. ACCURATE BENCHMARKING ---
void run_benchmark() {
    const int NUM_OPERATIONS = 5'000'000;
    
    std::vector<Trade*> trade_pointers;
    trade_pointers.reserve(BUFFER_SIZE);

    std::cout << "\n--- Running Accurate Benchmark ---" << std::endl;
    std::cout << "Performing " << NUM_OPERATIONS 
              << " allocation/deallocation cycles." << std::endl;
    std::cout << "Buffer Capacity: " << BUFFER_SIZE << std::endl;
    
    // --- Start the Clock ---
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        if (i < BUFFER_SIZE) {
            trade_pointers.push_back(new Trade());
        } else {
            size_t index_to_replace = i % BUFFER_SIZE;
            delete trade_pointers[index_to_replace];
            trade_pointers[index_to_replace] = new Trade();
        }
    }
    
    // --- Stop the Clock ---
    auto end_time = std::chrono::high_resolution_clock::now();
    
    for (Trade* ptr : trade_pointers) {
        delete ptr;
    }
    
    // --- Report Results ---
    std::chrono::duration<double, std::milli> elapsed_ms = end_time - start_time;
    double ops_per_second = NUM_OPERATIONS / (elapsed_ms.count() / 1000.0);

    std::cout << "\nTotal Time Elapsed: " << elapsed_ms.count() << " ms" << std::endl;
    std::cout << "Operations per second: " << static_cast<long long>(ops_per_second) << std::endl;
    std::cout << "---------------------------\n" << std::endl;
}


// 8. THE MAIN FUNCTION
int main() {
    auto start_time = std::chrono::high_resolution_clock::now();
    CircularBuffer buffer;

    std::thread writer_thread(writer_thread_func, std::ref(buffer));
    std::thread reader_thread(reader_thread_func, std::cref(buffer));

    writer_thread.join();
    reader_thread.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed_ms = end_time - start_time;
    std::cout << "Simulation completed in " << elapsed_ms.count() << " ms" << std::endl;
    return 0;
}