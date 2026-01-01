#include <vector>
#include <cassert>
#include <iostream>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <math.h>

typedef struct  memory_grid{
    void* head_page;
    size_t* allocations_head_page;
} memory_grid;

memory_grid initialize_allocatorc() {
    memory_grid allocator;
    #ifdef WIN32
    allocator.head_page = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    allocator.allocations_head_page = (size_t*) VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    #else
    allocator.head_page = mmap(NULL,4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    allocator.allocations_head_page = (size_t*)mmap(NULL,4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    #endif
    allocator.allocations_head_page[2] = (size_t)allocator.head_page + 16;
    return allocator;
}

void uninitialize_chain(void* head_page) {
    if (head_page == nullptr) {
        return;
    }
    uninitialize_chain((void*) ((size_t*)head_page)[1]);
    #ifdef WIN32
    VirtualFree(head_page, 0, MEM_RELEASE);
    #else
    munmap(head_page,4096);
    #endif
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
    // std::cout << "Pointer not found in allocations!" << std::endl;
    return ptr;
}

void print_page(void* page) {
    size_t* current_page = (size_t*) page;
    for (int i = 0; i < 20; i++) {
        std::cout << "Entry " << i << ": " << current_page[i] << std::endl;
    }
}

int main() {
    std::cout << "=== STARTING 4KB STRESS TEST ===" << std::endl;

    memory_grid allocator = initialize_allocatorc();
    
    std::cout << "\n--- Test 1: Fragmentation (Swiss Cheese) ---" << std::endl;
    std::vector<int*> ptrs;
    const int NUM_ALLOCS = 50;
    const int CHUNK_SIZE = 32;

    for (int i = 0; i < NUM_ALLOCS; i++) {
        int* p = (int*)malloc_from_page(allocator.allocations_head_page, CHUNK_SIZE);
        if (!p) {
            std::cerr << "FAILED at index " << i << "!" << std::endl;
            return 1;
        }
        std::cout << "Allocating block " << p << std::endl;
        *p = i;
        ptrs.push_back(p);
    }
    std::cout << "Allocated " << NUM_ALLOCS << " blocks of " << CHUNK_SIZE << " bytes." << std::endl;

    for (int i = 0; i < NUM_ALLOCS; i += 2) {
        freec_from_page(allocator.allocations_head_page, ptrs[i]);
        ptrs[i] = nullptr; 
    }
    std::cout << "Freed 50% of blocks to create holes." << std::endl;

    for (int i = 0; i < NUM_ALLOCS; i += 2) {
        ptrs[i] = (int*)malloc_from_page(allocator.allocations_head_page, CHUNK_SIZE);
        if (!ptrs[i]) {
            std::cerr << "Re-allocation FAILED! Fragmentation logic might be broken." << std::endl;
            return 1;
        }
        *ptrs[i] = 999;
    }
    std::cout << "Successfully refilled the holes." << std::endl;

    for (int i = 0; i < NUM_ALLOCS; i++) {
        freec_from_page(allocator.allocations_head_page, ptrs[i]);
    }

    std::cout << "\n--- Test 2: Coalescing (The Big Squeeze) ---" << std::endl;
    
    int* big_chunk = (int*)malloc_from_page(allocator.allocations_head_page, 3500); 

    if (big_chunk) {
        std::cout << "SUCCESS: Allocated 3500 bytes. Coalescing is working!" << std::endl;
        big_chunk[874] = 1234; 
        freec_from_page(allocator.allocations_head_page, big_chunk);
    } else {
        std::cerr << "FAILURE: Could not allocate 3500 bytes after freeing everything." << std::endl;
        std::cerr << "Likely cause: Allocator is not merging adjacent free blocks." << std::endl;
    }
    
    std::cout << "\n--- Test 3: Capacity Limit ---" << std::endl;

    int* half1 = (int*)malloc_from_page(allocator.allocations_head_page, 2000);
    int* half2 = (int*)malloc_from_page(allocator.allocations_head_page, 2000); // This should FAIL (2000+2000 > 4096)

    if (half1) std::cout << "Allocated first 2000 bytes (Expected)." << std::endl;
    if (!half2) std::cout << "Second 2000 bytes FAILED (Expected - Out of memory)." << std::endl;
    else std::cout << "Second 2000 bytes SUCCEEDED (Surprising! Check your math)." << std::endl;

    if (half1) freec_from_page(allocator.allocations_head_page, half1);
    if (half2) freec_from_page(allocator.allocations_head_page, half2);

    std::cout << "\n--- Test 3.5: The True Limit ---" << std::endl;
    
    int* p1 = (int*)malloc_from_page(allocator.allocations_head_page, 2030);
    int* p2 = (int*)malloc_from_page(allocator.allocations_head_page, 2030);

    if (p1 && p2) {
        std::cout << "Both fit! Your headers are tiny (<= 18 bytes)." << std::endl;
        
        int* p3 = (int*)malloc_from_page(allocator.allocations_head_page, 100); 
        if (!p3) std::cout << "Final allocation failed as expected (OOM handled)." << std::endl;
        else std::cout << "ERROR: We somehow allocated more than 4096 bytes? Did you implement expansion?" << std::endl;
        
    } else if (p1 && !p2) {
        std::cout << "Second allocation failed as expected. OOM Logic works." << std::endl;
    }

    if (p1) freec_from_page(allocator.allocations_head_page, p1);
    if (p2) freec_from_page(allocator.allocations_head_page, p2);

    std::cout << "\n--- Cleaning up Allocator ---" << std::endl;

    uninitialize_allocatorc(&allocator);
    std::cout << "\n=== TEST COMPLETE ===" << std::endl;

    return 0;
}