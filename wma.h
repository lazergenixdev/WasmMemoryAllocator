// 
//    WASM Memory Allocator -- version 1.1.0
// --------------------------------------------
// a general purpose memory allocator for WASM
//
// License: MIT (see end of file)
// Author: lazergenixdev
//
// DOCUMENTATION:
//     Simply include this header and in one file
//     define `WMA_IMPLEMENTATION`.
//
//     FUNCTIONS:
//       - wma_alloc   <=> C malloc
//       - wma_realloc <=> C realloc
//       - wma_free    <=> C free
//
#ifndef WMA_H
#define WMA_H
#include <stddef.h>
#include <stdint.h>

#ifndef WMA_DEF
#define WMA_DEF extern
#endif

#define WMA_PAGE_SIZE 65536             // ~ Fixed page size for WASM 
#define WMA_INVALID ((void*)0xFFFFFFFF) // ~ Pointer that will always be invalid

// ~ Set which allocator to use as the global allocator
// fast     -- Simple allocator with a fixed number of allocations.
//          -- Allocations may be of any size.
//          -- ! Does not work with external use of `memory_grow`.
// generic  -- Default allocator, unlimited allocations of any size.
#ifndef WMA_ALLOCATOR
#define WMA_ALLOCATOR generic
#endif

#define WMA__fast    0
#define WMA__generic 1
#define WMA__COMBINE(A,B) A ## B
#define WMA__COMBINE2(A,B) WMA__COMBINE(A,B)

// ~ Determine which allocator is being used at compile-time
#define WMA_USING_ALLOCATOR(NAME) WMA__COMBINE2(WMA__, WMA_ALLOCATOR) == WMA__COMBINE(WMA__, NAME)

// ~ Set maximum number of allocations for fast allocator
#ifndef WMA_FAST_MAX_ALLOCATIONS
#define WMA_FAST_MAX_ALLOCATIONS 65536
#endif

#define WMA__FN(A,B,C) A ## B ## C
#define wma__realloc(A,Ptr,Size) WMA__FN(wma_, A, _realloc)(&wma_global_allocator.A, Ptr, Size)
#define wma__alloc(A,Size)       WMA__FN(wma_, A, _alloc  )(&wma_global_allocator.A, Size) 
#define wma__free(A,Ptr)         WMA__FN(wma_, A, _free   )(&wma_global_allocator.A, Ptr)

// ~ Access the allocator, and each of it's corresponding functions
#define wma_allocator         (wma_global_allocator.WMA_ALLOCATOR)
#define wma_realloc(Ptr,Size) wma__realloc(WMA_ALLOCATOR, Ptr, Size)
#define wma_alloc(Size)       wma__alloc(WMA_ALLOCATOR, Size) 
#define wma_free(Ptr)         wma__free(WMA_ALLOCATOR, Ptr)

typedef struct {
	const char* file;
	const char* function;
	int         line;
	int         order;
} Wma_Metadata;

typedef struct {
	uint32_t offset; // Offset relative to `heap_start`
	uint32_t allocated:1;
	uint32_t size:31;
} Wma_Slot;

typedef struct {
	uint32_t      start;          // Start of total heap memory
	uint32_t      heap_start;     // Start of memory that can be allocated
	uint32_t      total_size;     // Total size of heap including overhead
	uint32_t      available_size; // Total amount of memory that can be allocated (able to grow)
	uint32_t      slot_capacity;  // Maximum number of slots
	uint32_t      slot_count;     // Current number of slots
	Wma_Slot*     slots;
	uint32_t      first_free;     // Index of first free slot
	uint32_t      allocated;      // Total size of allocated memory
#ifdef WMA_TRACK_ALLOCATIONS
	Wma_Metadata* metadata;       // Mirror of slots, giving extra allocation info
	Wma_Metadata  next_metadata;  // The metadata of the next allocation
#endif
} Wma_Fast_Allocator;

