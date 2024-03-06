# Memory allocator

A program that implements the functions malloc, calloc, realloc and free in order to create a minimalistic memory allocator that can be used to manage virtual memory. It uses different strategies for small and large chunk allocations. It ensures memory alignment to 8 bytes, reusing blocks efficiently and always finding the most suitable free block.
