#ifndef PUNCTA_H_
#define PUNCTA_H_
#include <math.h>
#include "coc.h"

typedef struct Number {
    long long int_value;
    double float_value;
    bool is_float;
} Number;

typedef enum TokenKind{
    tok_eof        = -1,
    tok_identifier = -2,
    tok_number     = -3
} TokenKind;

typedef struct Token {
    TokenKind kind;
    Coc_String text;
    Number number;
} Token;

typedef struct Lexer {
    Coc_String src;
    size_t pos;
} Lexer;

static inline char lexer_peek(Lexer *l) {
    if (l->pos >= l->src.size) return '\0';
    return l->src.items[l->pos];
}

static inline char lexer_peek2(Lexer *l) {
    if (l->pos + 1 >= l->src.size) return '\0';
    return l->src.items[l->pos + 1];
}

static inline char lexer_get(Lexer *l) {
    if (l->pos >= l->src.size) return '\0';
    return l->src.items[l->pos++];
}

static inline void skip_whitespace(Lexer *l) {
    while (isspace(lexer_peek(l))) l->pos++;
}

static inline Token lexer_next(Lexer *l) {
    skip_whitespace(l);
    char c = lexer_peek(l);
    if (c == '\0') return (Token){.kind = tok_eof};
    if (isalpha(c) || c == '_') {
        Coc_String id = {0};
        coc_vec_append(&id, lexer_get(l));
        while (isalnum(lexer_peek(l)) || lexer_peek(l) == '_') {
            coc_vec_append(&id, lexer_get(l));
        }
        return (Token){.kind = tok_identifier, .text = id};
    }
    if (isdigit(c)) {
        Coc_String num = {0};
        bool is_float = false;
        while (isdigit(lexer_peek(l))) {
            coc_vec_append(&num, lexer_get(l));
        }
        if (lexer_peek(l) == '.' && isdigit(lexer_peek2(l))) {
            is_float = true;
            coc_vec_append(&num, lexer_get(l));
            while (isdigit(lexer_peek(l))) {
                coc_vec_append(&num, lexer_get(l));
            }
        }
        coc_str_append_null(&num);
        char *end_ptr;
        errno = 0;
        if (is_float) {
            double float_val = strtod(num.items, &end_ptr);
            return (Token){
                .kind = tok_number,
                .number = (Number){
                    .float_value = float_val,
                    .is_float = true
                }
            };
        } else {
            long long int_val = strtoll(num.items, &end_ptr, 10);
            return (Token){
                .kind = tok_number,
                .number = (Number){
                    .int_value = int_val,
                    .is_float = false
                }
            };
        }
    }
    char ch = lexer_get(l);
    return (Token){.kind = (TokenKind)ch};
}

typedef enum OpCode {
    OP_ASSIGN,
    OP_ACT,
    OP_JEQ,
    OP_JMP,
    OP_LABEL
} OpCode;

typedef struct Instruction {
    OpCode op;
    Coc_String OperandA;
    Coc_String OperandB;
    Coc_String Label;
    Number number;
    bool is_B_number;
} Instruction;

typedef struct Program {
    Instruction *items;
    size_t size;
    size_t capacity;
} Program;

typedef struct LabelEntry {
    Coc_String key;
    int value;
    bool is_used;
} LabelEntry;

typedef struct LabelHashTable {
    LabelEntry *items;
    LabelEntry *new_items;
    size_t size;
    size_t capacity;
} LabelHashTable;

typedef struct Parser {
    Lexer *lex;
    Token cur_tok;
    Program instructions;
    LabelHashTable labels;
} Parser;

static inline void parser_next(Parser *p) {
    p->cur_tok = lexer_next(p->lex);
}

static inline bool parser_accept(Parser *p, TokenKind k) {
    if (p->cur_tok.kind == k) {
        parser_next(p);
        return true;
    }
    return false;
}

static inline Token parser_expect(Parser *p, TokenKind k, const char *msg) {
    if (p->cur_tok.kind != k) {
        coc_log(COC_ERROR, "Syntax error at line %d: expected %s",
                p->instructions.size + 1, msg);
        exit(1);
    }
    Token t = p->cur_tok;
    parser_next(p);
    return t;
}