typedef struct Wma_Region {
	uint32_t size:31;
	uint32_t used:1;
	struct Wma_Region* prev;
	struct Wma_Region* next;
} Wma_Region;

typedef struct {
	Wma_Region* heads[64];
	Wma_Region* tails[64];
} Wma_Generic_Allocator;

typedef union {
	Wma_Fast_Allocator    fast;
	Wma_Generic_Allocator generic;
} Wma_Global_Allocator;

typedef struct Wma_Arena_Block {
	struct Wma_Arena_Block* prev;
} Wma_Arena_Block;

typedef struct {
	uint32_t offset;
	uint32_t block_size;
	Wma_Arena_Block* current;
} Wma_Arena_Allocator;

// Size to WASM page count
#define WMA_MB(AMOUNT) (16*(AMOUNT))

//////////////////////////////////////////////////////////////////////////////////////////////////

WMA_DEF Wma_Global_Allocator wma_global_allocator;

WMA_DEF void* wma_fast_realloc(Wma_Fast_Allocator* Allocator, void* Ptr, size_t Size);
WMA_DEF void* wma_fast_alloc  (Wma_Fast_Allocator* Allocator, size_t Size);
WMA_DEF void  wma_fast_free   (Wma_Fast_Allocator* Allocator, void* Ptr);

WMA_DEF void* wma_generic_realloc(Wma_Generic_Allocator* Allocator, void* Ptr, size_t Size);
WMA_DEF void* wma_generic_alloc  (Wma_Generic_Allocator* Allocator, size_t Size);
WMA_DEF void  wma_generic_free   (Wma_Generic_Allocator* Allocator, void* Ptr);

WMA_DEF void wma_arena_allocator_create (Wma_Arena_Allocator* out_Allocator, uint32_t Page_Count);
WMA_DEF void wma_arena_allocator_destroy(Wma_Arena_Allocator* Allocator);

WMA_DEF void* wma_arena_alloc   (Wma_Arena_Allocator* Allocator, size_t Size);
WMA_DEF void  wma_arena_free_all(Wma_Arena_Allocator* Allocator);
WMA_DEF void  wma_arena_free    (Wma_Arena_Allocator* Allocator, void* Ptr);

// Note: Arenas can ONLY call free() on the last item allocated, is also not very useful.

//////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WMA_IMPLEMENTATION

#if defined(wma__panic)
#define WMA__PANIC(TOPIC, WHAT) wma__panic(TOPIC, WHAT, __LINE__)
#else
#define WMA__PANIC(TOPIC, WHAT) (void)0
#endif

