#include "MemoryManager.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <chrono>
#include <thread>

// ARM-specific includes (only on ARM)
#ifdef __aarch64__
#include <asm/hwcap.h>
#include <sys/auxv.h>
#endif

MemoryManager::MemoryManager() 
    : profiling_enabled(false), peak_allocation(0), total_allocations(0) {
    // Initialize page tracking
    total_pages = get_system_memory_total() / PAGE_SIZE;
    free_pages = total_pages;
    
    // Setup MMU if available
    #ifdef __aarch64__
    setup_mmu();
    #endif
    
    // Enable ZRAM by default
    enable_zram();
    
    // Setup cache
    setup_cache();
    
    // Initialize stats
    reset_stats();
}

MemoryManager::~MemoryManager() {
    // Clean up all allocations
    for (auto& block : allocations) {
        if (!block.is_free) {
            if (block.is_mmap) {
                munmap(block.address, block.size);
            } else {
                free(block.address);
            }
        }
    }
    allocations.clear();
    
    // Clean up pools
    for (auto& [id, pool] : pools) {
        munmap(pool.start_address, pool.total_size);
    }
    pools.clear();
    
    // Disable ZRAM
    if (zram_enabled) {
        teardown_zram_device();
    }
}

void* MemoryManager::allocate(size_t size) {
    if (size == 0) return nullptr;
    
    std::lock_guard<std::mutex> lock(global_mutex);
    
    // Check memory limit
    if (memory_limit_enabled && stats.current_usage + size > memory_limit) {
        return nullptr;
    }
    
    // Try to find a free block
    MemoryBlock* block = find_free_block(size);
    
    if (block != nullptr) {
        // Found a free block, split if too large
        if (block->size > size + sizeof(MemoryBlock)) {
            split_block(block, size);
        }
        
        // Mark as used
        block->is_free = false;
        block->allocation_time = std::chrono::system_clock::now();
        
        // Update stats
        stats.current_usage += block->size;
        stats.allocation_count++;
        if (stats.current_usage > stats.peak_usage) {
            stats.peak_usage = stats.current_usage;
        }
        
        // Track allocation
        if (profiling_enabled) {
            allocation_trace.push_back(block->address);
        }
        
        return block->address;
    }
    
    // No free block, allocate new memory
    void* ptr = malloc(size);
    if (ptr == nullptr) {
        return nullptr;
    }
    
    // Create new block
    MemoryBlock new_block;
    new_block.address = ptr;
    new_block.size = size;
    new_block.is_free = false;
    new_block.is_mapped = false;
    new_block.is_mmap = false;
    new_block.allocation_time = std::chrono::system_clock::now();
    new_block.pool_id = -1;
    new_block.next = nullptr;
    new_block.prev = nullptr;
    new_block.alignment = 0;
    new_block.protection = PROT_READ_WRITE;
    
    allocations.push_back(new_block);
    
    // Update stats
    stats.current_usage += size;
    stats.total_allocated += size;
    stats.allocation_count++;
    if (stats.current_usage > stats.peak_usage) {
        stats.peak_usage = stats.current_usage;
    }
    
    // Track allocation
    if (profiling_enabled) {
        allocation_trace.push_back(ptr);
    }
    
    return ptr;
}

void* MemoryManager::allocate_aligned(size_t size, size_t alignment) {
    if (size == 0 || alignment == 0) return nullptr;
    
    // Ensure alignment is power of 2
    if ((alignment & (alignment - 1)) != 0) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(global_mutex);
    
    // Check memory limit
    if (memory_limit_enabled && stats.current_usage + size > memory_limit) {
        return nullptr;
    }
    
    // Allocate aligned memory
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return nullptr;
    }
    
    // Create new block
    MemoryBlock new_block;
    new_block.address = ptr;
    new_block.size = size;
    new_block.is_free = false;
    new_block.is_mapped = false;
    new_block.is_mmap = false;
    new_block.allocation_time = std::chrono::system_clock::now();
    new_block.pool_id = -1;
    new_block.next = nullptr;
    new_block.prev = nullptr;
    new_block.alignment = alignment;
    new_block.protection = PROT_READ_WRITE;
    
    allocations.push_back(new_block);
    
    // Update stats
    stats.current_usage += size;
    stats.total_allocated += size;
    stats.allocation_count++;
    if (stats.current_usage > stats.peak_usage) {
        stats.peak_usage = stats.current_usage;
    }
    
    // Track allocation
    if (profiling_enabled) {
        allocation_trace.push_back(ptr);
    }
    
    return ptr;
}

