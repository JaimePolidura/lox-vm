#include "strength_reduction.h"

struct sr {
    struct arena_lox_allocator sr_allocator;
    struct lox_ir * lox_ir;
};

static struct sr alloc_strength_reduction(struct lox_ir *);
static void free_strength_reduction(struct sr *);
static bool perform_strength_reduction_block(struct lox_ir_block *, void *);
static bool perform_strength_reduction_data_node(struct lox_ir_data_node *, void **, struct lox_ir_data_node *, void *);
static bool is_node_power_of_two(struct lox_ir_data_node*);
static void reduce_multiplication_strength_into_multiple_shifts(struct sr*, struct decomposed_power_two,
                                                                struct lox_ir_data_binary_node*, struct lox_ir_data_node*, struct lox_ir_data_node*);

typedef void (*strength_reduction_transformer_t) (struct lox_ir_data_binary_node *, struct sr *);
static void modulo_strength_reduction_transformer(struct lox_ir_data_binary_node *, struct sr *);
static void check_even_strength_reduction_transformer(struct lox_ir_data_binary_node *, struct sr *);
static void division_strength_reduction_transformer(struct lox_ir_data_binary_node *, struct sr *);
static void multiplication_strength_reduction_transformer(struct lox_ir_data_binary_node *, struct sr *);

strength_reduction_transformer_t transformation_by_binary_op[] = {
        [OP_ADD] = NULL,
        [OP_SUB] = NULL,
        [OP_MUL] = &multiplication_strength_reduction_transformer,
        [OP_DIV] = &division_strength_reduction_transformer,
        [OP_GREATER] = NULL,
        [OP_BINARY_OP_AND] = NULL,
        [OP_BINARY_OP_OR] = NULL,
        [OP_LEFT_SHIFT] = NULL,
        [OP_RIGHT_SHIFT] = NULL,
        [OP_LESS] = NULL,
        [OP_MODULO] = &modulo_strength_reduction_transformer,
        [OP_EQUAL] = &check_even_strength_reduction_transformer,
};

void perform_strength_reduction(struct lox_ir *lox_ir) {
    struct sr strength_reduction = alloc_strength_reduction(lox_ir);

    for_each_lox_ir_block(
            lox_ir->first_block,
            &strength_reduction.sr_allocator.lox_allocator,
            &strength_reduction,
            LOX_IR_BLOCK_OPT_REPEATED,
            perform_strength_reduction_block
    );

    free_strength_reduction(&strength_reduction);
}

static bool perform_strength_reduction_block(struct lox_ir_block * block, void * extra) {
    struct sr * sr = extra;

    for (struct lox_ir_control_node * current_control_node = block->first;; current_control_node = current_control_node->next) {
        for_each_data_node_in_lox_ir_control(
                current_control_node,
                sr,
                LOX_IR_DATA_NODE_OPT_PRE_ORDER,
                perform_strength_reduction_data_node
        );

        if (current_control_node == block->last) {
            break;
        }
    }

    return true;
}

static bool perform_strength_reduction_data_node(
        struct lox_ir_data_node * _,
        void ** __,
        struct lox_ir_data_node * current_data_node,
        void * extra
) {
    struct sr * sr = extra;

    if (current_data_node->type == LOX_IR_DATA_NODE_BINARY) {
        struct lox_ir_data_binary_node * binary = (struct lox_ir_data_binary_node *) current_data_node;
        strength_reduction_transformer_t transformer = transformation_by_binary_op[binary->operator];
        if (transformer != NULL) {
            transformer(binary, sr);
        }
    }

    return true;
}

