#include "lox_ir_file_visualizer.h"

static void add_block_as_label_file(struct lox_ir_visualizer*, struct lox_ir_block *);
static bool generate_file_visualization_block_lox_ir(struct lox_ir_block*, void *);
static void generate_file_visualization_control_lox_ir(struct lox_ir_visualizer*, struct lox_ir_control_node *);
static char * operand_to_string(struct lox_ir_ll_operand);
static char * register_size_to_string(uint64_t);
static char * comparation_type_to_string(comparation_operator_type_ll_lox_ir);
static char * unary_operator_to_string(unary_operator_type_ll_lox_ir);
static char * binary_operator_to_string(binary_operator_type_ll_lox_ir);

typedef void(* generate_control_file_line_t)(struct lox_ir_visualizer*, struct lox_ir_control_node*);
void generate_control_file_line_move(struct lox_ir_visualizer*, struct lox_ir_control_node*);
void generate_control_file_line_comparation(struct lox_ir_visualizer*, struct lox_ir_control_node*);
void generate_control_file_line_return(struct lox_ir_visualizer*, struct lox_ir_control_node*);
void generate_control_file_line_unary(struct lox_ir_visualizer*, struct lox_ir_control_node*);
void generate_control_file_line_binary(struct lox_ir_visualizer*, struct lox_ir_control_node*);
void generate_control_file_line_func_call(struct lox_ir_visualizer*, struct lox_ir_control_node*);

generate_control_file_line_t line_generator_by_control_type[] = {
        [LOX_IR_CONTROL_NODE_LL_COMPARATION] = generate_control_file_line_comparation,
        [LOX_IR_CONTROL_NODE_LL_FUNCTION_CALL] = generate_control_file_line_func_call,
        [LOX_IR_CONTROL_NODE_LL_RETURN] = generate_control_file_line_return,
        [LOX_IR_CONTROL_NODE_LL_BINARY] = generate_control_file_line_binary,
        [LOX_IR_CONTROL_NODE_LL_UNARY] = generate_control_file_line_unary,
        [LOX_IR_CONTROL_NODE_LL_MOVE] = generate_control_file_line_move,

        [LOX_IR_CONTROL_NODE_LL_COND_FUNCTION_CALL] = NULL,
};

void generate_file_visualization_lox_ir(
        struct lox_ir_visualizer * lox_ir_visualizer,
        struct lox_ir_block * first_block
) {
    for_each_lox_ir_block(
            first_block,
            NATIVE_LOX_ALLOCATOR(),
            lox_ir_visualizer,
            LOX_IR_BLOCK_OPT_NOT_REPEATED,
            generate_file_visualization_block_lox_ir
    );
}

static bool generate_file_visualization_block_lox_ir(struct lox_ir_block * block, void * extra) {
    struct lox_ir_visualizer * lox_ir_visualizer = extra;

    for (struct lox_ir_control_node * current_node = block->first;; current_node = current_node->next) {
        generate_file_visualization_control_lox_ir(lox_ir_visualizer, current_node);

        if (block->last == current_node) {
            break;
        }
    }

    return true;
}

static void generate_file_visualization_control_lox_ir(
        struct lox_ir_visualizer * visualizer,
        struct lox_ir_control_node * control_node
) {
    generate_control_file_line_t line_generator = line_generator_by_control_type[control_node->type];

    if (line_generator == NULL) {
        //TODO panic
    }

    line_generator(visualizer, control_node);
}

