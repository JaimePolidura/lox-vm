#include "gc_barrier_elimination.h"

extern void lox_assert_failed(char * method_location, char * error_message, ...);

struct gcbe {
    struct lox_ir * lox_ir;
    struct arena_lox_allocator gcbe_allocator;
};

static struct gcbe * alloc_gc_barrier_elimination(struct lox_ir *);
static void free_gc_barrier_elimination(struct gcbe *);

static bool perform_gc_barrier_elimination_block(struct lox_ir_block*,void*);
static void perform_gc_barrier_elimination_control(struct gcbe*,struct lox_ir_control_node*);
static struct lox_ir_gc_write_barrier * get_gc_write_barrier(struct lox_ir_control_node*);
static bool has_gc_write_barrier(struct lox_ir_control_node*);
static lox_ir_type_t get_input_type(struct lox_ir_control_node*);

void perform_gc_barrier_elimination(struct lox_ir * lox_ir) {
    struct gcbe * gcbe = alloc_gc_barrier_elimination(lox_ir);

    for_each_lox_ir_block(
            lox_ir->first_block,
            &gcbe->gcbe_allocator.lox_allocator,
            gcbe,
            LOX_IR_BLOCK_OPT_NOT_REPEATED,
            perform_gc_barrier_elimination_block
    );

    free_gc_barrier_elimination(gcbe);
}

static bool perform_gc_barrier_elimination_block(struct lox_ir_block * current_block, void * extra) {
    struct gcbe * gcbe = extra;

    for (struct lox_ir_control_node * current_control = current_block->first;; current_control = current_control->next) {
        perform_gc_barrier_elimination_control(gcbe, current_control);

        if (current_control == current_block->last) {
            break;
        }
    }

    return true;
}

static void perform_gc_barrier_elimination_control(struct gcbe * gcbe, struct lox_ir_control_node * control) {
    if (has_gc_write_barrier(control)) {
        struct lox_ir_gc_write_barrier * wb = get_gc_write_barrier(control);
        lox_ir_type_t input_type = get_input_type(control);
        if (!is_marked_as_escaped_lox_ir_control(control) || !is_object_lox_ir_type(input_type)) {
            wb->requires_write_gc_barrier = false;
        } else if (input_type == LOX_IR_TYPE_LOX_ANY) {
            wb->requires_lox_any_type_check = true;
        } else if (is_native_lox_ir_type(input_type)) {
            wb->requires_native_to_lox_pointer_cast = true;
        }
    }
}

static struct gcbe * alloc_gc_barrier_elimination(struct lox_ir * lox_ir) {
    struct gcbe * gcbe = NATIVE_LOX_MALLOC(sizeof(struct gcbe));
    gcbe->lox_ir = lox_ir;

    struct arena arena;
    init_arena(&arena);
    gcbe->gcbe_allocator = to_lox_allocator_arena(arena);

    return gcbe;
}

static void free_gc_barrier_elimination(struct gcbe * gcbe) {
    free_arena(&gcbe->gcbe_allocator.arena);
    NATIVE_LOX_FREE(gcbe);
}

static lox_ir_type_t get_input_type(struct lox_ir_control_node * control) {
    switch (control->type) {
        case LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD:
            struct lox_ir_control_set_struct_field_node * set_field = ((struct lox_ir_control_set_struct_field_node *) control);
            return set_field->new_field_value->produced_type->type;
        case LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT:
            struct lox_ir_control_set_array_element_node * set_arr_element = ((struct lox_ir_control_set_array_element_node *) control);
            return set_arr_element->new_element_value->produced_type->type;
        default:
            lox_assert_failed("lox_ir_data_node.c::get_input_type", "Illegal control node type %i", control->type);
    }
}

static bool has_gc_write_barrier(struct lox_ir_control_node * control) {
    return control->type == LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD
           || control->type == LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT;
}

static struct lox_ir_gc_write_barrier * get_gc_write_barrier(struct lox_ir_control_node * control) {
    switch (control->type) {
        case LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD:
            return &((struct lox_ir_control_set_struct_field_node *) control)->gc_write_barrier;
        case LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT:
            return &((struct lox_ir_control_set_struct_field_node *) control)->gc_write_barrier;
        default:
            return NULL;
    }
}