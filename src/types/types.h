#pragma once

#include "../shared.h"
#include "../memory/memory.h"

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ,
} lox_value_type;

typedef enum {
    OBJ_STRING,
} object_type_t;

struct object {
    object_type_t type;
    struct object * next; //Next object heap allocated
};

typedef struct {
  lox_value_type type;
  union {
    bool boolean;
    double number;
    struct object * object;
  } as;
} lox_value_t;

#define FROM_OBJECT(value) ((lox_value_t){VAL_OBJ, {.object = (struct object*) value}})
#define FROM_NUMBER(value) ((lox_value_t){VAL_NUMBER, {.number = value}})
#define FROM_BOOL(value) ((lox_value_t){VAL_BOOL, {.boolean = value}})
#define FROM_NIL() ((lox_value_t){VAL_NIL})

#define TO_NUMBER(value) ((value).as.number)
#define TO_BOOL(value) ((value).as.boolean)
#define TO_OBJECT(value) ((value).as.object)

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJECT(value) ((value).type == VAL_OBJ)

#define OBJECT_TYPE(value) (TO_OBJECT(value)->type)
