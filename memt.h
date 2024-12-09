/*
    to use: just include this file
 */


#ifndef MEM_TRACKER_H
#define MEM_TRACKER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


void* mt_malloc(size_t size, const char* file, uint32_t line);
void mt_free(void* ptr, const char* file, uint32_t line);
void mt_print_report(void);


#define WARN(msg, file, line) fprintf(stderr, "warning: %s at %s:%d\n", msg, file, line)


typedef struct {
    void *address;      // memory location
    size_t size;        // how much memory
    const char *file;   // file where it happened
    uint32_t line;      // line where it happened
} Mem;


#define MAX_ALLOCATIONS 1000000


typedef struct {
    Mem mem[MAX_ALLOCATIONS];
    size_t total_allocated_size;
    size_t total_free_size;
    size_t current_allocated_size;
    size_t max_allocated_size;
    unsigned long allocation_count;
    unsigned long free_count;
    unsigned long failed_allocations;
    unsigned long double_frees;
    unsigned long invalid_frees;
} MemData;


static MemData data = {0};


static Mem* find_by_address(void *address) {
    for (uint32_t i = 0; i < MAX_ALLOCATIONS; i++) {
        if (data.mem[i].address == address)
            return &data.mem[i];
    }
    return NULL;
}


static Mem* find_free_slot() {
    for (uint32_t i = 0; i < MAX_ALLOCATIONS; i++) {
        if (data.mem[i].address == NULL)
            return &data.mem[i];
    }
    return NULL;
}


static void insertm(void *address, size_t size, const char *file, uint32_t line) {
    if (address == NULL) {
        WARN("memory allocation failed", file, line);
        data.failed_allocations++;
        return;
    }

    Mem *mem = find_free_slot();
    if (mem == NULL) {
        WARN("max allocations reached", file, line);
        data.failed_allocations++;
        return;
    }


    mem->address = address;
    mem->size = size;
    mem->file = file;
    mem->line = line;


    data.total_allocated_size += size;
    data.current_allocated_size += size;
    if (data.current_allocated_size > data.max_allocated_size) {
        data.max_allocated_size = data.current_allocated_size;
    }
    data.allocation_count++;
}


static int erase(void *address, const char *file, uint32_t line) {
    if (address == NULL) {
        WARN("tried to free a null pointer", file, line);
        data.invalid_frees++;
        return -1;
    }

    Mem *mem = find_by_address(address);
    if (mem == NULL) {
        WARN("double free or invalid free", file, line);
        data.double_frees++;
        return -1;
    }


    data.total_free_size += mem->size;
    data.current_allocated_size -= mem->size;
    data.free_count++;


    mem->address = NULL;
    mem->size = 0;
    mem->file = NULL;
    mem->line = 0;

    return 0;
}


void mt_print_report() {
    printf("\n======= memory report =======\n");
    printf("total allocations       : %lu\n", data.allocation_count);
    printf("total frees             : %lu\n", data.free_count);
    printf("failed allocations      : %lu\n", data.failed_allocations);
    printf("double/invalid frees    : %lu\n", data.double_frees + data.invalid_frees);
    printf("total memory allocated  : %lu bytes\n", data.total_allocated_size);
    printf("total memory freed      : %lu bytes\n", data.total_free_size);
    printf("current allocated memory: %lu bytes\n", data.current_allocated_size);
    printf("max allocated memory    : %lu bytes\n", data.max_allocated_size);
    printf("memory leaked           : %lu bytes\n", data.total_allocated_size - data.total_free_size);
    printf("=============================\n\n");

    if (data.current_allocated_size != 0) {
        printf("======= detailed leaks =======\n");
        for (int i = 0; i < MAX_ALLOCATIONS; i++) {
            if (data.mem[i].address != NULL) {
                printf("leak at %s:%d - address: %p, size: %lu bytes\n",
                       data.mem[i].file,
                       data.mem[i].line,
                       data.mem[i].address,
                       data.mem[i].size);
            }
        }
        printf("=============================\n\n");
    }
}


void* mt_malloc(size_t size, const char* file, uint32_t line) {
    void *ptr;
    

    #ifdef malloc
    #undef malloc
    #endif
    ptr = malloc(size);

    #define malloc(size) mt_malloc(size, __FILE__, __LINE__)

    insertm(ptr, size, file, line);
    return ptr;
}


void mt_free(void* ptr, const char* file, uint32_t line) {

    #ifdef free
    #undef free
    #endif
    if (erase(ptr, file, line) == 0)
        free(ptr);

    #define free(ptr) mt_free(ptr, __FILE__, __LINE__)
}


static void init_memory_tracking() {
    atexit(mt_print_report);
}


#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void mem_tracker_constructor() {
    init_memory_tracking();
}
#endif

#endif 


#define malloc(size) mt_malloc(size, __FILE__, __LINE__)
#define free(ptr) mt_free(ptr, __FILE__, __LINE__)

#ifdef __cplusplus
}

#endif
