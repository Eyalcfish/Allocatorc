#include <vector>
#include <cassert>
#include <iostream>
#include <windows.h>
#include <math.h>

typedef struct  memory_grid{
    void* head_page;
    size_t* allocations_head_page;
} memory_grid;

memory_grid initialize_allocatorc() {
    memory_grid allocator;
    allocator.head_page = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    allocator.allocations_head_page = (size_t*) VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    allocator.allocations_head_page[2] = (size_t)allocator.head_page + 16;
    return allocator;
}

void uninitialize_chain(void* head_page) {
    if (head_page == nullptr) {
        return;
    }
    uninitialize_chain(head_page);
    VirtualFree(head_page, 0, MEM_RELEASE);
}

void uninitialize_allocatorc(memory_grid* allocator) {
    uninitialize_chain(allocator->head_page);
    uninitialize_chain(allocator->allocations_head_page);
}

size_t next_break_from_page(size_t* page, int* index) {
    for (int i = *index; i < 512; i++) {
        if (page[i] != 0) {
            *index = i;
            return page[i];
        }
    }
    *index = 512;
    return 4096;
}

void* malloc_from_page(size_t* page, size_t size) {
    void* first_pointer = (size_t*)page[2];
    for (int i = 3; i < 512; i++) {
        if (page[i] == 0) {
            int s = i;
            size_t next = next_break_from_page(page, &s);
            size_t space = next - page[i-1];
            if (space+(size_t)first_pointer*(i == 3) > size) {
                // std::cout<< "Allocated memory at: " << (size_t)first_pointer << " index: " << i << std::endl;
                size_t buffer = page[i-1]*(i-1 != 2);
                page[i] = size+buffer;
                return (void*)((size_t)first_pointer+buffer);
            }
            i = s;
        }
    }
    std::cout << "Out of memory!" << std::endl;
    return nullptr;
}



void* freec_from_page(size_t* page, void* ptr) {
    void* first_pointer = (size_t*)page[2];
    if (first_pointer == ptr) {
        // std::cout << "Freed memory at: " << (size_t)ptr << " index: 3" << std::endl;
        page[3] = 0;
        return ptr;
    }
    for (int i = 3; i < 512; i++) {
        if (page[i]+(size_t)first_pointer > (size_t) ptr) {
            if (page[i] == 0) {
                for (int j = i+1; j < 512;j++) {
                    if (page[j] != 0) {
                        // std::cout << "Freed memory at: " << (size_t)ptr << " index: " << j << std::endl;
                        page[j] = 0;
                        return ptr;
                    }
                }
            }
            // std::cout << "Freed memory at: " << (size_t)ptr << " index: " << i << std::endl;
            page[i] = 0;
            return ptr;
        }
    }
    std::cout << "Pointer not found in allocations!" << std::endl;
    return ptr;
}

void print_page(void* page) {
    size_t* current_page = (size_t*) page;
    for (int i = 0; i < 20; i++) {
        std::cout << "Entry " << i << ": " << current_page[i] << std::endl;
    }
}


// --- YOUR HEADER INCLUDES HERE ---
// #include "allocator.h" 
// (Assuming the struct and function definitions are available)

void print_status(const char* action, int size = 0) {
    if (size > 0) std::cout << "[Action] " << action << " (" << size << " bytes)" << std::endl;
    else std::cout << "[Action] " << action << std::endl;
}