//a:  n % 2 -> n & 0x01 == 0
static void check_even_strength_reduction_transformer(
        struct lox_ir_data_binary_node * binary_op,
        struct sr * sr
) {
    struct lox_ir_data_binary_node * left_binary = (struct lox_ir_data_binary_node *) binary_op->left;

    if(binary_op->right->type == LOX_IR_DATA_NODE_CONSTANT &&
       binary_op->left->type == LOX_IR_DATA_NODE_BINARY &&
        left_binary->operator == OP_MODULO &&
       left_binary->right->type == LOX_IR_DATA_NODE_CONSTANT &&
       AS_NUMBER(GET_CONST_VALUE_LOX_IR_NODE(left_binary->right)) == 2)
    {
        lox_ir_type_t right_type = binary_op->right->produced_type->type;

        left_binary->operator = OP_BINARY_OP_AND;

        binary_op->right = &create_lox_ir_const_node(right_type, TO_LOX_VALUE_NUMBER(0), NULL,
                                                     LOX_IR_ALLOCATOR(sr->lox_ir))->data;
        left_binary->right = &create_lox_ir_const_node(right_type, TO_LOX_VALUE_NUMBER(1), NULL,
                                                       LOX_IR_ALLOCATOR(sr->lox_ir))->data;
    }
}

//  n * 2 -> n << 1; Where constant number is not f64 type
//  n * 5 -> n << 2 + m; Where constant number is not f64 type
// case a: n * 5
// case b: 5 * n
static void multiplication_strength_reduction_transformer(
        struct lox_ir_data_binary_node * binary_op,
        struct sr * sr
) {
    bool is_multiplication_a = binary_op->right->type == LOX_IR_DATA_NODE_CONSTANT &&
                               IS_I64_LOX_IR_TYPE(binary_op->right->produced_type->type) &&
                               binary_op->left->produced_type->type != LOX_IR_TYPE_F64 &&
                               AS_NUMBER(GET_CONST_VALUE_LOX_IR_NODE(binary_op->right)) > 8.0;
    bool is_multiplication_b = binary_op->left->type == LOX_IR_DATA_NODE_CONSTANT &&
                               IS_I64_LOX_IR_TYPE(binary_op->right->produced_type->type) &&
                               binary_op->right->produced_type->type != LOX_IR_TYPE_F64 &&
                               AS_NUMBER(GET_CONST_VALUE_LOX_IR_NODE(binary_op->left)) > 8.0;

    if(is_multiplication_a || is_multiplication_b) {
        struct lox_ir_data_node * multipled_variable_node = is_multiplication_a ? binary_op->left : binary_op->right;
        struct lox_ir_data_node * multipled_number_node = is_multiplication_a ? binary_op->right : binary_op->left;

        int multipled_number = AS_NUMBER(GET_CONST_VALUE_LOX_IR_NODE(multipled_number_node));
        struct decomposed_power_two decomposed_power_two = decompose_power_of_two(multipled_number);

        if(decomposed_power_two.n_exponents == 1 && decomposed_power_two.remainder == 0){
            binary_op->operator = OP_LEFT_SHIFT;
            binary_op->right = &create_lox_ir_const_node(decomposed_power_two.exponents[0],
                                                         multipled_number_node->produced_type->type, NULL,
                                                         LOX_IR_ALLOCATOR(sr->lox_ir))->data;
            binary_op->left = multipled_variable_node;

        } else if (decomposed_power_two.n_exponents > 0) {
            reduce_multiplication_strength_into_multiple_shifts(sr, decomposed_power_two, binary_op,
                multipled_variable_node, multipled_number_node);
        }

        NATIVE_LOX_FREE(decomposed_power_two.exponents);
    }
}

//  m / n -> m >> k Where n is power of 2 and m is not f64:
static void division_strength_reduction_transformer(
        struct lox_ir_data_binary_node * binary_op,
        struct sr * sr
) {
    if (binary_op->right->type == LOX_IR_DATA_NODE_CONSTANT &&
        is_node_power_of_two(binary_op->left) &&
            binary_op->right->produced_type->type != LOX_IR_TYPE_F64
    ) {
        binary_op->operator = OP_RIGHT_SHIFT;
    }
}

