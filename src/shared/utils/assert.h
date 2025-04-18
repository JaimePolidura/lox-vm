#pragma once

#include "shared.h"

//Method location format: <file name>::<method name>
void lox_assert(bool condition_to_be_true, char *, char *, ...);

void lox_assert_failed(char *, char *, ...);
