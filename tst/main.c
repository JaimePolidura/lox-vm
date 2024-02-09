#include "scanner_test.h"
#include "compiler_test.h"
#include "hash_table_test.h"
#include "vm_test.h"
#include "trie_test.h"

#define RUN_VM_TEST
#define RUN_HASH_TABLE_TEST
#define RUN_COMPILER_TEST
#define RUN_GLOBAL_STRING_POOL_TEST
#define RUN_TRIE_TEST

//Run in debug mode
int main() {
#ifdef RUN_GLOBAL_STRING_POOL_TEST
    init_global_string_pool();
#endif

#ifdef RUN_VM_TEST
    simple_vm_test_with_structs_outer();
    simple_vm_test_with_while_outer();
    simple_vm_test_with_ifs_outer();
    simple_vm_test_with_scope_variables_outer();
    simple_vm_test_with_functions_outer();
    simple_vm_test_with_nested_functions_outer();
    simple_vm_test_with_for_loops_outer();
#endif

#ifdef RUN_HASH_TABLE_TEST
    hash_table_put_contains_remove_get_test_outer();
    hash_table_add_all_test_outer();
    hash_table_multiple_put_test_outer();
#endif

#ifdef RUN_COMPILER_TEST
    simple_scanner_test_outer();
    simple_compiler_test_with_for_outer();
    simple_compiler_test_with_structs_outer();
    simple_compiler_test_with_scope_variables_outer();
    simple_compiler_test_with_functions_outer();
    simple_compiler_test_if_statements_outer();
    simple_compiler_test_if_while_outer();
    simple_compiler_test_outer();
#endif

#ifdef RUN_TRIE_TEST
    trie_test_put_with_general_case_outer();
    trie_test_put_with_same_prefix_key_outer();
    trie_test_put_with_same_key_outer();
    trie_test_emtpy_trie_contains_outer();
#endif

    return 0;
}