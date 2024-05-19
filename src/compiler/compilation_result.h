#pragma once

struct compilation_result {
    struct package * compiled_package;
    char * error_message;
    int local_count;
    bool success;
};