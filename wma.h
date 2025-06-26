//////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Wasm Memory Allocator
//
// License: MIT (see end of file)
// Author: lazergenixdev
//
//////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef WMA_H
#define WMA_H
#include <stddef.h>
#include <stdint.h>

#ifndef WMA_DEF
#define WMA_DEF extern
#endif

#define WMA_PAGE_SIZE 65526
#define WMA_INVALID ((void*)0xFFFFFFFF)
#define WMA_IN_USE_BIT 0x80000000
#define WMA_SIZE_MASK  0x7FFFFFFF

typedef struct {
	uint32_t offset;
	uint32_t size; // Top bit set -> page is in use
} Wma_Memory_Range;

typedef struct {
	uint32_t          start;
	uint32_t          total_size;
	uint32_t          available_size;
	uint32_t          slot_capacity;
	uint32_t*         slot_count;
	Wma_Memory_Range* slots;
} Wma_Fixed_Heap;

typedef struct {
	int temp;	
} Wma_Growing_Heap;

// Size to WASM page count
#define WMA_MB(AMOUNT) (16*(AMOUNT))
#define WMA_GB(AMOUNT) (1024*16*(AMOUNT))

WMA_DEF void  wma_create_fixed_heap(Wma_Fixed_Heap* out_Heap, size_t Page_Count);
WMA_DEF void  wma_destroy_fixed_heap(Wma_Fixed_Heap* Heap);
WMA_DEF void* wma_fixed_realloc(Wma_Fixed_Heap* Heap, void* Ptr, size_t Size);
WMA_DEF void* wma_fixed_alloc(Wma_Fixed_Heap* Heap, size_t Size);
WMA_DEF void  wma_fixed_free(Wma_Fixed_Heap* Heap, void* Ptr);

WMA_DEF void  wma_create_growing_heap(Wma_Growing_Heap* out_Heap);
WMA_DEF void  wma_destroy_growing_heap(Wma_Growing_Heap* Heap);
WMA_DEF void* wma_growing_realloc(Wma_Growing_Heap* Heap, void* Ptr);
WMA_DEF void* wma_growing_alloc(Wma_Growing_Heap* Heap, size_t Size);
WMA_DEF void  wma_growing_free(Wma_Growing_Heap* Heap, void* Ptr);

//////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WMA_IMPLEMENTATION

// There are a fixed number of allocations allowed.
// This limit is set such that each page can have
// up to 1024 Allocations/Page. This does not restrict
// how big allocations can be. Every 8 pages need a
// single page to bookkeep the allocations.

static uint32_t wma__ceil_div(uint32_t Num, uint32_t Den)
{
    return (Num + Den - 1) / Den;
}

WMA_DEF void wma_create_fixed_heap(Wma_Fixed_Heap* out_Heap, size_t Page_Count)
{
	// Allocate memory required
	int num_bookkeep_pages = wma__ceil_div(Page_Count, 8);
	int pages_required = num_bookkeep_pages + Page_Count;
	int start_page = __builtin_wasm_memory_grow(0, pages_required);

	// Setup heap data structure
	out_Heap->start          = start_page * WMA_PAGE_SIZE;
	out_Heap->total_size     = pages_required * WMA_PAGE_SIZE;
	out_Heap->available_size = Page_Count * WMA_PAGE_SIZE;
	out_Heap->slot_capacity  = num_bookkeep_pages * 1024 - 1;
	out_Heap->slot_count     = (uint32_t*)out_Heap->start;
	out_Heap->slots          = (Wma_Memory_Range*)out_Heap->start + sizeof(uint32_t);

	// Initialize initial state of the Heap
	*out_Heap->slot_count = 1;
	out_Heap->slots[0] = (Wma_Memory_Range) { .size = out_Heap->available_size };
}

static void wma__shift_slots_up(Wma_Fixed_Heap* Heap, uint32_t Index)
{
	__wasm_panic("Error", __FUNCTION__, __FILE__,  __LINE__);
}

WMA_DEF void* wma_fixed_alloc(Wma_Fixed_Heap* Heap, size_t Size)
{
	// Find a slot that is free
	uint32_t count = *Heap->slot_count;
	for (uint32_t i = 0; i < count; ++i)
	{
		Wma_Memory_Range* slot = &Heap->slots[i];

		if (slot->size & WMA_IN_USE_BIT)
			continue;

		uint32_t slot_size = slot->size & WMA_SIZE_MASK;

		// Make sure this slot has enough space to fit allocation
		if (slot_size < Size)
			continue;

		// Split slot into two
		if (slot_size > Size) {
			wma__shift_slots_up(Heap, i+1);

			// Resize this slot
			slot->size = Size | WMA_IN_USE_BIT;
			
			// Create new slot
			Heap->slots[i+1] = (Wma_Memory_Range) {
				.offset = slot->offset + Size,
				.size   = slot_size - Size,
			};
			*Heap->slot_count += 1;
		}
		// Otherwise just use whole slot
		else slot->size |= WMA_IN_USE_BIT;

		return (void*)(Heap->start + slot->offset);
	}

	// Did not find anything free
	return WMA_INVALID;
}

WMA_DEF void wma_fixed_free(Wma_Fixed_Heap* Heap, void* Ptr)
{
	uint32_t offset = (uint32_t)Ptr - Heap->start;

}

#endif

#endif // WMA_H