static inline void parse_statement(Parser *p) {
    Token first = parser_expect(p, tok_identifier, "the first identifier");
    if (parser_accept(p, (TokenKind)':')) {
        Instruction inst = {0};
        inst.op = OP_LABEL;
        inst.Label = first.text;
        coc_vec_append(&p->instructions, inst);
        coc_ht_insert(&p->labels, first.text, p->instructions.size - 1);
        return;
    }
    if (parser_accept(p, (TokenKind)';')) {
        Instruction inst = {0};
        inst.op = OP_JMP;
        inst.Label = first.text;
        coc_vec_append(&p->instructions, inst);
        return;
    }
    parser_expect(p, (TokenKind)',', "',' after the first identifier");

    Token second = p->cur_tok;
    if (second.kind == tok_identifier || second.kind == tok_number) {
        parser_next(p);
        if (parser_accept(p, (TokenKind)'.')) {
            Instruction inst = {0};
            inst.op = OP_ASSIGN;
            inst.OperandA = first.text;
            if (second.kind == tok_number) {
                inst.number = second.number;
                inst.is_B_number = true;
            } else {
                inst.OperandB = second.text;
                inst.is_B_number = false;
            }
            coc_vec_append(&p->instructions, inst);
            return;
        }

        if (parser_accept(p, (TokenKind)'?')) {
            Token label = parser_expect(p, tok_identifier, "label");
            if (parser_accept(p, (TokenKind)';')) {
                Instruction inst = {0};
                inst.op = OP_JEQ;
                inst.OperandA = first.text;
                if (second.kind == tok_number) {
                    inst.number = second.number;
                    inst.is_B_number = true;
                } else {
                    inst.OperandB = second.text;
                    inst.is_B_number = false;
                }
                inst.Label = label.text;
                coc_vec_append(&p->instructions, inst);
                return;
            }
        }

        if (parser_accept(p, (TokenKind)'!')) {
            Instruction inst = {0};
            inst.op = OP_ACT;
            inst.OperandA = first.text;
            inst.OperandB = second.text;
            coc_vec_append(&p->instructions, inst);
            return;
        }
    }
}

static inline void parse_program(Parser *p) {
    while (p->cur_tok.kind != tok_eof) {
        parse_statement(p);
    }
}

typedef struct VarEntry {
    Coc_String key;
    Number value;
    bool is_used;
} VarEntry;

typedef struct VarHashTable {
    VarEntry *items;
    VarEntry *new_items;
    size_t size;
    size_t capacity;
} VarHashTable;

typedef void (*Action)(Number *);

typedef struct ActEntry {
    Coc_String key;
    Action value;
    bool is_used;
} ActEntry;

typedef struct ActHashTable {
    ActEntry *items;
    ActEntry *new_items;
    size_t size;
    size_t capacity;
} ActHashTable;

typedef struct VM {
    Program *prog;
    LabelHashTable *labels;
    VarHashTable vars;
    ActHashTable acts;
    int pc;
} VM;

static inline void register_act(VM *vm, const char *name, Action act) {
    Coc_String act_name = {0};
    coc_str_append(&act_name, name);
    coc_ht_insert(&vm->acts, act_name, act);
}

static inline Number *vm_get_var(VM *vm, Coc_String *var_name) {
    Number *value = NULL;
    coc_ht_find(&vm->vars, *var_name, value);
    if (value == NULL) {
        coc_str_append_null(var_name);
        coc_log(COC_ERROR, "Runtime error at line %d: variable '%s' not found",
                vm->pc + 1, var_name->items);
        exit(1);
    }
    return value;
}

static inline Action *vm_get_action(VM *vm, Coc_String *act_name) {
    Action *act = NULL;
    coc_ht_find(&vm->acts, *act_name, act);
    if (act == NULL) {
        coc_str_append_null(act_name);
        coc_log(COC_ERROR, "Runtime error at line %d: action '%s' not found",
                vm->pc + 1, act_name);
        exit(1);
    }
    return act;
}

static inline int vm_get_label(VM *vm, Coc_String *label) {
    int *pos = NULL;
    coc_ht_find(vm->labels, *label, pos);
    if (pos == NULL) {
        coc_str_append_null(label);
        coc_log(COC_ERROR, "Runtime error at line %d: label '%s' not found",
                vm->pc + 1, label);
        exit(1);
    }
    return *pos;
}

static inline void vm_call_label(VM *vm, Coc_String *label) {
    vm->pc = vm_get_label(vm, label);
}

static inline void vm_assign(VM *vm, Instruction *inst) {
    Number *value = NULL;
    if (inst->is_B_number) value = &inst->number;
    else value = vm_get_var(vm, &inst->OperandB);
    coc_ht_insert(&vm->vars, inst->OperandA, *value);
    vm->pc++;
}

