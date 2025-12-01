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

Token lexer_next(Lexer *l) {
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
    LabelHashTable label_pos;
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

void parse_statement(Parser *p) {
    Token first = parser_expect(p, tok_identifier, "the first identifier");
    if (parser_accept(p, (TokenKind)':')) {
        Instruction inst = {0};
        inst.op = OP_LABEL;
        inst.Label = first.text;
        coc_vec_append(&p->instructions, inst);
        coc_ht_insert(&p->label_pos, first.text, p->instructions.size - 1);
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

void parse_program(Parser *p) {
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

typedef struct Interpreter {
    Program *prog;
    LabelHashTable *label_pos;
    VarHashTable vars;
    ActHashTable acts;
    int pc;
} Interpreter;

void register_act(Interpreter *intp, const char *name, Action act) {
    Coc_String act_name = {0};
    coc_str_append(&act_name, name);
    coc_ht_insert(&intp->acts, act_name, act);
}

static inline void interpreter_assign(Interpreter *intp, Instruction *inst) {
    Number *value = NULL;
    if (inst->is_B_number) {
        value = &inst->number;
    } else {
        coc_ht_find(&intp->vars, inst->OperandB, value);
        if (value == NULL) {
            coc_str_append_null(&inst->OperandB);
            coc_log(COC_ERROR, "Runtime error at line %d: variable '%s' not found",
                    intp->pc + 1, inst->OperandB.items);
            exit(1);
        }
    }
    coc_ht_insert(&intp->vars, inst->OperandA, *value);
    intp->pc++;
}

static inline void interpreter_act(Interpreter *intp, Instruction *inst) {
    Number *a = NULL;
    coc_ht_find(&intp->vars, inst->OperandA, a);
    if (a == NULL) {
        coc_str_append_null(&inst->OperandA);
        coc_log(COC_ERROR, "Runtime error at line %d: variable '%s' not found",
                intp->pc + 1, inst->OperandA.items);
        exit(1);
    }
    Action *act = NULL;
    coc_ht_find(&intp->acts, inst->OperandB, act);
    if (act == NULL) {
        coc_str_append_null(&inst->OperandB);
        coc_log(COC_ERROR, "Runtime error at line %d: action '%s' not found",
                intp->pc + 1, inst->OperandB.items);
        exit(1);
    }
    (*act)(a);
    intp->pc++;
}

static inline void interpreter_jmp(Interpreter *intp, Instruction *inst) {
    int *pos = NULL;
    coc_ht_find(intp->label_pos, inst->Label, pos);
    if (pos ==NULL) {
        coc_str_append_null(&inst->Label);
        coc_log(COC_ERROR, "Runtime error at line %d: label '%s' not found",
                intp->pc + 1, inst->Label.items);
        exit(1);
    }
    intp->pc = *pos;
}

static inline void interpreter_jeq(Interpreter *intp, Instruction *inst) {
    Number *a = NULL;
    coc_ht_find(&intp->vars, inst->OperandA, a);
    if (a == NULL) {
        coc_str_append_null(&inst->OperandA);
        coc_log(COC_ERROR, "Runtime error at line %d: variable '%s' not found",
                intp->pc + 1, inst->OperandA.items);
        exit(1);
    }
    Number *b = NULL;
    if (inst->is_B_number) {
        b = &inst->number;
    } else {
        coc_ht_find(&intp->vars, inst->OperandB, b);
        if (b == NULL) {
            coc_str_append_null(&inst->OperandB);
            coc_log(COC_ERROR, "Runtime error at line %d: variable '%s' not found",
                    intp->pc + 1, inst->OperandB.items);
            exit(1);
        }
    }
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
    if (cond) interpreter_jmp(intp, inst);
    else intp->pc++;
}

static inline void interpreter_label(Interpreter *intp, Instruction *inst) {
    intp->pc++;
}

void run(Interpreter *intp) {
    intp->pc = 0;
    int n = intp->prog->size;
    while (intp->pc < n) {
        Instruction *inst = &intp->prog->items[intp->pc];
        switch (inst->op) {
        case OP_ASSIGN:
            interpreter_assign(intp, inst);
            break;
        case OP_ACT:
            interpreter_act(intp, inst);
            break;
        case OP_JEQ:
            interpreter_jeq(intp, inst);
            break;
        case OP_JMP:
            interpreter_jmp(intp, inst);
            break;
        case OP_LABEL:
            interpreter_label(intp, inst);
            break;
        default:
            break;
        }
    }
}

void act_print(Number *n) {
    if (n->is_float)
        printf("%f\n", n->float_value);
    else
        printf("%lld\n", n->int_value);
}

void act_inc(Number *n) {
    if (n->is_float) n->float_value += 1.0;
    else n->int_value += 1;
}

void act_dec(Number *n) {
    if (n->is_float) n->float_value -= 1.0;
    else n->int_value -= 1;
}

void act_input(Number *n) {
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

void debug_print_vars(Interpreter *intp) {
    coc_log_raw(COC_DEBUG, "Final variables:\n");
    for (size_t i = 0; i < intp->vars.capacity; i++) {
        VarEntry *e = &intp->vars.items[i];
        if (!e->is_used) continue;
        coc_str_append_null(&e->key);
        coc_log_raw(COC_DEBUG, "%s = ", e->key.items);
        if (e->value.is_float)
            coc_log_raw(COC_DEBUG, "%f\n", e->value.float_value);
        else
            coc_log_raw(COC_DEBUG, "%lld\n", e->value.int_value);
    }
}

static inline void register_builtin_actions(Interpreter *intp) {
    register_act(intp, "print", act_print);
    register_act(intp, "inc",   act_inc);
    register_act(intp, "dec",   act_dec);
    register_act(intp, "input", act_input);
}

int run_file(const char *filename, void (*register_user_actions)(Interpreter *)) {
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
        .label_pos = {0}
    };
    parser_next(&parser);
    parse_program(&parser);
    Interpreter intp = {
        .prog = &parser.instructions,
        .label_pos = &parser.label_pos,
        .acts = {0},
        .vars = {0},
        .pc = 0
    };
    register_builtin_actions(&intp);
    if (register_user_actions != NULL) {
        coc_log(COC_DEBUG, "Register user actions");
        register_user_actions(&intp);
    }
    run(&intp);
    debug_print_vars(&intp);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        coc_log(COC_ERROR, "Usage: %s <file name>", argv[0]);
        return 1;
    }
    run_file(argv[1], NULL);
    return 0;
}