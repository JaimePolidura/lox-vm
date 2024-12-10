#include "u64_hash_table_test.h"
#include "mark_bitmap_test.h"
#include "hash_table_test.h"
#include "vm_inline_test.h"
#include "package_test.h"
#include "scanner_test.h"
#include "vm_jit_test.h"
#include "stack_test.h"
#include "utils_test.h"
#include "trie_test.h"
#include "ssa/ssa_creation_test.h"
#include "ssa/ssa_node_test.h"
#include "vm_test.h"
#include "u8_set_test.h"
#include "u8_hash_table.h"

//#define RUN_U64_HASH_TABLE_TEST
//#define RUN_MARK_BITMAP_TEST
//#define RUN_HASH_TABLE_TEST
//#define RUN_VM_INLINE_TEST
//#define RUN_COMPILER_TEST
//#define RUN_U8_HASH_TABLE
//#define RUN_PACKAGE_TEST
//#define RUN_VM_JIT_TEST
//#define RUN_U8_SET_TEST
//#define RUN_UTILS_TEST
//#define RUN_STACK_TEST
//#define RUN_TRIE_TEST
//#define RUN_SSA_CREATION_TEST
//#define RUN_SSA_NODES_TEST
#define RUN_VM_TEST

extern struct trie_list * compiled_packages;
extern const char * compiling_base_dir;

//Run in debug mode
int main() {
    init_global_string_pool();

#ifdef RUN_U8_HASH_TABLE
    u8_hash_table_test_add_contains_outer();
#endif
#ifdef RUN_U8_SET_TEST
    u8_set_test_intersection_outer();
    u8_set_test_difference_outer();
    u8_set_test_union_outer();
    u8_set_test_remove_outer();
    u8_set_test_add_contains_outer();
#endif
#ifdef RUN_SSA_NODES_TEST
    ssa_block_dominates_outer();
    ssa_data_node_is_eq_outer();
    ssa_data_node_hash_outer();
#endif
#ifdef RUN_SSA_CREATION_TEST
    ssa_creation_licm_outer();
//    ssa_creation_sr_outer();
//    ssa_creation_cse_outer();
//    ssa_creation_nested_loop_outer();
//    ssa_creation_scp_outer();
//    ssa_creation_phis_inserter_and_optimizer_outer();
//    ssa_creation_no_phis_outer();
#endif
#ifdef RUN_MARK_BITMAP_TEST
    multiple_mark_bitmap_test_outer();
    mark_bitmap_test_simple_outer();
#endif
#ifdef RUN_VM_INLINE_TEST
    vm_inline_monitor_test_outer();
    vm_inline_multiple_calls_outer();
    vm_inline_packages_test_outer();
    vm_inline_multiple_returns_test_outer();
    vm_inline_if_test_outer();
    vm_inline_for_loop_test_outer();
    vm_inline_simple_function_test_outer();
#endif
#ifdef RUN_VM_JIT_TEST
    vm_jit_native_functions_test_outer();
    vm_jit_functions_test_outer();
    vm_jit_struct_test_outer();
    vm_jit_arrays_test_outer();
    vm_jit_globals_test_outer();
    vm_jit_monitors_test_outer();
    vm_jit_for_loop_test_outer();
    vm_jit_if_test_outer();
    vm_jit_simple_function_test_outer();
#endif
#ifdef RUN_U64_HASH_TABLE_TEST
    u64_hash_table_remove_outer();
    u64_hash_table_iterator_outer();
    u64_hash_table_emtpy_iterator_outer();
    u64_hash_table_put_contains_test_outer();
    u64_hash_table_put_get_test_outer();
    u64_hash_table_grow_outer();
#endif
#ifdef RUN_PACKAGE_TEST
    package_import_name_to_absolute_path_test_outer();
    package_read_package_name_test_outer();
#endif
#ifdef RUN_UTILS_TEST
    utils_string_builder_test_outer();
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

    #ifdef USING_GEN_GC_ALG
//    simple_vm_test_barriers_outer();
//    simple_vm_test_major_gc_outer();
    #endif

//    simple_vm_test_gc_old_gen_outer();
//    simple_vm_test_gc_local_objects_may_be_moved_outer();
//    simple_vm_test_gc_global_objects_may_be_moved_outer();
//    vm_global_functions_test_outer();
//    vm_file_global_structs_test_outer();
//    vm_file_global_variables_test_outer();
//    simple_vm_test_threads_gc_outer();
//    simple_vm_test_threads_no_race_condition_outer();
//    simple_vm_test_threads_race_condition_outer();
//    simple_vm_test_threads_join_outer();
//    simple_vm_test_with_structs_outer();
//    simple_vm_test_with_while_outer();
//    simple_vm_test_with_ifs_outer();
//    simple_vm_test_with_scope_variables_outer();
//    simple_vm_test_with_functions_outer();
//    simple_vm_test_with_nested_functions_outer();
//    simple_vm_test_with_for_loops_outer();
    simple_vm_test_inline_array_length_outer();
//    simple_vm_test_inline_array_initilization_outer();
//    simple_vm_test_empty_array_initilization_outer();
//    simple_vm_test_const_outer();
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