#ifndef MEMORY_MANAGER_HPP
#define MEMORY_MANAGER_HPP

#include <cstddef>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>

/**
 * @brief Memory allocation statistics
 */
struct MemoryStats {
    size_t total_allocated = 0;
    size_t total_freed = 0;
    size_t current_usage = 0;
    size_t peak_usage = 0;
    size_t allocation_count = 0;
    size_t free_count = 0;
    size_t fragmentation_count = 0;
    size_t page_fault_count = 0;
    size_t swap_usage = 0;
    float fragmentation_percentage = 0.0f;
    std::chrono::system_clock::time_point last_allocation_time;
    
    // Additional metrics
    size_t heap_usage = 0;
    size_t stack_usage = 0;
    size_t mmap_usage = 0;
    size_t shared_memory_usage = 0;
    size_t zram_usage = 0;
    float compression_ratio = 1.0f;
};

/**
 * @brief Advanced memory manager for Raspberry Pi 5
 * 
 * Implements custom memory management with MMU access,
 * virtual memory mapping, and paging optimized for
 * the ARM Cortex-A76 architecture.
 */
class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();
    
    // Core memory management
    void* allocate(size_t size);
    void* allocate_aligned(size_t size, size_t alignment);
    void deallocate(void* ptr);
    void* reallocate(void* ptr, size_t new_size);
    void* allocate_zeroed(size_t size);
    
    // Virtual memory management
    void* map_file(const std::string& file_path, size_t size);
    void* map_file_readonly(const std::string& file_path, size_t size);
    void* map_anonymous(size_t size);
    void unmap_file(void* ptr, size_t size);
    bool protect_memory(void* ptr, size_t size, int protection);
    
    // Memory paging
    void enable_paging();
    void disable_paging();
    bool is_paging_enabled() const { return paging_enabled; }
    void* page_in(void* virtual_address);
    void page_out(void* virtual_address);
    void* allocate_pages(size_t page_count);
    void free_pages(void* ptr, size_t page_count);
    size_t get_page_size() const { return PAGE_SIZE; }
    size_t get_page_count() const;
    size_t get_total_pages() const;
    size_t get_free_pages() const;
    
    // Memory pool management
    void* allocate_from_pool(size_t size, int pool_id);
    void free_to_pool(void* ptr, int pool_id);
    void create_memory_pool(size_t size, int pool_id);
    void destroy_memory_pool(int pool_id);
    size_t get_pool_usage(int pool_id) const;
    size_t get_pool_size(int pool_id) const;
    bool pool_exists(int pool_id) const;
    
    // Memory statistics
    MemoryStats get_stats() const;
    void print_stats() const;
    void reset_stats();
    void dump_memory_map() const;
    
    // Memory profiling
    void start_profiling();
    void stop_profiling();
    bool is_profiling() const { return profiling_enabled; }
    void dump_profiling_data(const std::string& filename) const;
    std::vector<void*> get_allocation_trace() const;
    size_t get_allocation_count() const;
    size_t get_peak_allocation() const;
    
    // ZRAM compression
    void enable_zram();
    void disable_zram();
    bool is_zram_enabled() const { return zram_enabled; }
    void set_compression_algorithm(const std::string& algorithm);
    std::string get_compression_algorithm() const;
    float get_compression_ratio() const;
    size_t get_zram_usage() const;
    size_t get_zram_total() const;
    
    // Memory tuning
    void set_swappiness(int value);
    int get_swappiness() const;
    void clear_cache();
    void drop_caches();
    void sync_memory();
    void compact_memory();
    
    // ARM Cortex-A76 specific features
    void enable_mmu();
    void disable_mmu();
    bool is_mmu_enabled() const { return mmu_enabled; }
    void invalidate_cache();
    void clean_cache();
    void clean_invalidate_cache();
    void enable_cache();
    void disable_cache();
    bool is_cache_enabled() const { return cache_enabled; }
    
    // Memory limits
    void set_memory_limit(size_t limit);
    size_t get_memory_limit() const;
    bool is_within_limit(size_t size) const;
    
    // Memory protection
    enum Protection {
        PROT_NONE = 0,
        PROT_READ = 1,
        PROT_WRITE = 2,
        PROT_EXEC = 4,
        PROT_READ_WRITE = PROT_READ | PROT_WRITE,
        PROT_ALL = PROT_READ | PROT_WRITE | PROT_EXEC
    };
    
    bool set_protection(void* ptr, size_t size, Protection protection);
    Protection get_protection(void* ptr) const;
    
private:
    struct MemoryBlock {
        void* address;
        size_t size;
        bool is_free;
        bool is_mapped;
        bool is_mmap;
        std::chrono::system_clock::time_point allocation_time;
        int pool_id;
        void* next;
        void* prev;
        size_t alignment;
        Protection protection;
    };
    
    struct MemoryPool {
        void* start_address;
        size_t total_size;
        size_t used_size;
        std::vector<MemoryBlock> blocks;
        mutable std::mutex pool_mutex;
        bool is_zram_enabled;
        std::string name;
    };
    
    std::vector<MemoryBlock> allocations;
    std::map<int, MemoryPool> pools;
    mutable std::mutex global_mutex;
    MemoryStats stats;
    std::atomic<bool> profiling_enabled;
    std::vector<void*> allocation_trace;
    std::atomic<size_t> peak_allocation;
    std::atomic<size_t> total_allocations;
    
    // Page size for ARM64
    static constexpr size_t PAGE_SIZE = 4096;
    static constexpr size_t PAGE_SHIFT = 12;
    static constexpr size_t PAGE_MASK = (PAGE_SIZE - 1);
    
    // Features
    bool paging_enabled = false;
    bool zram_enabled = false;
    bool mmu_enabled = false;
    bool cache_enabled = true;
    bool memory_limit_enabled = false;
    
    size_t memory_limit = 0;
    size_t total_pages = 0;
    size_t free_pages = 0;
    
    // ZRAM settings
    std::string compression_algorithm = "lz4";
    size_t zram_size = 1024 * 1024 * 1024; // 1GB default
    
    // Helper functions
    MemoryBlock* find_free_block(size_t size);
    MemoryBlock* find_block(void* ptr);
    void coalesce_blocks();
    void split_block(MemoryBlock* block, size_t size);
    void merge_adjacent_blocks(MemoryBlock* block);
    void* align_address(void* addr, size_t alignment);
    size_t align_size(size_t size, size_t alignment);
    size_t round_up_to_page(size_t size) const;
    size_t round_down_to_page(size_t size) const;
    
    // ARM specific functions
    void setup_mmu();
    void setup_page_tables();
    void setup_cache();
    uint64_t get_arm_register(const std::string& reg);
    void set_arm_register(const std::string& reg, uint64_t value);
    void invalidate_tlb();
    void dsb(); // Data synchronization barrier
    void isb(); // Instruction synchronization barrier
    
    // Memory pool management
    int get_next_pool_id();
    void* allocate_pool_memory(size_t size);
    void deallocate_pool_memory(void* ptr, size_t size);
    bool is_in_pool(void* ptr, int pool_id) const;
    MemoryBlock* find_block_in_pool(void* ptr, int pool_id);
    
    // ZRAM management
    bool setup_zram_device();
    bool teardown_zram_device();
    bool write_zram_config();
    size_t get_zram_compressed_size() const;
    size_t get_zram_original_size() const;
    
    // System memory info
    size_t get_system_memory_total() const;
    size_t get_system_memory_free() const;
    size_t get_system_memory_available() const;
    size_t get_system_memory_used() const;
};

#endif // MEMORY_MANAGER_HPP
