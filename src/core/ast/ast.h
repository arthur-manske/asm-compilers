#ifndef DPP_AST_H
#define DPP_AST_H

#include "core/types.h"
#include "core/arena/arena.h"

enum dpp_op_kind {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_AND, OP_OR, OP_XOR, OP_SHL, OP_SHR,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_ASSIGN,
    OP_DOT, OP_ARROW,
    OP_LOGICAL_AND, OP_LOGICAL_OR,
    OP_INC, OP_DEC,
    OP_BITWISE_NOT,
    OP_CALL,
    OP_INVALID
};

enum dpp_node_kind {

	NOD_INVALID,
	NOD_TRANSLATION_UNIT,
	NOD_FUNCTION_DECL,
	NOD_VAR_DECL,
	NOD_TYPEDEF,
	NOD_PARAM_DECL,
	NOD_STRUCT_DECL,
	NOD_UNION_DECL,
	NOD_ENUM_DECL,
	NOD_ENUM_CONST,
	NOD_COMPOUND_STMT,
	NOD_RETURN_STMT,
	NOD_IF_STMT,
	NOD_WHILE_STMT,
	NOD_DO_WHILE_STMT,
	NOD_FOR_STMT,
	NOD_BREAK_STMT,
	NOD_CONTINUE_STMT,
	NOD_SWITCH_STMT,
	NOD_CASE_STMT,
	NOD_DEFAULT_STMT,
	NOD_EXPR_STMT,
	NOD_ASM_STMT,
	NOD_BINARY_EXPR,
	NOD_TERNARY_EXPR,
	NOD_UNARY_EXPR,
	NOD_INDEX_EXPR,
	NOD_CAST_EXPR,
	NOD_STMT_EXPR,
	NOD_INT_LITERAL,
	NOD_FLOAT_LITERAL,
	NOD_STRING_LITERAL,
	NOD_IDENTIFIER,
	NOD_ATTR_ALWAYS,
	NOD_ATTR_ERANGE,
	NOD_ATTR_EVAL,
	NOD_ATTR_CREATES,
	NOD_ATTR_DESTROYS,
	NOD_ATTR_INITIALIZED,
	NOD_ATTR_INITIALIZER,
	NOD_ATTR_DEINITIALIZER,
	NOD_ATTR_POSSIBLE_DEINITIALIZER,
	NOD_IMPLICIT_RET,
	NOD_SIZEOF,
	NOD_INIT_LIST,
	NOD_DESIGNATED_INIT,
	NOD_LABEL_STMT,
	NOD_GOTO_STMT,
	NOD_TYPEOF,
	NOD_DECLARATOR
};

struct dpp_evidence {
	const u8            *evi_field;
	size_t               evi_field_len;
	const u8            *evi_group;
	size_t               evi_group_len;
	const u8            *evi_pool; /* Resource pool associated with this member */
	size_t               evi_pool_len;
	const u8            *evi_by; /* Function that performed the action */
	size_t               evi_by_len;
	bool                 evi_is_init; /* true = init, false = deinit */
	struct dpp_evidence *evi_next;
};

struct dpp_node {
	enum dpp_node_kind nod_kind;

	/* Localização para erros */
	u32 nod_line;
	u32 nod_column;

	/* Dados do nó */
	union {
		struct {
			s64 val_int;
		} nod_val;
		f64 val_float;
		struct {
		        const u8 *id_name;
		        size_t    id_len;
		        enum dpp_type_kind id_type;
		} nod_id;

		struct {
		        enum dpp_op_kind op_kind;
		        struct dpp_node *op_lhs;
		        struct dpp_node *op_rhs;
		        struct dpp_node *op_cond; /* Para NOD_TERNARY_EXPR */
		} nod_op;
        struct {
            struct dpp_node *asm_outputs;
            struct dpp_node *asm_inputs;
            struct dpp_node *asm_clobbers;
        } nod_asm;
	} nod_data;

	u32 nod_ptr_depth;  /* Profundidade de ponteiros (*) */
	u32 nod_array_size; /* Tamanho se for array [] */
	u32 nod_type_flags; /* NOD_TYPE_* below */
	u32 nod_type_bits;  /* Largura em bits (específico do Mago) */
	u32 nod_storage;    /* NOD_STORAGE_* below */

	bool                 nod_is_packed;    /* __attribute__((packed)) */
	u64                  nod_attr_flags;   /* NOD_ATTR_* below */
	bool                 nod_is_variadic;  /* For functions with ... */
	bool                 nod_is_postfix;   /* For post-increment/decrement */
	u32                  nod_member_index; /* Index of the field in a struct/union */

	struct dpp_node     *nod_type_node;    /* Points to the struct/union/typedef decl node */
	void                *nod_type;         /* struct dpp_type* */
	struct dpp_node     *nod_ref;          /* For identifiers: points to the actual decl node */
	void                *nod_llvm_val;     /* LLVMValueRef (used during codegen) */
	struct dpp_evidence *nod_evidence;

	struct dpp_node *nod_next;
	struct dpp_node *nod_child;
	struct dpp_node *nod_attrs;
};

/* Type Flags */
#define NOD_TYPE_UNSIGNED (1u << 0)
#define NOD_TYPE_SIGNED   (1u << 1)
#define NOD_TYPE_CONST    (1u << 2)
#define NOD_TYPE_VOLATILE (1u << 3)
#define NOD_TYPE_RESTRICT (1u << 7)
#define NOD_TYPE_LONG     (1u << 4)
#define NOD_TYPE_SHORT    (1u << 5)
#define NOD_TYPE_TYPEDEF  (1u << 6)

/* Storage Classes */
#define NOD_STORAGE_EXTERN (1u << 0)
#define NOD_STORAGE_STATIC (1u << 1)

/* Attribute Flags (64-bit bitfield) */
#define NOD_ATTR_PURE            (1ULL << 0)
#define NOD_ATTR_CONST           (1ULL << 1)
#define NOD_ATTR_NORETURN        (1ULL << 2)
#define NOD_ATTR_MALLOC          (1ULL << 3)
#define NOD_ATTR_RETURNS_NONNULL (1ULL << 4)
#define NOD_ATTR_NONNULL         (1ULL << 5)
#define NOD_ATTR_HOT             (1ULL << 6)
#define NOD_ATTR_COLD            (1ULL << 7)
#define NOD_ATTR_ALWAYS_INLINE   (1ULL << 8)
#define NOD_ATTR_NOINLINE        (1ULL << 9)
#define NOD_ATTR_UNUSED          (1ULL << 10)
#define NOD_ATTR_DEPRECATED      (1ULL << 11)
#define NOD_ATTR_NODISCARD       (1ULL << 12)

struct dpp_node *dpp_node_new(struct dpp_arena *arena, enum dpp_node_kind kind, u32 line, u32 col);

#endif
void dpp_node_add_child(struct dpp_node *parent, struct dpp_node *child);