// Define wma__panic to enable runtime assertions
#if !defined(wma__assert) && defined(wma__panic)
#define wma__assert(EXPR) \
	do { \
		if (!(EXPR)) { \
			WMA__PANIC("Assertion Failed", #EXPR); \
		} \
	} while (0)
#elif !defined(wma__assert)
#define wma__assert(EXPR) (void)0
#endif

static uint32_t wma__ceil_div(uint32_t Num, uint32_t Den)
{
	return (Num + Den - 1) / Den;
}

static uint32_t wma__min(uint32_t A, uint32_t B)
{
	return A < B ? A : B;
}

static uint32_t wma__max(uint32_t A, uint32_t B)
{
	return A < B ? B : A;
}

static void wma__memory_copy(void* Dst, void* Src, size_t Size)
{
	for (int i = 0; i < Size; ++i)
	{
		((uint8_t*)Dst)[i] = ((uint8_t*)Src)[i];
	}
}

Wma_Global_Allocator wma_global_allocator = {0};

// Fast Allocator Implementation:
// There are a fixed number of allocations allowed.
// Every 8 pages need a single page to bookkeep
// the allocations.

static void wma_fast_allocator_reset(Wma_Fast_Allocator* Allocator)
{
	Allocator->first_free = 0;
	Allocator->slot_count = 1;
	Allocator->slots[0] = (Wma_Slot) { .size = Allocator->available_size };
}

static void wma_fast_allocator_create(Wma_Fast_Allocator* out_Allocator, uint32_t Max_Allocations)
{
	wma__assert(out_Allocator != NULL);
	wma__assert(Max_Allocations != 0);

	// Allocate memory required
	int num_bookkeep_pages = wma__ceil_div(Max_Allocations, WMA_PAGE_SIZE / sizeof(Wma_Slot));
	int pages_required = num_bookkeep_pages + 1;
	int start_page = __builtin_wasm_memory_grow(0, pages_required);

	// Setup heap data structure
	out_Allocator->start          = start_page * WMA_PAGE_SIZE;
	out_Allocator->heap_start     = (start_page + num_bookkeep_pages) * WMA_PAGE_SIZE;
	out_Allocator->total_size     = pages_required * WMA_PAGE_SIZE;
	out_Allocator->available_size = WMA_PAGE_SIZE;
	out_Allocator->slot_capacity  = num_bookkeep_pages * WMA_PAGE_SIZE / sizeof(Wma_Slot) - 1;
	out_Allocator->slots          = (Wma_Slot*)out_Allocator->start;
	wma_fast_allocator_reset(out_Allocator);
}

// [i-1][ i ][i+1][i+2]
//           ---->
// [i-1][ i ][ i ][i+1][i+2]
static void wma__shift_slots_up(Wma_Fast_Allocator* Allocator, uint32_t Index)
{
	uint32_t count = Allocator->slot_count;
	wma__assert(count < Allocator->slot_capacity);

	for (uint32_t i = count; i > Index; --i)
		Allocator->slots[i] = Allocator->slots[i-1];
}

static void* wma__assign_slot(Wma_Fast_Allocator* Allocator, uint32_t Index, size_t Size)
{
	Wma_Slot* slot = &Allocator->slots[Index];

	// Fit slot to size, if we are able to create a new free slot
	if (slot->size > Size && Allocator->slot_count < Allocator->slot_capacity) {
		// Make room so we can insert a new slot
		wma__shift_slots_up(Allocator, Index+1);
		// Create new slot with remaining space
		Allocator->slots[Index+1] = (Wma_Slot) {
			.offset = slot->offset + Size,
			.size   = slot->size - Size,
		};
		// Resize this slot to fit allocation
		slot->size = Size;
		Allocator->slot_count += 1;
	}

	// Advance first free slot
	if (Allocator->first_free == Index) {
		Allocator->first_free += 1;
	}
	
	slot->allocated = 1;
	Allocator->allocated += slot->size;
	return (void*)(Allocator->heap_start + slot->offset);
}

WMA_DEF void* wma_fast_alloc(Wma_Fast_Allocator* Allocator, size_t Size)
{
	if (Allocator->available_size == 0)
		wma_fast_allocator_create(Allocator, WMA_FAST_MAX_ALLOCATIONS);
	
	for (uint32_t i = Allocator->first_free; i < Allocator->slot_count; ++i)
	{
		Wma_Slot* slot = &Allocator->slots[i];
		
		if (slot->allocated)   continue;
		if (slot->size < Size) continue;
		
		return wma__assign_slot(Allocator, i, Size);
	}

	// Failed to find a slot that is both free and with enough space
	// We have 2 options now:

	// 1. Grow the last slot
	Wma_Slot* last_slot = &Allocator->slots[Allocator->slot_count-1];
	if (last_slot->allocated == 0) {
		uint32_t grow_amount = Size - last_slot->size;
		uint32_t grow_pages = wma__ceil_div(grow_amount, WMA_PAGE_SIZE);

		__builtin_wasm_memory_grow(0, grow_pages);
		Allocator->total_size     += WMA_PAGE_SIZE * grow_pages;
		Allocator->available_size += WMA_PAGE_SIZE * grow_pages;

		last_slot->size += WMA_PAGE_SIZE * grow_pages;
		wma__assert(last_slot->size >= Size);

		return wma__assign_slot(Allocator, Allocator->slot_count - 1, Size);
	}

	// 2. Grow the heap and insert a new slot
	if (Allocator->slot_count == Allocator->slot_capacity) {
		WMA__PANIC("WMA", "Maximum number of allocations reached");
	}

	uint32_t grow_pages = wma__ceil_div(Size, WMA_PAGE_SIZE);
	last_slot = &Allocator->slots[Allocator->slot_count];
	last_slot->offset = Allocator->available_size;
	last_slot->size   = WMA_PAGE_SIZE * grow_pages;
	Allocator->slot_count += 1;
	wma__assert(last_slot->size >= Size);

	__builtin_wasm_memory_grow(0, grow_pages);
	Allocator->total_size     += WMA_PAGE_SIZE * grow_pages;
	Allocator->available_size += WMA_PAGE_SIZE * grow_pages;

	return wma__assign_slot(Allocator, Allocator->slot_count - 1, Size);
}

static void wma__shift_slots_down(Wma_Fast_Allocator* Allocator, uint32_t Index)
{
	uint32_t count = Allocator->slot_count;
	wma__assert(count > 0);
	for (uint32_t i = Index; i < count; ++i)
		Allocator->slots[i] = Allocator->slots[i+1];
	Allocator->slot_count -= 1;
}

static void wma__fast_free_slot(Wma_Fast_Allocator* Allocator, uint32_t Index)
{
	Wma_Slot* slot = &Allocator->slots[Index];
	uint32_t count = Allocator->slot_count;
	uint32_t index = Index;
	slot->allocated = 0;
	Allocator->allocated -= slot->size;

	// Combine with slot to the right
	if (Index < count-1) {
		if (Allocator->slots[Index+1].allocated == 0) {
			slot->size += Allocator->slots[Index+1].size;
			wma__shift_slots_down(Allocator, Index+1);
		}
	}
	// Combine with slot to the left
	if (Index > 0) {
		if (Allocator->slots[Index-1].allocated == 0) {
			Allocator->slots[Index-1].size += slot->size;
			wma__shift_slots_down(Allocator, Index);
			index = Index - 1;
		}
	}

	if (Allocator->first_free > index) {
		Allocator->first_free = index;
	}
}

static uint32_t wma__fast_find_slot(Wma_Fast_Allocator* Allocator, void* Ptr)
{
	uint32_t offset = (uint32_t)Ptr - Allocator->heap_start;
	uint32_t left = 0;
	uint32_t right = Allocator->slot_count - 1;
	while (left <= right)
	{
		uint32_t mid = (left + right) / 2;
		Wma_Slot* mid_slot = &Allocator->slots[mid];

		if (mid_slot->offset < offset) {
			left = mid + 1;
		}
		else if (mid_slot->offset > offset) {
			right = mid - 1;
		}
		else {
			return mid;
		}
	}
	return -1;
}

WMA_DEF void wma_fast_free(Wma_Fast_Allocator* Allocator, void* Ptr)
{
	uint32_t index = wma__fast_find_slot(Allocator, Ptr);
	wma__assert(index < Allocator->slot_count);
	wma__fast_free_slot(Allocator, index);
}

WMA_DEF void* wma_fast_realloc(Wma_Fast_Allocator* Allocator, void* Ptr, size_t Size)
{
	if (Ptr == NULL)
		return wma_fast_alloc(Allocator, Size);

	// Find slot at pointer
	uint32_t index = wma__fast_find_slot(Allocator, Ptr);
	wma__assert(index < Allocator->slot_count);

	// Try to extend this slot
	uint32_t old_size = Allocator->slots[index].size;
	uint32_t grow_amount = Size - old_size;
	uint32_t count = Allocator->slot_count;
	for (;index < count-1;) {
		Wma_Slot* next_slot = &Allocator->slots[index+1];
		
		if (next_slot->allocated)          break;
		if (next_slot->size < grow_amount) break;
			
		next_slot->size -= grow_amount;
		if (next_slot->size == 0) {
			wma__shift_slots_down(Allocator, index+1);
		}
		else {
			Allocator->slots[index+1].offset += grow_amount;
		}

		Allocator->slots[index].size = Size;
		return Ptr;
	}

	// Extending failed, so free this slot and allocate another
	wma__fast_free_slot(Allocator, index);
	void* ptr = wma_fast_alloc(Allocator, Size);
	wma__memory_copy(ptr, Ptr, old_size);
	return ptr;
}

static int wma__bucket_index(size_t Size)
{
	if (Size < 128) return (Size >> 3) - 1;
	int clz = __builtin_clz(Size);
	return (clz > 19)
		?          110 - (clz<<2) + ((Size >> (29-clz)) ^ 4)
		: wma__min( 71 - (clz<<1) + ((Size >> (30-clz)) ^ 2), 63);
}

static int wma__regions_are_adjacent(Wma_Region* Left, Wma_Region* Right)
{
	return (uint32_t)(Left + 1) + Left->size == (uint32_t)Right;
}

static void* wma__generic_try_allocate(Wma_Generic_Allocator* Allocator, uint32_t Bucket_Index, Wma_Region* Region, size_t Size)
{
	if (Region->size < Size)
		return WMA_INVALID;
	
	if (Region->size > Size + sizeof(Wma_Region)) {
		Wma_Region* new_region = (void*)((uint32_t)(Region + 1) + Size);
		new_region->size = Region->size - Size - sizeof(Wma_Region);
		new_region->prev = Region;
		new_region->next = Region->next;
		Region->next = new_region;
		Region->size = Size;
	
		wma__assert(wma__regions_are_adjacent(Region, new_region));
	}

	// Connect the two free regions inbetween
	if (Region->prev) {
		Region->prev->next = Region->next;
	}
	else {
		Allocator->heads[Bucket_Index] = Region->next;
	}
	if (Region->next) {
		Region->next->prev = Region->prev;
	}
	else {
		Allocator->tails[Bucket_Index] = Region->prev;
	}

	Region->used = 1;
	return Region + 1;
}

WMA_DEF void* wma_generic_alloc(Wma_Generic_Allocator* Allocator, size_t Size)
{
    int bucket_index = wma__bucket_index(wma__max(Size, 8));

    Wma_Region* region = Allocator->heads[bucket_index];
    while (region)
	{
		void* ptr = wma__generic_try_allocate(Allocator, bucket_index, region, Size);	
		if (ptr != WMA_INVALID)
			return ptr;

		region = region->next;
    }

	uint32_t pages_required = wma__ceil_div(Size + sizeof(Wma_Region), WMA_PAGE_SIZE);
	uint32_t start_page = __builtin_wasm_memory_grow(0, pages_required);
	region = (void*)(start_page * WMA_PAGE_SIZE);
	region->size = pages_required * WMA_PAGE_SIZE - sizeof(Wma_Region);
	region->used = 0;
	region->prev = NULL;
	region->next = NULL;

	if (Allocator->heads[bucket_index] == NULL) {
		Allocator->heads[bucket_index] = region;
		Allocator->tails[bucket_index] = region;
	}
	else {
		Wma_Region* last = Allocator->tails[bucket_index];
		region->prev = last;
		last->next = region;
		Allocator->tails[bucket_index] = region;
	}

    return wma__generic_try_allocate(Allocator, bucket_index, region, Size);
}

static void* wma__generic_try_extend(Wma_Generic_Allocator* Allocator, uint32_t Bucket_Index, Wma_Region* Region)
{
	return WMA_INVALID;
}

WMA_DEF void* wma_generic_realloc(Wma_Generic_Allocator* Allocator, void* Ptr, size_t Size)
{
	if (Ptr == NULL)
		return wma_generic_alloc(Allocator, Size);

	Wma_Region* region = (void*)((uint32_t)Ptr - sizeof(Wma_Region));
    wma__assert(region->used == 1);
	int bucket_index = wma__bucket_index(wma__max(region->size, 8));
	uint32_t old_size = region->size;

	if (Size < region->size) {
		// too easy xD
		return Ptr;
	}

	if (Size > region->size) {
		void* ptr = wma__generic_try_extend(Allocator, bucket_index, region);
		if (ptr != WMA_INVALID)
			return ptr;
	}

	wma_generic_free(Allocator, Ptr);
	void* ptr = wma_generic_alloc(Allocator, Size);
	wma__memory_copy(ptr, Ptr, old_size);
	return ptr;
}

static void wma__combine_regions(Wma_Generic_Allocator* Allocator, uint32_t Bucket_Index, Wma_Region* Region)
{
	if (Region->prev && wma__regions_are_adjacent(Region->prev, Region)) {
		Region->prev->size += Region->size + sizeof(Wma_Region);
		Region->prev->next = Region->next;
		Region->next->prev = Region->prev;
		if (!Region->next) {
			Allocator->tails[Bucket_Index] = Region->prev;
		}
		Region = Region->prev; // prev now replaces this region
	}
	
	if (Region->next && wma__regions_are_adjacent(Region, Region->next)) {
		Region->size += Region->next->size + sizeof(Wma_Region);
		if (Region->next->next) {
			Region->next->next->prev = Region;
		}
		else {
			Allocator->tails[Bucket_Index] = Region;
		}
		Region->next = Region->next->next;
	}
}

WMA_DEF void wma_generic_free(Wma_Generic_Allocator* Allocator, void* Ptr)
{
	Wma_Region* region = (void*)((uint32_t)Ptr - sizeof(Wma_Region));
    wma__assert(region->used == 1);
	int bucket_index = wma__bucket_index(wma__max(region->size, 8));

	region->used = 0;

	Wma_Region* left = region->prev;
	while (left)
	{
		if (left->used == 0) break;
		left->next = region;
		left = left->prev;
	}
	
	Wma_Region* right = region->next;
	while (right)
	{
		if (right->used == 0) break;
		right->prev = region;
		right = right->next;
	}

	region->prev = left;
	region->next = right;

	if (left) {
		left->next = region;
	}
	else {
		Allocator->heads[bucket_index] = region;
	}
	if (right) {
		right->prev = region;
	}
	else {
		Allocator->tails[bucket_index] = region;
	}

	wma__combine_regions(Allocator, bucket_index, region);
}

#endif

#endif // WMA_H

//
// version 1.0 (2025.6.26)
//     Very simple allocator implemented, just to get a baseline. There are
//     definitely better ways to implement a heap allocator.
//
// version 1.0.1 (2025.8.3)
//     - Rename basic -> fast
//     - fast: allocating more pages when full
//     - fast: binary search on free and realloc
//     - Added 'generic' allocator
//     - generic: no allocation limit, also not implemented
//
// version 1.1.0 (2025.8.4)
//     - fast: fix problem in realloc where memory would not get copied
//     - generic: need to implement memory shrinking with realloc
//	   - generic: need to implement memory extension with realloc
//
// Roadmap (no plans for when):
//     - Measure performance
//     - Implement memory arenas!!
//     - Implement allocation alignment?
//     - Implement allocation tracking (needed?)
//
// Notes:
//     For debugging, I think it would be nice to be able to use
//     Javascript to hold metadata for each allocation and tracking.
//     That way there won't be any memory corruption (hopefully).
//

//
// MIT License
// 
// Copyright (c) 2025 lazergenixdev
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// 