int main() {
    std::cout << "=== STARTING 4KB STRESS TEST ===" << std::endl;

    // 1. Initialize
    memory_grid allocator = initialize_allocatorc();

    // ==========================================
    // TEST 1: The "Swiss Cheese" (Fragmentation)
    // ==========================================
    // We will allocate 50 small blocks, then free every SECOND one.
    // This forces the allocator to handle a Free List with many gaps.
    
    std::cout << "\n--- Test 1: Fragmentation (Swiss Cheese) ---" << std::endl;
    std::vector<int*> ptrs;
    const int NUM_ALLOCS = 50;
    const int CHUNK_SIZE = 32; // Small enough to fit 50 in 4KB easily

    // Alloc 50 blocks
    for (int i = 0; i < NUM_ALLOCS; i++) {
        int* p = (int*)malloc_from_page(allocator.allocations_head_page, CHUNK_SIZE);
        if (!p) {
            std::cerr << "FAILED at index " << i << "!" << std::endl;
            return 1;
        }
        std::cout << "Allocating block " << p << std::endl;
        *p = i; // Write data to ensure we own it
        ptrs.push_back(p);
    }
    std::cout << "Allocated " << NUM_ALLOCS << " blocks of " << CHUNK_SIZE << " bytes." << std::endl;

    // Free every EVEN index (0, 2, 4...) -> Creates holes
    for (int i = 0; i < NUM_ALLOCS; i += 2) {
        freec_from_page(allocator.allocations_head_page, ptrs[i]);
        ptrs[i] = nullptr; 
    }
    std::cout << "Freed 50% of blocks to create holes." << std::endl;

    // RE-ALLOCATE into those holes
    // If your allocator works, it should reuse the holes and NOT ask for new space at the end.
    for (int i = 0; i < NUM_ALLOCS; i += 2) {
        ptrs[i] = (int*)malloc_from_page(allocator.allocations_head_page, CHUNK_SIZE);
        if (!ptrs[i]) {
            std::cerr << "Re-allocation FAILED! Fragmentation logic might be broken." << std::endl;
            return 1;
        }
        *ptrs[i] = 999; // Test write
    }
    std::cout << "Successfully refilled the holes." << std::endl;

    // Clean up everything for the next test
    for (int i = 0; i < NUM_ALLOCS; i++) {
        freec_from_page(allocator.allocations_head_page, ptrs[i]);
    }

    // ==========================================
    // TEST 2: The "Big Squeeze" (Coalescing)
    // ==========================================
    // We freed everything above. The allocator SHOULD have merged all those small 
    // chunks back into one giant free block.
    // If we can allocate a huge 3500 byte chunk now, coalescing works.
    // If this fails, your allocator is leaking memory or not merging neighbors.

    std::cout << "\n--- Test 2: Coalescing (The Big Squeeze) ---" << std::endl;
    
    // 3500 bytes is close to 4096 but leaves room for headers/metadata
    int* big_chunk = (int*)malloc_from_page(allocator.allocations_head_page, 3500); 

    if (big_chunk) {
        std::cout << "SUCCESS: Allocated 3500 bytes. Coalescing is working!" << std::endl;
        // Verify we can write to the end
        // 3500 bytes / 4 bytes per int = 875 ints
        big_chunk[874] = 1234; 
        freec_from_page(allocator.allocations_head_page, big_chunk);
    } else {
        std::cerr << "FAILURE: Could not allocate 3500 bytes after freeing everything." << std::endl;
        std::cerr << "Likely cause: Allocator is not merging adjacent free blocks." << std::endl;
    }


    // ==========================================
    // TEST 3: The "Edge Walk" (Capacity Limit)
    // ==========================================
    // Try to fill the remaining space exactly. 
    // Depending on your overhead, this might hit the limit.
    
    std::cout << "\n--- Test 3: Capacity Limit ---" << std::endl;

    // Let's assume header is ~16 bytes.
    // Try to allocate two large chunks that should JUST fit.
    int* half1 = (int*)malloc_from_page(allocator.allocations_head_page, 2000);
    int* half2 = (int*)malloc_from_page(allocator.allocations_head_page, 2000); // This should FAIL (2000+2000 > 4096)

    if (half1) std::cout << "Allocated first 2000 bytes (Expected)." << std::endl;
    if (!half2) std::cout << "Second 2000 bytes FAILED (Expected - Out of memory)." << std::endl;
    else std::cout << "Second 2000 bytes SUCCEEDED (Surprising! Check your math)." << std::endl;

    if (half1) freec_from_page(allocator.allocations_head_page, half1);
    if (half2) freec_from_page(allocator.allocations_head_page, half2);

    std::cout << "\n--- Test 3.5: The True Limit ---" << std::endl;

    // 2030 + 2030 = 4060 bytes payload.
    // 4096 - 4060 = 36 bytes left for headers.
    // If you have 2 headers, and each is > 18 bytes, this MUST fail.
    // If your header is 16 bytes, this might JUST fit (32 bytes total overhead).
    
    int* p1 = (int*)malloc_from_page(allocator.allocations_head_page, 2030);
    int* p2 = (int*)malloc_from_page(allocator.allocations_head_page, 2030);

    if (p1 && p2) {
        std::cout << "Both fit! Your headers are tiny (<= 18 bytes)." << std::endl;
        
        // Let's break it for real.
        // 4096 total. 
        int* p3 = (int*)malloc_from_page(allocator.allocations_head_page, 100); 
        if (!p3) std::cout << "Final allocation failed as expected (OOM handled)." << std::endl;
        else std::cout << "ERROR: We somehow allocated more than 4096 bytes? Did you implement expansion?" << std::endl;
        
    } else if (p1 && !p2) {
        std::cout << "Second allocation failed as expected. OOM Logic works." << std::endl;
    }

    if (p1) freec_from_page(allocator.allocations_head_page, p1);
    if (p2) freec_from_page(allocator.allocations_head_page, p2);

    // Final Cleanup
    uninitialize_allocatorc(&allocator);
    std::cout << "\n=== TEST COMPLETE ===" << std::endl;

    return 0;
}