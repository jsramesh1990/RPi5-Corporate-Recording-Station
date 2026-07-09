/**
 * @file test_memory.cpp
 * @brief Memory Manager Unit Tests
 * 
 * Tests the MemoryManager class including:
 * - Basic allocation/deallocation
 * - Aligned allocation
 * - Memory pools
 * - ZRAM compression
 * - Paging
 * - Memory protection
 * - Statistics tracking
 * - Error handling
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <vector>
#include <random>

#include "../../src/core/MemoryManager.hpp"

// ============================================================
// TEST FIXTURE
// ============================================================

class MemoryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mem_mgr = &MemoryManager::getInstance();
        mem_mgr->reset_stats();
        mem_mgr->enable_paging();
        mem_mgr->enable_zram();
    }
    
    void TearDown() override {
        // Clean up any remaining allocations
        // MemoryManager is a singleton, so we just reset stats
        mem_mgr->reset_stats();
    }
    
    MemoryManager* mem_mgr;
};

// ============================================================
// TEST: Basic Allocation
// ============================================================

TEST_F(MemoryManagerTest, BasicAllocation) {
    // Allocate small block
    size_t size = 1024;
    void* ptr = mem_mgr->allocate(size);
    ASSERT_NE(ptr, nullptr);
    
    // Write to memory
    memset(ptr, 0xAA, size);
    
    // Verify memory content
    uint8_t* bytes = static_cast<uint8_t*>(ptr);
    for (size_t i = 0; i < size; i++) {
        EXPECT_EQ(bytes[i], 0xAA);
    }
    
    // Check statistics
    auto stats = mem_mgr->get_stats();
    EXPECT_EQ(stats.allocation_count, 1);
    EXPECT_EQ(stats.current_usage, size);
    
    // Deallocate
    mem_mgr->deallocate(ptr);
    stats = mem_mgr->get_stats();
    EXPECT_EQ(stats.current_usage, 0);
    EXPECT_EQ(stats.free_count, 1);
}

TEST_F(MemoryManagerTest, MultipleAllocations) {
    const int NUM_ALLOCATIONS = 100;
    const size_t SIZE = 4096;
    std::vector<void*> ptrs;
    
    // Allocate multiple blocks
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        void* ptr = mem_mgr->allocate(SIZE);
        ASSERT_NE(ptr, nullptr);
        memset(ptr, i & 0xFF, SIZE);
        ptrs.push_back(ptr);
    }
    
    // Verify statistics
    auto stats = mem_mgr->get_stats();
    EXPECT_EQ(stats.allocation_count, NUM_ALLOCATIONS);
    EXPECT_EQ(stats.current_usage, NUM_ALLOCATIONS * SIZE);
    
    // Deallocate in reverse order
    for (int i = NUM_ALLOCATIONS - 1; i >= 0; i--) {
        mem_mgr->deallocate(ptrs[i]);
    }
    
    stats = mem_mgr->get_stats();
    EXPECT_EQ(stats.current_usage, 0);
    EXPECT_EQ(stats.free_count, NUM_ALLOCATIONS);
}

TEST_F(MemoryManagerTest, ZeroSizeAllocation) {
    void* ptr = mem_mgr->allocate(0);
    EXPECT_EQ(ptr, nullptr);
    
    auto stats = mem_mgr->get_stats();
    EXPECT_EQ(stats.allocation_count, 0);
}

TEST_F(MemoryManagerTest, NullDeallocation) {
    // Deallocating nullptr should not crash
    mem_mgr->deallocate(nullptr);
    
    auto stats = mem_mgr->get_stats();
    EXPECT_EQ(stats.free_count, 0);
}

// ============================================================
// TEST: Aligned Allocation
// ============================================================

TEST_F(MemoryManagerTest, AlignedAllocation) {
    const size_t ALIGNMENT = 4096;
    const size_t SIZE = 1024;
    
    void* ptr = mem_mgr->allocate_aligned(SIZE, ALIGNMENT);
    ASSERT_NE(ptr, nullptr);
    
    // Check alignment
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % ALIGNMENT, 0);
    
    // Write to memory
    memset(ptr, 0xBB, SIZE);
    
    // Verify
    uint8_t* bytes = static_cast<uint8_t*>(ptr);
    for (size_t i = 0; i < SIZE; i++) {
        EXPECT_EQ(bytes[i], 0xBB);
    }
    
    mem_mgr->deallocate(ptr);
}

TEST_F(MemoryManagerTest, VariousAlignments) {
    std::vector<size_t> alignments = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    
    for (size_t align : alignments) {
        void* ptr = mem_mgr->allocate_aligned(1024, align);
        ASSERT_NE(ptr, nullptr);
        
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        EXPECT_EQ(addr % align, 0);
        
        mem_mgr->deallocate(ptr);
    }
}

TEST_F(MemoryManagerTest, InvalidAlignment) {
    // Alignment must be power of 2
    void* ptr = mem_mgr->allocate_aligned(1024, 3);
    EXPECT_EQ(ptr, nullptr);
    
    ptr = mem_mgr->allocate_aligned(1024, 0);
    EXPECT_EQ(ptr, nullptr);
}

// ============================================================
// TEST: Reallocation
// ============================================================

TEST_F(MemoryManagerTest, Reallocation) {
    // Allocate initial block
    size_t initial_size = 1024;
    void* ptr = mem_mgr->allocate(initial_size);
    ASSERT_NE(ptr, nullptr);
    
    // Write pattern
    memset(ptr, 0xCC, initial_size);
    
    // Reallocate to larger size
    size_t new_size = 2048;
    void* new_ptr = mem_mgr->reallocate(ptr, new_size);
    ASSERT_NE(new_ptr, nullptr);
    
    // Verify data preserved (for first initial_size bytes)
    uint8_t* bytes = static_cast<uint8_t*>(new_ptr);
    for (size_t i = 0; i < initial_size; i++) {
        EXPECT_EQ(bytes[i], 0xCC);
    }
    
    // Write to new memory
    memset(new_ptr, 0xDD, new_size);
    
    // Verify
    for (size_t i = 0; i < new_size; i++) {
        EXPECT_EQ(bytes[i], 0xDD);
    }
    
    mem_mgr->deallocate(new_ptr);
}

TEST_F(MemoryManagerTest, ReallocateNull) {
    void* ptr = mem_mgr->reallocate(nullptr, 1024);
    ASSERT_NE(ptr, nullptr);
    mem_mgr->deallocate(ptr);
}

TEST_F(MemoryManagerTest, ReallocateZero) {
    void* ptr = mem_mgr->allocate(1024);
    ASSERT_NE(ptr, nullptr);
    
    void* new_ptr = mem_mgr->reallocate(ptr, 0);
    EXPECT_EQ(new_ptr, nullptr);
}

// ============================================================
// TEST: Memory Pools
// ============================================================

TEST_F(MemoryManagerTest, MemoryPoolCreation) {
    int pool_id = 1;
    size_t pool_size = 1024 * 1024; // 1 MB
    
    mem_mgr->create_memory_pool(pool_size, pool_id);
    EXPECT_TRUE(mem_mgr->pool_exists(pool_id));
    EXPECT_EQ(mem_mgr->get_pool_size(pool_id), pool_size);
    EXPECT_EQ(mem_mgr->get_pool_usage(pool_id), 0);
    
    // Allocate from pool
    void* ptr = mem_mgr->allocate_from_pool(4096, pool_id);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(mem_mgr->get_pool_usage(pool_id), 4096);
    
    // Write to pool memory
    memset(ptr, 0xEE, 4096);
    
    // Free to pool
    mem_mgr->free_to_pool(ptr, pool_id);
    EXPECT_EQ(mem_mgr->get_pool_usage(pool_id), 0);
    
    // Clean up
    mem_mgr->destroy_memory_pool(pool_id);
    EXPECT_FALSE(mem_mgr->pool_exists(pool_id));
}

TEST_F(MemoryManagerTest, MultiplePools) {
    // Create multiple pools
    for (int i = 1; i <= 3; i++) {
        mem_mgr->create_memory_pool(1024 * 1024, i);
        EXPECT_TRUE(mem_mgr->pool_exists(i));
    }
    
    // Allocate from different pools
    void* ptr1 = mem_mgr->allocate_from_pool(4096, 1);
    void* ptr2 = mem_mgr->allocate_from_pool(8192, 2);
    void* ptr3 = mem_mgr->allocate_from_pool(16384, 3);
    
    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_NE(ptr3, nullptr);
    
    // Free all
    mem_mgr->free_to_pool(ptr1, 1);
    mem_mgr->free_to_pool(ptr2, 2);
    mem_mgr->free_to_pool(ptr3, 3);
    
    // Clean up
    for (int i = 1; i <= 3; i++) {
        mem_mgr->destroy_memory_pool(i);
        EXPECT_FALSE(mem_mgr->pool_exists(i));
    }
}

TEST_F(MemoryManagerTest, PoolExhaustion) {
    int pool_id = 1;
    size_t pool_size = 1024 * 1024; // 1 MB
    
    mem_mgr->create_memory_pool(pool_size, pool_id);
    
    // Allocate until pool is exhausted
    size_t allocated = 0;
    std::vector<void*> ptrs;
    
    while (allocated < pool_size) {
        size_t chunk = 4096;
        if (allocated + chunk > pool_size) {
            chunk = pool_size - allocated;
        }
        void* ptr = mem_mgr->allocate_from_pool(chunk, pool_id);
        if (ptr == nullptr) {
            break;
        }
        ptrs.push_back(ptr);
        allocated += chunk;
    }
    
    // Next allocation should fail
    void* ptr = mem_mgr->allocate_from_pool(4096, pool_id);
    EXPECT_EQ(ptr, nullptr);
    
    // Free some allocations
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        mem_mgr->free_to_pool(ptrs[i], pool_id);
    }
    
    // Should be able to allocate again
    ptr = mem_mgr->allocate_from_pool(4096, pool_id);
    EXPECT_NE(ptr, nullptr);
    mem_mgr->free_to_pool(ptr, pool_id);
    
    // Clean up remaining
    for (void* p : ptrs) {
        mem_mgr->free_to_pool(p, pool_id);
    }
    
    mem_mgr->destroy_memory_pool(pool_id);
}

// ============================================================
// TEST: ZRAM Compression
// ============================================================

TEST_F(MemoryManagerTest, ZRAMCompression) {
    if (!mem_mgr->is_zram_enabled()) {
        GTEST_SKIP() << "ZRAM not available";
    }
    
    // Allocate compressible data (repeating pattern)
    const size_t SIZE = 10 * 1024 * 1024; // 10 MB
    void* ptr = mem_mgr->allocate(SIZE);
    ASSERT_NE(ptr, nullptr);
    
    // Fill with compressible pattern
    memset(ptr, 0xAA, SIZE);
    
    auto stats = mem_mgr->get_stats();
    float compression_ratio = mem_mgr->get_compression_ratio();
    
    // Compression ratio should be > 1 for compressible data
    EXPECT_GT(compression_ratio, 1.0f);
    
    // Deallocate
    mem_mgr->deallocate(ptr);
}

TEST_F(MemoryManagerTest, ZRAMIncompressibleData) {
    if (!mem_mgr->is_zram_enabled()) {
        GTEST_SKIP() << "ZRAM not available";
    }
    
    // Allocate incompressible data (random)
    const size_t SIZE = 10 * 1024 * 1024; // 10 MB
    void* ptr = mem_mgr->allocate(SIZE);
    ASSERT_NE(ptr, nullptr);
    
    // Fill with random data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 255);
    
    uint8_t* bytes = static_cast<uint8_t*>(ptr);
    for (size_t i = 0; i < SIZE; i++) {
        bytes[i] = dist(gen);
    }
    
    float compression_ratio = mem_mgr->get_compression_ratio();
    
    // Compression ratio should be lower for random data
    EXPECT_LT(compression_ratio, 3.0f);
    
    mem_mgr->deallocate(ptr);
}

// ============================================================
// TEST: Memory Protection
// ============================================================

TEST_F(MemoryManagerTest, MemoryProtection) {
    const size_t SIZE = 4096;
    void* ptr = mem_mgr->allocate(SIZE);
    ASSERT_NE(ptr, nullptr);
    
    // Set protection to read-only
    EXPECT_TRUE(mem_mgr->set_protection(ptr, SIZE, MemoryManager::PROT_READ));
    
    // Verify protection
    auto protection = mem_mgr->get_protection(ptr);
    EXPECT_EQ(protection, MemoryManager::PROT_READ);
    
    // Try to write to read-only memory (should cause segfault in production)
    // In test, we just verify protection is set
    // mem_mgr->deallocate(ptr);
}

TEST_F(MemoryManagerTest, MemoryProtectionFlags) {
    const size_t SIZE = 4096;
    void* ptr = mem_mgr->allocate(SIZE);
    ASSERT_NE(ptr, nullptr);
    
    // Test different protection combinations
    std::vector<MemoryManager::Protection> protections = {
        MemoryManager::PROT_NONE,
        MemoryManager::PROT_READ,
        MemoryManager::PROT_WRITE,
        MemoryManager::PROT_EXEC,
        MemoryManager::PROT_READ_WRITE,
        MemoryManager::PROT_ALL
    };
    
    for (auto prot : protections) {
        EXPECT_TRUE(mem_mgr->set_protection(ptr, SIZE, prot));
        EXPECT_EQ(mem_mgr->get_protection(ptr), prot);
    }
    
    mem_mgr->deallocate(ptr);
}

// ============================================================
// TEST: Statistics
// ============================================================

TEST_F(MemoryManagerTest, StatisticsTracking) {
    // Reset stats
    mem_mgr->reset_stats();
    auto stats = mem_mgr->get_stats();
    EXPECT_EQ(stats.allocation_count, 0);
    EXPECT_EQ(stats.current_usage, 0);
    EXPECT_EQ(stats.peak_usage, 0);
    
    // Allocate memory
    const size_t SIZE1 = 1024 * 1024;
    void* ptr1 = mem_mgr->allocate(SIZE1);
    stats = mem_mgr->get_stats();
    EXPECT_EQ(stats.allocation_count, 1);
    EXPECT_EQ(stats.current_usage, SIZE1);
    EXPECT_EQ(stats.peak_usage, SIZE1);
    
    // Allocate more memory
    const size_t SIZE2 = 2 * 1024 * 1024;
    void* ptr2 = mem_mgr->allocate(SIZE2);
    stats = mem_mgr->get_stats();
    EXPECT_EQ(stats.allocation_count, 2);
    EXPECT_EQ(stats.current_usage, SIZE1 + SIZE2);
    EXPECT_EQ(stats.peak_usage, SIZE1 + SIZE2);
    
    // Deallocate
    mem_mgr->deallocate(ptr1);
    stats = mem_mgr->get_stats();
    EXPECT_EQ(stats.current_usage, SIZE2);
    EXPECT_EQ(stats.free_count, 1);
    
    // Deallocate remaining
    mem_mgr->deallocate(ptr2);
    stats = mem_mgr->get_stats();
    EXPECT_EQ(stats.current_usage, 0);
    EXPECT_EQ(stats.free_count, 2);
}

TEST_F(MemoryManagerTest, Profiling) {
    // Start profiling
    mem_mgr->start_profiling();
    EXPECT_TRUE(mem_mgr->is_profiling());
    
    // Do some allocations
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; i++) {
        ptrs.push_back(mem_mgr->allocate(1024));
    }
    
    // Dump profiling data
    std::string dump_file = "/tmp/memory_profile.txt";
    mem_mgr->dump_profiling_data(dump_file);
    
    // Verify dump file exists
    std::ifstream file(dump_file);
    EXPECT_TRUE(file.good());
    file.close();
    
    // Clean up
    for (void* ptr : ptrs) {
        mem_mgr->deallocate(ptr);
    }
    
    mem_mgr->stop_profiling();
    EXPECT_FALSE(mem_mgr->is_profiling());
}

// ============================================================
// TEST: Edge Cases
// ============================================================

TEST_F(MemoryManagerTest, LargeAllocation) {
    // Try to allocate large block (near memory limit)
    size_t large_size = 100 * 1024 * 1024; // 100 MB
    void* ptr = mem_mgr->allocate(large_size);
    
    // Might fail on systems with low memory, but shouldn't crash
    if (ptr != nullptr) {
        mem_mgr->deallocate(ptr);
    }
}

TEST_F(MemoryManagerTest, Fragmentation) {
    // Allocate and free in pattern to create fragmentation
    std::vector<void*> ptrs;
    
    // Allocate interleaved sizes
    for (int i = 0; i < 100; i++) {
        size_t size = (i % 5 + 1) * 1024;
        void* ptr = mem_mgr->allocate(size);
        if (ptr) {
            ptrs.push_back(ptr);
        }
    }
    
    // Free every other allocation
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        mem_mgr->deallocate(ptrs[i]);
        ptrs[i] = nullptr;
    }
    
    // Should still be able to allocate
    void* new_ptr = mem_mgr->allocate(4096);
    EXPECT_NE(new_ptr, nullptr);
    mem_mgr->deallocate(new_ptr);
    
    // Clean up
    for (void* ptr : ptrs) {
        if (ptr) {
            mem_mgr->deallocate(ptr);
        }
    }
}

// ============================================================
// MAIN
// ============================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
