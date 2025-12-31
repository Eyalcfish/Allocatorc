#include <iostream>
#include <windows.h>

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

void* mallocc(memory_grid* allocator, size_t size) {
    size_t* last_page = (size_t*) allocator->allocations_head_page[0];
    size_t* next_page = (size_t*)allocator->allocations_head_page[1];
    int free_size = 0;
    void* free_size_address = nullptr;
    int last_index = 0;
    for (int i = 2; i < 512; i+=2) {
        if (allocator->allocations_head_page[i] == 0) {
            free_size += allocator->allocations_head_page[i+1];
            last_index = i;
        } else {
            free_size = 0;
            free_size_address = (void*)(allocator->allocations_head_page[i] + allocator->allocations_head_page[i+1]);
        }
        if (free_size >= size) {
            allocator->allocations_head_page[last_index] = (size_t)free_size_address;
            allocator->allocations_head_page[last_index+1] = (size_t)size;
            std::cout<< "Allocated in gap: " << free_size_address << std::endl;
            return free_size_address;
        }
    }
    if (free_size_address != nullptr) {
        allocator->allocations_head_page[last_index] = (size_t)free_size_address;
        allocator->allocations_head_page[last_index+1] = (size_t)size;
        std::cout<< "Allocated at the end: " << free_size_address << " " << last_index << std::endl;
        return free_size_address;
        
    }
    std::cout << "Allocation failed." << std::endl;
    return nullptr;
}

void* freec(memory_grid* allocator, void* ptr) {
    for (int i = 4; i < 512; i+=2) {
        if (allocator->allocations_head_page[i] == (size_t) ptr) {
            std::cout << "Freed memory at: " << (size_t)ptr << " index: " << i << std::endl;
            allocator->allocations_head_page[i] = 0;
        }
    }
    return ptr;
}

void print_page(void* page) {
    size_t* current_page = (size_t*) page;
    for (int i = 500; i < 512; i++) {
        std::cout << "Entry " << i << ": " << current_page[i] << std::endl;
    }
}

int main() {
    memory_grid allocator = initialize_allocatorc();

    std::cout << "Allocator initialized." << std::endl;
    int* ptr1 = (int*) mallocc(&allocator, 16);
    print_page(allocator.allocations_head_page);
    freec(&allocator, ptr1);
    print_page(allocator.allocations_head_page);
    int* ptr2 = (int*) mallocc(&allocator, 8);
    std::cout << "Allocated memory at: " << ptr1 << " " << ptr2 << std::endl;
    ptr1[0] = 42;
    ptr2[0] = 3; 
    ptr1[1] = 84;
    std::cout << "Allocated memory at: " << ptr1[0] << std::endl;
    std::cout << "Allocated memory at: " << ptr1[1] << std::endl;
    std::cout << "Allocated memory at: " << ptr2[0] << std::endl;
    // print_page(allocator.head_page);
    freec(&allocator, ptr2);
    uninitialize_allocatorc(&allocator);
    return 0;
}