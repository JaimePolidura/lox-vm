#pragma once

#include "../shared.h"

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ,
} lox_value_type;

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRUCT
} object_type_t;

struct object {
    object_type_t type;
    bool gc_marked;
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

bool cast_to_boolean(lox_value_t value);

struct object * allocate_object(size_t size, object_type_t type);

#define FROM_RAW_TO_OBJECT(value) ((lox_value_t){VAL_OBJ, {.object = (struct object*) value}})
#define FROM_RAW_TO_NUMBER(value) ((lox_value_t){VAL_NUMBER, {.number = value}})
#define FROM_RAW_TO_BOOL(value) ((lox_value_t){VAL_BOOL, {.boolean = value}})
#define NIL_VALUE() ((lox_value_t){VAL_NIL})

#define TO_NUMBER_RAW(value) ((value).as.number)
#define TO_BOOL_RAW(value) ((value).as.boolean)
#define TO_OBJECT_RAW(value) ((value).as.object)

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJECT(value) ((value).type == VAL_OBJ)

#define OBJECT_TYPE(value) (TO_OBJECT_RAW(value)->type)
#define ALLOCATE_OBJ(type, object_type) (type *) allocate_object(sizeof(type), object_type)