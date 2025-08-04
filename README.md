# Wasm Memory Allocator (WMA)
Web assembly does not have a builtin allocator for general memory allocation.
The best that is offered is `__builtin_wasm_memory_grow` or to use Emscriptem (🤢).
This single-header library is provided to serve as a drop-in replacement for malloc/free for WASM.

## Usage
Include [wma.h](https://github.com/lazergenixdev/WasmMemoryAllocator/blob/main/wma.h) and define `WMA_IMPLEMENTATION` in one compilation unit (one .c/.cpp file).

✅ `wma_malloc`
✅ `wma_realloc`
✅ `wma_free`

```c
int* array = wma_malloc(10 * sizeof(int));
// ...
array = wma_realloc(array, 20 * sizeof(int));
// ...
wma_free(array);
```

## Example
[https://lazergenixdev.github.io/WasmMemoryAllocator/example/](https://lazergenixdev.github.io/WasmMemoryAllocator/example/)