#pragma once

#include "shared.h"
#include "params.h"

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ,
} lox_value_type;

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE_FUNCTION,
    OBJ_STRUCT_INSTANCE,
    OBJ_STRUCT_DEFINITION,
    OBJ_PACKAGE,
    OBJ_ARRAY
} object_type_t;

struct object {
    object_type_t type;
    void * gc_info;
};

#define LOX_OBJECT(a) ((struct object) {.type = a, .gc_info = NULL} )

#ifdef NAN_BOXING

typedef uint64_t lox_value_t;

#define FLOAT_QNAN ((uint64_t) 0x7ffc000000000000)
#define FLOAT_SIGN_BIT ((uint64_t) 0x8000000000000000)
#define TAG_NIL 1
#define TAG_FALSE 2
#define TAG_TRUE 3

#define AS_NUMBER(value) (double) value
#define TO_LOX_VALUE_NUMBER(value) (lox_value_t) value
#define IS_NUMBER(value) ((value & FLOAT_QNAN) != FLOAT_QNAN)

#define NIL_VALUE (TAG_NIL | FLOAT_QNAN)
#define IS_NIL(value) (value == (NIL_VALUE))

#define FALSE_VALUE (lox_value_t)(FLOAT_QNAN | TAG_FALSE)
#define TRUE_VALUE (lox_value_t)(FLOAT_QNAN | TAG_TRUE)
#define AS_BOOL(value) (value == TRUE_VALUE)
#define IS_BOOL(value) (((value) | 1) == TRUE_VALUE)
#define TO_LOX_VALUE_BOOL(value) (value ? TRUE_VALUE : FALSE_VALUE)

#define TO_LOX_VALUE_OBJECT(value) (lox_value_t)(FLOAT_SIGN_BIT | FLOAT_QNAN | (uint64_t)(uintptr_t)(value))
#define AS_OBJECT(value) ((struct object *) (uintptr_t) ((value) & ~(FLOAT_SIGN_BIT | FLOAT_QNAN)))

#define IS_OBJECT(value) (((value) & (FLOAT_QNAN | FLOAT_SIGN_BIT)) == (FLOAT_QNAN | FLOAT_SIGN_BIT))

#define AS_FUNCTION(value) (struct function_object *) AS_OBJECT(value)

#else

typedef struct {
  lox_value_type type;
  union {
    bool boolean;
    double immediate;
    struct object * object;
  } value_as;
} lox_value_t;

#define AS_NUMBER(value_node) ((value_node).value_as.immediate)
#define TO_LOX_VALUE_NUMBER(value_node) ((lox_value_t){VAL_NUMBER, {.immediate = value_node}})
#define IS_NUMBER(value_node) ((value_node).type == VAL_NUMBER)

#define IS_NIL(value_node) ((value_node).type == VAL_NIL)
#define NIL_VALUE() ((lox_value_t){VAL_NIL})

#define FALSE_VALUE(value_node) ((lox_value_t){VAL_BOOL, {.boolean = false}})
#define TRUE_VALUE(value_node) ((lox_value_t){VAL_BOOL, {.boolean = true}})
#define AS_BOOL(value_node) ((value_node).value_as.boolean)
#define IS_BOOL(value_node) ((value_node).type == VAL_BOOL)
#define TO_LOX_VALUE_BOOL(value_node) ((lox_value_t){VAL_BOOL, {.boolean = value_node}})

#define TO_LOX_VALUE_OBJECT(value_node) ((lox_value_t){VAL_OBJ, {.object = (struct object*) value_node}})
#define AS_OBJECT(value_node) ((value_node).value_as.object)
#define IS_OBJECT(value_node) (value_node.type == VAL_OBJ)

#define AS_FUNCTION(value_node) (struct function_object *) (value_node.value_as.object)

#endif

typedef void (*lox_type_consumer_t)(lox_value_t);

void init_object(struct object * object, object_type_t type);

bool cast_to_boolean(lox_value_t value);

struct object * allocate_object(size_t size, object_type_t type);

char * to_string(lox_value_t value);

lox_value_type get_lox_type(lox_value_t lox_value);

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)
#define ALLOCATE_OBJ(type, object_type) (type *) allocate_object(sizeof(type), object_type)