//  m % n -> m & (n - 1) Where n is power of 2:
static void modulo_strength_reduction_transformer(
        struct lox_ir_data_binary_node * binary_op,
        struct sr * sr
) {
    if (binary_op->right->type == LOX_IR_DATA_NODE_CONSTANT && is_node_power_of_two(binary_op->right)) {
        double constant_right = AS_NUMBER(GET_CONST_VALUE_LOX_IR_NODE(binary_op->right));
        binary_op->operator = OP_BINARY_OP_AND;
        binary_op->right = &create_lox_ir_const_node(constant_right - 1, binary_op->right->produced_type->type,
                                                     NULL, LOX_IR_ALLOCATOR(sr->lox_ir))->data;
    }
}

static struct sr alloc_strength_reduction(struct lox_ir * lox_ir) {
    struct arena arena;
    init_arena(&arena);

    return (struct sr) {
        .sr_allocator = to_lox_allocator_arena(arena),
        .lox_ir = lox_ir
    };
}

static void free_strength_reduction(struct sr * sr) {
    free_arena(&sr->sr_allocator.arena);
}

static bool is_node_power_of_two(struct lox_ir_data_node * const_node) {
    return const_node->type == LOX_IR_DATA_NODE_CONSTANT &&
           is_double_power_of_2(AS_NUMBER(GET_CONST_VALUE_LOX_IR_NODE(const_node)));
}

//decomposed_power_two contains at least 1 exponent
static void reduce_multiplication_strength_into_multiple_shifts(
        struct sr * sr,
        struct decomposed_power_two decomposed_power_two,
        struct lox_ir_data_binary_node * binary_mul_op,
        struct lox_ir_data_node * multipled_variable_node,
        struct lox_ir_data_node * multipled_number_node
) {
    //We fill shift_nodes with binary shift nodes
    struct ptr_arraylist shift_nodes;
    init_ptr_arraylist(&shift_nodes, NATIVE_LOX_ALLOCATOR());
    for (int i = 0; i < decomposed_power_two.n_exponents; i++) {
        int power_of_two = decomposed_power_two.exponents[i];

        struct lox_ir_data_binary_node * binary = ALLOC_LOX_IR_DATA(
                LOX_IR_DATA_NODE_BINARY, struct lox_ir_data_binary_node, NULL, LOX_IR_ALLOCATOR(sr->lox_ir)
        );
        binary->operator = OP_LEFT_SHIFT;
        binary->left = multipled_variable_node;
        binary->right = &create_lox_ir_const_node(power_of_two, multipled_number_node->produced_type->type, NULL,
                                                  LOX_IR_ALLOCATOR(sr->lox_ir))->data;
        append_ptr_arraylist(&shift_nodes, binary);
    }

    //Head node wil contain the new multiplication tree
    struct lox_ir_data_binary_node * head_node = NULL;
    for(int i = 0; i < shift_nodes.in_use; i++){
        struct lox_ir_data_binary_node * shift_node = shift_nodes.values[i];
        if (i > 0) {
            struct lox_ir_data_binary_node * add = ALLOC_LOX_IR_DATA(
                    LOX_IR_DATA_NODE_BINARY, struct lox_ir_data_binary_node, NULL, LOX_IR_ALLOCATOR(sr->lox_ir)
            );
            add->operator = OP_ADD;
            add->left = &shift_node->data;
            add->right = &head_node->data;

            head_node = add;
        } else if(head_node == NULL) {
            head_node = shift_node;
        }
    }
    if (decomposed_power_two.remainder != 0) {
        struct lox_ir_data_binary_node * add = ALLOC_LOX_IR_DATA(
                LOX_IR_DATA_NODE_BINARY, struct lox_ir_data_binary_node, NULL, LOX_IR_ALLOCATOR(sr->lox_ir)
        );
        add->operator = OP_ADD;
        add->left = &head_node->data;
        add->right = &create_lox_ir_const_node(decomposed_power_two.remainder,
                                               multipled_number_node->produced_type->type,
                                               NULL, LOX_IR_ALLOCATOR(sr->lox_ir))->data;
        head_node = add;
    }

    //We will replace binary_op with head_node
    binary_mul_op->operator = head_node->operator;
    binary_mul_op->right = head_node->right;
    binary_mul_op->left = head_node->left;
}