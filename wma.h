#ifndef WMA_H
#define WMA_H
#include <stdint.h>

#define WMA_DEF extern

typedef struct {
	int temp;	
} Wma_Fixed_Heap;

typedef struct {
	int temp;	
} Wma_Growing_Heap;

WMA_DEF void wma_create_fixed_heap(Wma_Fixed_Heap* out_Heap, size_t Size);
WMA_DEF void wma_create_growing_heap(Wma_Growing_Heap* out_Heap);

WMA_DEF void* wma_fixed_realloc(Wma_Fixed_Heap* Heap, void* Ptr, size_t Size);
WMA_DEF void* wma_fixed_alloc(Wma_Fixed_Heap* Heap, size_t Size);
WMA_DEF void  wma_fixed_free(Wma_Fixed_Heap* Heap, void* Ptr);


WMA_DEF void wma_destroy_fixed_heap(Wma_Fixed_Heap* Heap);
WMA_DEF void wma_destroy_growing_heap(Wma_Growing_Heap* Heap);

WMA_DEF void* wma_growing_realloc(Wma_Fixed_Heap* Heap, void* Ptr);
WMA_DEF void* wma_growing_alloc(Wma_Fixed_Heap* Heap, size_t Size);
WMA_DEF void  wma_growing_free(Wma_Fixed_Heap* Heap, void* Ptr);

#endif WMA_H
