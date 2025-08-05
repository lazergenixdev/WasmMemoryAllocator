extern void __wasm_console_log(const char* message, int value);
extern void __wasm_panic(const char* topic, const char* what, const char* file, int line);
#define __debug_print(EXPR) __wasm_console_log(#EXPR, (int)(EXPR))

// Optional: Define wma__panic (in implementation file)
//           to display message on fatal errors
#define wma__panic(TOPIC,WHAT,LINE) __wasm_panic(TOPIC, WHAT, __FILE__, LINE)

// Optional: Define which allocator to use
#define WMA_ALLOCATOR fast


// 1. Header-Only library, just include,
//     and define WMA_IMPLEMENTATION in at
//     least one file.
#define WMA_IMPLEMENTATION
#include "../wma.h"


// 2. alloc/free functions act as a drop-in
//     replacement for malloc and free!
void* realloc(void* ptr, size_t size)
{
	return wma_realloc(ptr, size);
}
void* malloc(size_t size)
{
	return wma_alloc(size);
}
void free(void* ptr)
{
	wma_free(ptr);
}


//(4) Data structure is fully available,
//     so you can see all allocations and
//     their sizes.
#if WMA_USING_ALLOCATOR(fast)
int heap_size()
{
	return (int)wma_allocator.available_size;
}
int heap_start()
{
	return (int)wma_allocator.heap_start;
}
int allocation_count()
{
	return (int)wma_allocator.slot_count;
}
int allocation_size(int index)
{
	return (int)wma_allocator.slots[index].size;
}
int allocation_status(int index)
{
	return wma_allocator.slots[index].allocated;
}
int allocation_offset(int index)
{
	return wma_allocator.slots[index].offset;
}
#else
int heap_size() { return 0; }
int heap_start() { return 0; }
int allocation_count() { return 0; }
int allocation_size(int index) { return 0; }
int allocation_status(int index) { return 0; }
int allocation_offset(int index) { return 0; }
#endif

#define DECLARE_ARRAY(TYPE) \
typedef struct { \
	size_t len; \
	size_t cap; \
	TYPE*  data; \
} Array_ ## TYPE; \
void grow_##TYPE(Array_ ## TYPE* array, size_t amount) \
{ \
	if (array->cap >= array->len + amount) \
		return; \
	array->cap = (array->len + amount + 1) * 3 / 2; \
	array->data = realloc(array->data, sizeof(TYPE) * array->cap); \
} \
void push_##TYPE(Array_ ## TYPE* array, TYPE value) \
{ \
	grow_##TYPE(array, 1); \
	array->data[array->len++] = value; \
} \
void pop_##TYPE(Array_ ## TYPE* array) \
{ \
	array->len -= 1; \
}

DECLARE_ARRAY(int)
DECLARE_ARRAY(Array_int)

Array_Array_int arrays = {0};

void push(void)
{
	push_Array_int(&arrays, (Array_int) {0});
}
void pop(void)
{
	if (arrays.len == 0)
		return;
	free(arrays.data[arrays.len-1].data);
	pop_Array_int(&arrays);
}
int count(void)
{
	return arrays.len;
}

void array_push(int index, int value)
{
	push_int(&arrays.data[index], value);
}
void array_pop(int index)
{
	if (arrays.data[index].len == 0)
		return;
	pop_int(&arrays.data[index]);
}
size_t total_count(void)
{
	size_t sum = 0;
	for (size_t i = 0; i < arrays.len; ++i)
		sum += arrays.data[i].len;
	return sum;
}

void test()
{
	const int N = 1000;

	Array_int array = {0};
	for (int i = 1; i <= N; ++i) {
		push_int(&array, i);

		int sum = 0;
		for (int i = 0; i < array.len; ++i)
			sum += array.data[i];
		
		int n = i;
		__debug_print(array.data);
		__debug_print(n * (n+1) / 2);
		__debug_print(sum);

		wma__assert(n * (n+1) / 2 == sum);
	}
}