void MemoryManager::deallocate(void* ptr) {
    if (ptr == nullptr) return;
    
    std::lock_guard<std::mutex> lock(global_mutex);
    
    // Find the block
    MemoryBlock* block = find_block(ptr);
    if (block == nullptr) {
        // Not found, try free directly
        free(ptr);
        return;
    }
    
    // Check if in pool
    if (block->pool_id >= 0) {
        free_to_pool(ptr, block->pool_id);
        return;
    }
    
    // Mark as free
    block->is_free = true;
    block->allocation_time = std::chrono::system_clock::time_point();
    
    // Update stats
    stats.current_usage -= block->size;
    stats.total_freed += block->size;
    stats.free_count++;
    
    // Coalesce adjacent free blocks
    coalesce_blocks();
    
    // Deallocate if using mmap
    if (block->is_mmap) {
        munmap(block->address, block->size);
        block->address = nullptr;
        block->size = 0;
    }
}

void* MemoryManager::reallocate(void* ptr, size_t new_size) {
    if (ptr == nullptr) {
        return allocate(new_size);
    }
    
    if (new_size == 0) {
        deallocate(ptr);
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(global_mutex);
    
    MemoryBlock* block = find_block(ptr);
    if (block == nullptr) {
        return realloc(ptr, new_size);
    }
    
    // Check memory limit
    if (memory_limit_enabled && stats.current_usage - block->size + new_size > memory_limit) {
        return nullptr;
    }
    
    // Try to resize in place
    if (block->size >= new_size) {
        // Shrink
        block->size = new_size;
        stats.current_usage -= block->size - new_size;
        return ptr;
    }
    
    // Need to allocate new memory
    void* new_ptr = allocate(new_size);
    if (new_ptr == nullptr) {
        return nullptr;
    }
    
    // Copy data
    memcpy(new_ptr, ptr, block->size);
    
    // Free old block
    deallocate(ptr);
    
    return new_ptr;
}

void* MemoryManager::map_file(const std::string& file_path, size_t size) {
    int fd = open(file_path.c_str(), O_RDWR);
    if (fd < 0) {
        return nullptr;
    }
    
    // Get file size if not specified
    if (size == 0) {
        struct stat st;
        if (fstat(fd, &st) != 0) {
            close(fd);
            return nullptr;
        }
        size = st.st_size;
    }
    
    // Map file
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    
    if (ptr == MAP_FAILED) {
        return nullptr;
    }
    
    // Create new block
    MemoryBlock new_block;
    new_block.address = ptr;
    new_block.size = size;
    new_block.is_free = false;
    new_block.is_mapped = true;
    new_block.is_mmap = true;
    new_block.allocation_time = std::chrono::system_clock::now();
    new_block.pool_id = -1;
    new_block.next = nullptr;
    new_block.prev = nullptr;
    new_block.alignment = PAGE_SIZE;
    new_block.protection = PROT_READ_WRITE;
    
    allocations.push_back(new_block);
    
    // Update stats
    stats.current_usage += size;
    stats.mmap_usage += size;
    stats.allocation_count++;
    if (stats.current_usage > stats.peak_usage) {
        stats.peak_usage = stats.current_usage;
    }
    
    return ptr;
}

void MemoryManager::unmap_file(void* ptr, size_t size) {
    if (ptr == nullptr) return;
    
    std::lock_guard<std::mutex> lock(global_mutex);
    
    // Find the block
    MemoryBlock* block = find_block(ptr);
    if (block == nullptr || !block->is_mmap) {
        return;
    }
    
    // Unmap
    if (munmap(ptr, size) == 0) {
        block->is_free = true;
        block->address = nullptr;
        block->size = 0;
        
        stats.current_usage -= size;
        stats.mmap_usage -= size;
        stats.total_freed += size;
    }
}

void* MemoryManager::page_in(void* virtual_address) {
    if (!paging_enabled) return nullptr;
    
    // This would be implemented with actual page fault handling
    // For now, just return the address
    return virtual_address;
}

void MemoryManager::page_out(void* virtual_address) {
    if (!paging_enabled) return;
    
    // This would be implemented with actual page fault handling
    // For now, do nothing
}

void* MemoryManager::allocate_pages(size_t page_count) {
    size_t size = page_count * PAGE_SIZE;
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, 
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (ptr == MAP_FAILED) {
        return nullptr;
    }
    
    // Create block
    MemoryBlock new_block;
    new_block.address = ptr;
    new_block.size = size;
    new_block.is_free = false;
    new_block.is_mapped = true;
    new_block.is_mmap = true;
    new_block.allocation_time = std::chrono::system_clock::now();
    new_block.pool_id = -1;
    new_block.next = nullptr;
    new_block.prev = nullptr;
    new_block.alignment = PAGE_SIZE;
    new_block.protection = PROT_READ_WRITE;
    
    allocations.push_back(new_block);
    
    // Update stats
    stats.current_usage += size;
    stats.allocation_count++;
    if (stats.current_usage > stats.peak_usage) {
        stats.peak_usage = stats.current_usage;
    }
    
    free_pages -= page_count;
    
    return ptr;
}

void MemoryManager::free_pages(void* ptr, size_t page_count) {
    if (ptr == nullptr) return;
    
    unmap_file(ptr, page_count * PAGE_SIZE);
    free_pages += page_count;
}

MemoryStats MemoryManager::get_stats() const {
    std::lock_guard<std::mutex> lock(global_mutex);
    return stats;
}

void MemoryManager::print_stats() const {
    auto stats = get_stats();
    
    std::cout << "\n=== Memory Manager Statistics ===" << std::endl;
    std::cout << "Total Allocated: " << stats.total_allocated / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Total Freed:     " << stats.total_freed / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Current Usage:   " << stats.current_usage / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Peak Usage:      " << stats.peak_usage / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Allocations:     " << stats.allocation_count << std::endl;
    std::cout << "Frees:           " << stats.free_count << std::endl;
    std::cout << "Fragmentation:   " << stats.fragmentation_percentage << "%" << std::endl;
    std::cout << "MMAP Usage:      " << stats.mmap_usage / (1024 * 1024) << " MB" << std::endl;
    
    if (zram_enabled) {
        std::cout << "ZRAM Enabled:    Yes" << std::endl;
        std::cout << "Compression Ratio: " << get_compression_ratio() << "x" << std::endl;
    }
    
    std::cout << "===================================" << std::endl;
}

void MemoryManager::enable_zram() {
    if (zram_enabled) return;
    
    if (setup_zram_device()) {
        zram_enabled = true;
        std::cout << "ZRAM enabled with " << compression_algorithm << " compression" << std::endl;
    }
}

void MemoryManager::disable_zram() {
    if (!zram_enabled) return;
    
    if (teardown_zram_device()) {
        zram_enabled = false;
        std::cout << "ZRAM disabled" << std::endl;
    }
}

bool MemoryManager::setup_zram_device() {
    // This would be implemented with actual ZRAM device setup
    // For now, simulate success
    return true;
}

bool MemoryManager::teardown_zram_device() {
    // This would be implemented with actual ZRAM device teardown
    // For now, simulate success
    return true;
}

void MemoryManager::enable_mmu() {
    #ifdef __aarch64__
    mmu_enabled = true;
    setup_mmu();
    #endif
}

void MemoryManager::disable_mmu() {
    #ifdef __aarch64__
    mmu_enabled = false;
    #endif
}

void MemoryManager::setup_mmu() {
    #ifdef __aarch64__
    // This would set up the ARM MMU
    // For now, just mark as enabled
    #endif
}

void MemoryManager::setup_cache() {
    #ifdef __aarch64__
    // Enable caches
    cache_enabled = true;
    #endif
}

void MemoryManager::invalidate_cache() {
    #ifdef __aarch64__
    // Invalidate all caches
    __asm__ volatile("dsb sy");
    __asm__ volatile("isb");
    #endif
}

MemoryBlock* MemoryManager::find_free_block(size_t size) {
    // First-fit algorithm
    for (auto& block : allocations) {
        if (block.is_free && block.size >= size && !block.is_mmap) {
            return &block;
        }
    }
    return nullptr;
}

MemoryBlock* MemoryManager::find_block(void* ptr) {
    for (auto& block : allocations) {
        if (block.address == ptr && !block.is_free) {
            return &block;
        }
    }
    return nullptr;
}

void MemoryManager::coalesce_blocks() {
    // Implementation would merge adjacent free blocks
    // Simplified version
    for (size_t i = 0; i < allocations.size() - 1; i++) {
        if (allocations[i].is_free && allocations[i + 1].is_free) {
            // Merge blocks
            allocations[i].size += allocations[i + 1].size;
            allocations[i].next = allocations[i + 1].next;
            allocations.erase(allocations.begin() + i + 1);
            i--;
        }
    }
}

void MemoryManager::split_block(MemoryBlock* block, size_t size) {
    if (block->size <= size + sizeof(MemoryBlock)) {
        return;
    }
    
    // Create new block
    MemoryBlock new_block;
    new_block.address = (char*)block->address + size;
    new_block.size = block->size - size - sizeof(MemoryBlock);
    new_block.is_free = true;
    new_block.is_mapped = false;
    new_block.is_mmap = false;
    new_block.pool_id = -1;
    new_block.next = block->next;
    new_block.prev = block;
    new_block.alignment = 0;
    new_block.protection = PROT_READ_WRITE;
    
    block->size = size;
    block->next = (void*)&new_block;
}

void MemoryManager::reset_stats() {
    stats = MemoryStats{};
}

void MemoryManager::start_profiling() {
    profiling_enabled = true;
    allocation_trace.clear();
    total_allocations = 0;
}

void MemoryManager::stop_profiling() {
    profiling_enabled = false;
}

void MemoryManager::dump_profiling_data(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out.is_open()) return;
    
    out << "Memory Allocation Profile\n";
    out << "========================\n";
    out << "Total Allocations: " << allocation_trace.size() << "\n";
    out << "Peak Memory Usage: " << stats.peak_usage / (1024 * 1024) << " MB\n";
    out << "\nAllocation Trace:\n";
    
    for (size_t i = 0; i < allocation_trace.size(); i++) {
        out << i << ": " << allocation_trace[i] << "\n";
    }
    
    out.close();
}