static inline void vm_act(VM *vm, Instruction *inst) {
    Number *a = vm_get_var(vm, &inst->OperandA);
    Action *act = vm_get_action(vm, &inst->OperandB);
    (*act)(a);
    vm->pc++;
}

static inline void vm_jmp(VM *vm, Instruction *inst) {
    int pos = vm_get_label(vm, &inst->Label);
    vm->pc = pos;
}

static inline void vm_jeq(VM *vm, Instruction *inst) {
    Number *a = vm_get_var(vm, &inst->OperandA);
    Number *b = NULL;
    if (inst->is_B_number) b = &inst->number;
    else b = vm_get_var(vm, &inst->OperandB);
    bool cond = false;
    double eps = 1e-9;
    if (a->is_float == b->is_float) {
        if (a->is_float)
            cond = fabs(a->float_value - b->float_value) < eps;
        else
            cond = a->int_value == b->int_value;
    } else {
        if (a->is_float)
            cond = fabs(a->float_value - (double)b->int_value) < eps;
        else
            cond = fabs((double)a->int_value - b->float_value) < eps;
    }
    if (cond) vm_jmp(vm, inst);
    else vm->pc++;
}

static inline void vm_label(VM *vm, Instruction *inst) {
    vm->pc++;
}

void run(VM *vm) {
    vm->pc = 0;
    int n = vm->prog->size;
    while (vm->pc < n) {
        Instruction *inst = &vm->prog->items[vm->pc];
        switch (inst->op) {
        case OP_ASSIGN:
            vm_assign(vm, inst);
            break;
        case OP_ACT:
            vm_act(vm, inst);
            break;
        case OP_JEQ:
            vm_jeq(vm, inst);
            break;
        case OP_JMP:
            vm_jmp(vm, inst);
            break;
        case OP_LABEL:
            vm_label(vm, inst);
            break;
        default:
            break;
        }
    }
}

static inline void act_print(Number *n) {
    if (n->is_float)
        printf("%f\n", n->float_value);
    else
        printf("%lld\n", n->int_value);
}

static inline void act_inc(Number *n) {
    if (n->is_float) n->float_value += 1.0;
    else n->int_value += 1;
}

static inline void act_dec(Number *n) {
    if (n->is_float) n->float_value -= 1.0;
    else n->int_value -= 1;
}

static inline void act_input(Number *n) {
    putchar('>');
    char buf[128];
    if (!fgets(buf, sizeof(buf), stdin)) {
        coc_log(COC_ERROR, "Input error");
        exit(1);
    }
    bool is_float = false;
    for (char *p = buf; *p; p++) {
        if (*p == '.') {
            is_float = true;
            break;
        }
    }
    char *end;
    if (is_float) {
        double f = strtod(buf, &end);
        n->float_value = f;
        n->is_float = true;
    } else {
        long long v = strtoll(buf, &end, 10);
        n->int_value = v;
        n->is_float = false;
    }
}

static inline void act_puts(Number *n) {
    uint64_t value = n->is_float ? (uint64_t)n->float_value : (uint64_t)n->int_value;
    for (int i = 7; i >= 0; i--) {
        unsigned char c = (value >> (i * 8)) & 0xFF;
        if (c != '\0') putchar(c);
    }
}

static inline void register_builtin_actions(VM *vm) {
    register_act(vm, "print", act_print);
    register_act(vm, "inc",   act_inc);
    register_act(vm, "dec",   act_dec);
    register_act(vm, "input", act_input);
    register_act(vm, "puts",  act_puts);
}

static inline int run_file(const char *filename, void (*register_user_actions)(VM *)) {
    Coc_String source = {0};
    if (coc_read_entire_file(filename, &source) != 0) return errno;
    Lexer lex = {
        .src = source,
        .pos = 0
    };
    Parser parser = {
        .lex = &lex,
        .cur_tok = 0,
        .instructions = {0},
        .labels = {0}
    };
    parser_next(&parser);
    parse_program(&parser);
    VM vm = {
        .prog = &parser.instructions,
        .labels = &parser.labels,
        .acts = {0},
        .vars = {0},
        .pc = 0
    };
    register_builtin_actions(&vm);
    if (register_user_actions != NULL) {
        coc_log(COC_DEBUG, "Register user actions");
        register_user_actions(&vm);
    }
    run(&vm);
    return 0;
}

#endif