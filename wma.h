// 
//    WASM Memory Allocator -- version 1.0
// ------------------------------------------
// a general purpose memory allocator for WASM
//
// License: MIT (see end of file)
// Author: lazergenixdev
//
#ifndef WMA_H
#define WMA_H
#include <stddef.h>
#include <stdint.h>

#ifndef WMA_DEF
#define WMA_DEF extern
#endif
#ifndef WMA_ALLOCATOR
#define WMA_ALLOCATOR basic
#endif

#define WMA__FN(A,B,C) A ## B ## C
#define wma__allocator(A)        (&wma_global_allocator()->A)
#define wma__realloc(A,Ptr,Size) WMA__FN(wma_, A, _realloc)(wma__allocator(A), Ptr, Size)
#define wma__alloc(A,Size)       WMA__FN(wma_, A,   _alloc)(wma__allocator(A), Size) 
#define wma__free(A,Ptr)         WMA__FN(wma_, A,    _free)(wma__allocator(A), Ptr)

#define WMA_PAGE_SIZE 65536
#define WMA_INVALID ((void*)0xFFFFFFFF)

typedef struct {
	const char* file;
	const char* function;
	int         line;
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
	uint32_t      available_size; // Total amount of memory that can be allocated
	uint32_t      slot_capacity;  // Maximum number of slots
	uint32_t*     slot_count;     // Current number of slots
	Wma_Slot*     slots;
	uint32_t      first_free;     // Index of first free slot
#ifdef WMA_TRACK_ALLOCATIONS
	Wma_Metadata* metadata;       // Mirror of slots, giving extra allocation info
	Wma_Metadata  next_metadata;  // The metadata of the next allocation
#endif
} Wma_Basic_Allocator;

typedef union {
	Wma_Basic_Allocator basic;
} Wma_Global_Allocator;

// Size to WASM page count
#define WMA_MB(AMOUNT) (16*(AMOUNT))

//////////////////////////////////////////////////////////////////////////////////////////////////

#define wma_allocator         wma__allocator(WMA_ALLOCATOR)
#define wma_realloc(Ptr,Size) wma__realloc(WMA_ALLOCATOR, Ptr, Size)
#define wma_alloc(Size)       wma__alloc(WMA_ALLOCATOR, Size) 
#define wma_free(Ptr)         wma__free(WMA_ALLOCATOR, Ptr)

WMA_DEF Wma_Global_Allocator* wma_global_allocator();
WMA_DEF void  wma_create_basic_allocator(Wma_Basic_Allocator* out_Allocator, size_t Initial_Page_Count);
WMA_DEF void  wma_reset_basic_allocator(Wma_Basic_Allocator* Allocator);
WMA_DEF void  wma_grow_basic_allocator(Wma_Basic_Allocator* Allocator);
WMA_DEF void* wma_basic_realloc(Wma_Basic_Allocator* Allocator, void* Ptr, size_t Size);
WMA_DEF void* wma_basic_alloc(Wma_Basic_Allocator* Allocator, size_t Size);
WMA_DEF void  wma_basic_free(Wma_Basic_Allocator* Allocator, void* Ptr);

//////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WMA_IMPLEMENTATION

#define wma__assert(...) (void)0

static uint32_t wma__ceil_div(uint32_t Num, uint32_t Den)
{
	return (Num + Den - 1) / Den;
}

WMA_DEF Wma_Global_Allocator* wma_global_allocator()
{
	static Wma_Global_Allocator allocator = {0};
	return &allocator;
}

// Basic Allocator Implementation:
// There are a fixed number of allocations allowed.
// This limit is set such that each page can have
// up to 1024 Allocations/Page. This does not restrict
// how big allocations can be. Every 8 pages need a
// single page to bookkeep the allocations.

WMA_DEF void wma_create_basic_allocator(Wma_Basic_Allocator* out_Allocator, size_t Page_Count)
{
	// Allocate memory required
	int num_bookkeep_pages = wma__ceil_div(Page_Count, 4);
	int pages_required = num_bookkeep_pages + Page_Count;
	int start_page = __builtin_wasm_memory_grow(0, pages_required);

	// Setup heap data structure
	out_Allocator->start          = start_page * WMA_PAGE_SIZE;
	out_Allocator->heap_start     = (start_page + num_bookkeep_pages) * WMA_PAGE_SIZE;
	out_Allocator->total_size     = pages_required * WMA_PAGE_SIZE;
	out_Allocator->available_size = Page_Count * WMA_PAGE_SIZE;
	out_Allocator->slot_capacity  = num_bookkeep_pages * WMA_PAGE_SIZE / sizeof(Wma_Slot) - 1;
	out_Allocator->slot_count     = (uint32_t*)out_Allocator->start;
	out_Allocator->slots          = (Wma_Slot*)out_Allocator->start + sizeof(uint32_t);
	wma_reset_basic_allocator(out_Allocator);
}

