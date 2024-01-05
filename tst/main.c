#include "scanner_test.h"
#include "compiler_test.h"
#include "hash_table_test.h"

int main() {
    hash_table_put_contains_remove_get_test_outer();
    hash_table_add_all_test_outer();
    hash_table_multiple_put_test_outer();

    simple_scanner_test_outer();

    simple_compiler_test_outer();

    return 0;
}