void generate_control_file_line_func_call(
        struct lox_ir_visualizer *,
        struct lox_ir_control_node * control
) {
    struct lox_ir_control_ll_function_call * func_call = (struct lox_ir_control_ll_function_call *) control;
    struct string_builder function_call_string_builder;
    init_string_builder(&function_call_string_builder, NATIVE_LOX_ALLOCATOR());

    append_string_builder(&function_call_string_builder, "call ");
    append_string_builder(&function_call_string_builder, get_function_name_by_ptr(func_call->function_call_address));
    append_string_builder(&function_call_string_builder, "(");

    for(int i = 0; i < func_call->arguments.in_use; i++){
        struct lox_ir_ll_operand * argument = func_call->arguments.values[i];
        char * argument_string = operand_to_string(*argument);

        append_string_builder(&function_call_string_builder, argument_string);
        if (i + 1 < func_call->arguments.in_use) {
            append_string_builder(&function_call_string_builder, ", ");
        }
    }

    append_string_builder(&function_call_string_builder, ")");

    //TODO
}

void generate_control_file_line_binary(
        struct lox_ir_visualizer * visualizer,
        struct lox_ir_control_node * control
) {
    struct lox_ir_control_ll_binary * binary = (struct lox_ir_control_ll_binary *) control;
    char * operator_string = binary_operator_to_string(binary->operator);
    char * result_string = operand_to_string(binary->result);
    char * right_string = operand_to_string(binary->right);
    char * left_string = operand_to_string(binary->left);

    add_new_line_lox_ir_visualizer(visualizer, dynamic_format_string("\t%s %s, %s, %s",
        operator_string, result_string, left_string, right_string));
}

void generate_control_file_line_unary(
        struct lox_ir_visualizer * visualizer,
        struct lox_ir_control_node * control
) {
    struct lox_ir_control_ll_unary * unary = (struct lox_ir_control_ll_unary *) control;
    char * operator_string = unary_operator_to_string(unary->operator);
    char * operand_string = operand_to_string(unary->operand);
    add_new_line_lox_ir_visualizer(visualizer, dynamic_format_string("\t%s %s", operator_string, operand_string));
}

void generate_control_file_line_return(
        struct lox_ir_visualizer * visualizer,
        struct lox_ir_control_node * control
) {
    struct lox_ir_control_ll_return * ret = (struct lox_ir_control_ll_return *) control;
    char * operator_returned_string = !ret->empty_return ? operand_to_string(ret->to_return) : "";
    add_new_line_lox_ir_visualizer(visualizer, dynamic_format_string("\tret %s", operator_returned_string));
}

void generate_control_file_line_comparation(
        struct lox_ir_visualizer * visualizer,
        struct lox_ir_control_node * control
) {
    struct lox_ir_control_ll_comparation * comparation = (struct lox_ir_control_ll_comparation *) control;
    char * comparation_type = comparation_type_to_string(comparation->comparation_operator);
    char * right = operand_to_string(comparation->right);
    char * left = operand_to_string(comparation->left);

    add_new_line_lox_ir_visualizer(visualizer, dynamic_format_string("\tcmp %s, %s, %s", left, right, comparation_type));
}

void generate_control_file_line_move(struct lox_ir_visualizer * visualizer, struct lox_ir_control_node * control) {
    struct lox_ir_control_ll_move * move = (struct lox_ir_control_ll_move *) control;
    char * from = operand_to_string(move->from);
    char * to = operand_to_string(move->to);

    add_new_line_lox_ir_visualizer(visualizer, dynamic_format_string("\tmove %s, %s", to, from));
}

static char * operand_to_string(struct lox_ir_ll_operand operand) {
    switch (operand.type) {
        case LOX_IR_LL_OPERAND_IMMEDIATE: {
            char * operator_prefix = operand.immedate < 0 ? "-" : "";
            return dynamic_format_string("%s%ll", operator_prefix, operand.immedate);
        }
        case LOX_IR_LL_OPERAND_FLAGS: return "flags";
        case LOX_IR_LL_OPERAND_REGISTER: {
            char * reg_size_string = register_size_to_string(operand.v_register.register_bit_size);
            char * fp_reg_string = operand.v_register.is_float_register ? "#" : "";
            return dynamic_format_string("v%s%i %s", operand.v_register.number, fp_reg_string, reg_size_string);
        }
        case LOX_IR_LL_OPERAND_MEMORY_ADDRESS: {
            char * address_string = operand_to_string(*operand.memory_address.address);
            if (operand.memory_address.offset > 0) {
                return dynamic_format_string("[%s %llu]", address_string, operand.memory_address.offset);
            } else {
                return address_string;
            }
        }
        case LOX_IR_LL_OPERAND_STACK_SLOT: {
            char * stack_slot_string = dynamic_format_string("StackSlot(%i)", operand.stack_slot.slot_index);

            if(operand.stack_slot.offset > 0) {
                return dynamic_format_string("[%s %llu]", stack_slot_string, operand.memory_address.offset);
            } else {
                return stack_slot_string;
            }
        }
    }
}