size_t MemoryManager::get_system_memory_total() const {
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

size_t MemoryManager::get_system_memory_free() const {
    long pages = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

void MemoryManager::create_memory_pool(size_t size, int pool_id) {
    std::lock_guard<std::mutex> lock(global_mutex);
    
    // Check if pool already exists
    if (pools.find(pool_id) != pools.end()) {
        return;
    }
    
    // Allocate pool memory
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        return;
    }
    
    MemoryPool pool;
    pool.start_address = ptr;
    pool.total_size = size;
    pool.used_size = 0;
    pool.is_zram_enabled = zram_enabled;
    pool.name = "Pool-" + std::to_string(pool_id);
    
    pools[pool_id] = pool;
}

void MemoryManager::destroy_memory_pool(int pool_id) {
    std::lock_guard<std::mutex> lock(global_mutex);
    
    auto it = pools.find(pool_id);
    if (it == pools.end()) {
        return;
    }
    
    // Unmap pool memory
    munmap(it->second.start_address, it->second.total_size);
    pools.erase(it);
}

void* MemoryManager::allocate_from_pool(size_t size, int pool_id) {
    std::lock_guard<std::mutex> lock(global_mutex);
    
    auto it = pools.find(pool_id);
    if (it == pools.end()) {
        return nullptr;
    }
    
    auto& pool = it->second;
    std::lock_guard<std::mutex> pool_lock(pool.pool_mutex);
    
    // Check if pool has enough space
    if (pool.used_size + size > pool.total_size) {
        return nullptr;
    }
    
    // Find free block in pool
    void* ptr = (char*)pool.start_address + pool.used_size;
    pool.used_size += size;
    
    // Create block
    MemoryBlock block;
    block.address = ptr;
    block.size = size;
    block.is_free = false;
    block.is_mapped = true;
    block.is_mmap = false;
    block.allocation_time = std::chrono::system_clock::now();
    block.pool_id = pool_id;
    block.next = nullptr;
    block.prev = nullptr;
    block.alignment = 0;
    block.protection = PROT_READ_WRITE;
    
    pool.blocks.push_back(block);
    
    return ptr;
}

void MemoryManager::free_to_pool(void* ptr, int pool_id) {
    std::lock_guard<std::mutex> lock(global_mutex);
    
    auto it = pools.find(pool_id);
    if (it == pools.end()) {
        return;
    }
    
    auto& pool = it->second;
    std::lock_guard<std::mutex> pool_lock(pool.pool_mutex);
    
    // Find block in pool
    for (auto& block : pool.blocks) {
        if (block.address == ptr && !block.is_free) {
            block.is_free = true;
            pool.used_size -= block.size;
            break;
        }
    }
}
