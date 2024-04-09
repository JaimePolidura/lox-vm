#include "x64_jit_compiler_test.h"
#include "hash_table_test.h"
#include "package_test.h"
#include "scanner_test.h"
#include "vm_test.h"
#include "trie_test.h"
#include "stack_test.h"
#include "utils_test.h"

#define RUN_HASH_TABLE_TEST
#define RUN_COMPILER_TEST
#define RUN_PACKAGE_TEST
#define RUN_TRIE_TEST
#define RUN_VM_TEST
#define RUN_STACK_TEST
#define RUN_UTILS_TEST
#define JIT_X64_TEST

extern struct trie_list * compiled_packages;
extern const char * compiling_base_dir;

//Run in debug mode
int main() {
#ifdef JIT_X64_TEST
    x64_jit_compiler_structs_get_outer();
    x64_jit_compiler_structs_set_outer();
    x64_jit_compiler_structs_initialize_outer();
    x64_jit_compiler_division_multiplication_outer();
    x64_jit_compiler_simple_expression_outer();
    x64_jit_compiler_for_loop_outer();
    x64_jit_compiler_negation_outer();
#endif

#ifdef RUN_PACKAGE_TEST
    package_import_name_to_absolute_path_test_outer();
    package_read_package_name_test_outer();
#endif

#ifdef RUN_UTILS_TEST
    utils_string_equals_ignore_case_test_outer();
    utils_string_contains_test_outer();
    utils_to_upper_case_test_outer();
#endif

#ifdef RUN_STACK_TEST
    simple_stack_push_pop_test_outer();
    simple_stack_clear_test_outer();
#endif

#ifdef RUN_VM_TEST
    compiling_base_dir = NULL;
    compiled_packages = NULL;

    vm_global_functions_test_outer();
    vm_file_global_structs_test_outer();
    vm_file_global_variables_test_outer();

    simple_vm_test_threads_gc_outer();
    simple_vm_test_threads_no_race_condition_outer();
    simple_vm_test_threads_race_condition_outer();
    simple_vm_test_threads_join_outer();
    simple_vm_test_with_structs_outer();
    simple_vm_test_with_while_outer();
    simple_vm_test_with_ifs_outer();
    simple_vm_test_with_scope_variables_outer();
    simple_vm_test_with_functions_outer();
    simple_vm_test_with_nested_functions_outer();
    simple_vm_test_with_for_loops_outer();
    simple_vm_test_inline_array_initilization_outer();
    simple_vm_test_empty_array_initilization_outer();
#endif

#ifdef RUN_HASH_TABLE_TEST
    hash_table_put_contains_remove_get_test_outer();
    hash_table_multiple_put_test_outer();
    hash_table_put_if_present_outer();
    hash_table_put_if_absent_outer();
#endif

#ifdef RUN_COMPILER_TEST
    simple_scanner_test_outer();
#endif

#ifdef RUN_TRIE_TEST
    trie_test_put_with_general_case_outer();
    trie_test_put_with_same_prefix_key_outer();
    trie_test_put_with_same_key_outer();
    trie_test_emtpy_trie_contains_outer();
    trie_test_for_each_outer();
#endif

    return 0;
}