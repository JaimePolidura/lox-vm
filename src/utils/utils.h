#pragma once

#include "shared.h"

char * read_file_as_text(char * path, int length_path);
char * to_lower_case(char * key, int length);
bool string_contains(char * string, int length, char to_check);
char * relative_import_path_to_absolute(char * import_path, int import_path_length, char * base_compilation_path);