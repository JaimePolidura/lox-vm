extern void runtime_panic(char * format, ...);

void range_check_panic_jit_runime(int array_index, int array_size) {
    runtime_panic("Index %i out of bounds when array size is %i", array_index, array_size);
}