WMA_DEF void wma_reset_basic_allocator(Wma_Basic_Allocator* Allocator)
{
	*Allocator->slot_count = 1;
	Allocator->slots[0] = (Wma_Slot) { .size = Allocator->available_size };
}

static void wma__shift_slots_up(Wma_Basic_Allocator* Allocator, uint32_t Index)
{
	uint32_t count = *Allocator->slot_count;
	wma__assert(count < Allocator->slot_capacity);

	for (uint32_t i = count; i > Index; --i)
		Allocator->slots[i] = Allocator->slots[i-1];
}

WMA_DEF void* wma_basic_alloc(Wma_Basic_Allocator* Allocator, size_t Size)
{
	if (Allocator->available_size == 0)
		wma_create_basic_allocator(Allocator, 8);
	
	if (Size == 0)
		return WMA_INVALID;
		
	start: (void)0;
	uint32_t count = *Allocator->slot_count;
	for (uint32_t i = 0; i < count; ++i)
	{
		Wma_Slot* slot = &Allocator->slots[i];
		
		if (slot->allocated)
			continue;
		if (slot->size < Size)
			continue;

		if (slot->size > Size) {
			// Make room so we can insert a new slot
			wma__shift_slots_up(Allocator, i+1);
			// Create new slot with remaining space
			Allocator->slots[i+1] = (Wma_Slot) {
				.offset = slot->offset + Size,
				.size   = slot->size - Size,
			};
			// Resize this slot to fit allocation
			slot->size = Size;
			*Allocator->slot_count += 1;
		}
		
		slot->allocated = 1;
		return (void*)(Allocator->heap_start + slot->offset);
	}

	__builtin_wasm_memory_grow(0, 1);
	// !!! Very bad, assuming last slot is freed
	Wma_Slot* slot = &Allocator->slots[*Allocator->slot_count-1];
	slot->size += WMA_PAGE_SIZE;
	Allocator->available_size += WMA_PAGE_SIZE;
	goto start;
}

static void wma__shift_slots_down(Wma_Basic_Allocator* Allocator, uint32_t Index)
{
	uint32_t count = *Allocator->slot_count;
	for (uint32_t i = Index; i < count; ++i)
		Allocator->slots[i] = Allocator->slots[i+1];
	*Allocator->slot_count -= 1;
}

WMA_DEF void wma__basic_free_slot(Wma_Basic_Allocator* Allocator, uint32_t Index)
{
	Wma_Slot* slot = &Allocator->slots[Index];
	uint32_t count = *Allocator->slot_count;

	slot->allocated = 0;

	if (Index < count-1) {
		if (Allocator->slots[Index+1].allocated == 0) {
			slot->size += Allocator->slots[Index+1].size;
			wma__shift_slots_down(Allocator, Index+1);
		}
	}

	if (Index > 0) {
		if (Allocator->slots[Index-1].allocated == 0) {
			Allocator->slots[Index-1].size += slot->size;
			wma__shift_slots_down(Allocator, Index);
		}
	}
}

WMA_DEF void wma_basic_free(Wma_Basic_Allocator* Allocator, void* Ptr)
{
	uint32_t offset = (uint32_t)Ptr - Allocator->heap_start;

	uint32_t count = *Allocator->slot_count;
	for (uint32_t i = 0; i < count; ++i)
	{
		if (Allocator->slots[i].offset != offset)
			continue;

		wma__basic_free_slot(Allocator, i);
		return;
	}
}

WMA_DEF void* wma_basic_realloc(Wma_Basic_Allocator* Allocator, void* Ptr, size_t Size)
{
	if (Size == 0)
		return WMA_INVALID;
	
	if (Ptr == NULL)
		return wma_basic_alloc(Allocator, Size);

	// Find slot at pointer
	uint32_t index = 0;
	uint32_t offset = (uint32_t)Ptr - Allocator->heap_start;
	uint32_t count = *Allocator->slot_count;
	for (; index < count; ++index)
	{
		if (Allocator->slots[index].offset == offset)
			break;
	}

	if (index == count)
		return WMA_INVALID;

	// Try to extend this slot
	uint32_t grow_amount = Size - Allocator->slots[index].size;
	for (;index < count-1;) {
		Wma_Slot* next_slot = &Allocator->slots[index+1];
		
		if (next_slot->allocated)
			break;
		if (next_slot->size < grow_amount)
			break;
			
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
	wma__basic_free_slot(Allocator, index);
	return wma_basic_alloc(Allocator, Size);
}

#endif

#endif // WMA_H

//
// version 1.0 (2025.6.26)
//     Very simple allocator implemented, just to get a baseline. There are
//     definitely better ways to implement a heap allocator.
//
// Roadmap (no plans for when):
//  - Improve basic allocator
//      - allocating more pages when full
//      - binary search (free/realloc)
//      - free list
//  - Implement allocation tracking (needed?)
//  - Add another type of allocator
//  - Measure performance
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