static void add_block_as_label_file(struct lox_ir_visualizer * visualizer, struct lox_ir_block * block) {
    if (!contains_u64_hash_table(&visualizer->labels_id_by_block, (uint64_t) block)) {
        int new_label_id = visualizer->next_label_id++;
        put_u64_hash_table(&visualizer->labels_id_by_block, (uint64_t) block, (void *) new_label_id);

        add_new_line_lox_ir_visualizer(visualizer, dynamic_format_string("L%i:", new_label_id));
    }
}


static char * register_size_to_string(uint64_t reg_size) {
    if(reg_size == 64){
        return "";
    } else if(reg_size == 32) {
        return "DWORD";
    } else if(reg_size == 16) {
        return "WORD";
    } else if(reg_size == 8) {
        return "BYTE";
    } else {
        //TODO Panic
    }
}

static char * unary_operator_to_string(unary_operator_type_ll_lox_ir operator_type) {
    switch (operator_type) {
        case UNARY_LL_LOX_IR_LOGICAL_NEGATION: return "not";
        case UNARY_LL_LOX_IR_NUMBER_NEGATION: return "neg";
        case UNARY_LL_LOX_IR_F64_TO_I64_CAST: return "fp2int";
        case UNARY_LL_LOX_IR_I64_TO_F64_CAST: return "int2fp";
        default: //TODO Panic
    }
}

static char * comparation_type_to_string(comparation_operator_type_ll_lox_ir comparation_type) {
    switch (comparation_type) {
        case COMPARATION_LL_LOX_IR_EQ: return "==";
        case COMPARATION_LL_LOX_IR_NOT_EQ: return "!=";
        case COMPARATION_LL_LOX_IR_GREATER: return ">";
        case COMPARATION_LL_LOX_IR_GREATER_EQ: return ">=";
        case COMPARATION_LL_LOX_IR_LESS: return "<";
        case COMPARATION_LL_LOX_IR_LESS_EQ: return "<=";
        case COMPARATION_LL_LOX_IR_IS_TRUE: return "is true";
        case COMPARATION_LL_LOX_IR_IS_FALSE: return "is false";
        default: return NULL; //TODO Panic
    }
}

static char * binary_operator_to_string(binary_operator_type_ll_lox_ir binary_operator) {
    switch (binary_operator) {
        case BINARY_LL_LOX_IR_XOR: return "xor";
        case BINARY_LL_LOX_IR_AND: return "and";
        case BINARY_LL_LOX_IR_OR: return "or";
        case BINARY_LL_LOX_IR_MOD: return "mod";
        case BINARY_LL_LOX_IR_LEFT_SHIFT: return "<<";
        case BINARY_LL_LOX_IR_RIGHT_SHIFT: return ">>";
        case BINARY_LL_LOX_IR_ISUB: return "isub";
        case BINARY_LL_LOX_IR_FSUB: return "fsub";
        case BINARY_LL_LOX_IR_IADD: return "iadd";
        case BINARY_LL_LOX_IR_FADD: return "fadd";
        case BINARY_LL_LOX_IR_IMUL: return "imul";
        case BINARY_LL_LOX_IR_FMUL: return "fmul";
        case BINARY_LL_LOX_IR_IDIV: return "idiv";
        case BINARY_LL_LOX_IR_FDIV: return "fdiv";
    }
}