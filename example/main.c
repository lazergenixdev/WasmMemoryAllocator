extern void __wasm_console_log(const char* message, int value);
extern void __wasm_panic(const char* desc, const char* message, const char* file, int line);
#define __debug_print(EXPR) __wasm_console_log(#EXPR, (int)(EXPR))

#define WMA_IMPLEMENTATION
#include "../wma.h"

Wma_Fixed_Heap global_heap;

void* malloc(size_t size)
{
	void* ptr = wma_fixed_alloc(&global_heap, size);
	__wasm_console_log("malloc", (int)ptr);
	return ptr;
}

void init()
{
	wma_create_fixed_heap(&global_heap, WMA_MB(100));
}

int say_hi()
{
	char* message = malloc(100);
	message[0] = 'H';
	message[1] = 'e';
	message[2] = 'y';
	message[3] = '!';
	message[4] = '\0';
	__wasm_console_log(message, 0);
	
	char* message2 = malloc(100);
	message2[0] = 'O';
	message2[1] = 'v';
	message2[2] = 'e';
	message2[3] = 'r';
	message2[4] = 'r';
	message2[5] = 'a';
	message2[6] = 't';
	message2[7] = 'e';
	message2[8] = 'd';
	message2[9] = '\0';
	__wasm_console_log(message2, 0);
	return 1;
}
