
# Memory Allocator Architecture Guide

## 1.1 Why Implement a Memory Allocator?

### The Problem with malloc/free
- **Fragmentation**: Internal and external fragmentation explained with diagrams
- **Performance**: Real-time systems need predictable O(1) operations
- **Determinism**: Game engines and medical devices cannot tolerate unpredictable pauses
- **Memory constraints**: 2KB RAM microcontrollers cannot run glibc malloc

### When to Use Custom Allocators

| Use Case | Recommended Allocator | Reason |
|----------|----------------------|--------|
| Game engine (1000s of bullets) | Pool allocator | Same size objects, no fragmentation |
| Network packet processing | Stack allocator | LIFO pattern, extremely fast (few ns) |
| Database page cache | Buddy system | Power-of-two sizes, good coalescing |
| Operating system kernel | Slab allocator | Frequent same-size allocations |
| Real-time audio | Block allocator | O(1) worst-case time |

## 1.2 Memory Layout and Virtual Memory

### Process Memory Map
