// puncta.h - version 1.0.8 (2026-01-15)
// required coc.h >= 1.4.0
#ifndef PUNCTA_H_
#define PUNCTA_H_

#define PUNCTA_VERSION_MAJOR 1
#define PUNCTA_VERSION_MINOR 0
#define PUNCTA_VERSION_PATCH 8

#include <math.h>
#include <limits.h>
#include "coc.h"
#include "puncta_eval.h"

#define STRING_LEN (16 - 2 * sizeof(bool))
#define PACK_LEN   (sizeof(long long))
#define EXTRA_LEN  (STRING_LEN - PACK_LEN)

typedef struct Number {
    union {
        long long int_value;
        double    float_value;
    };
    char extra[EXTRA_LEN];
    bool is_extra;
    bool is_float;
} Number;

static inline int64_t number_trunc_i64(const Number *n, const char *who, int line) {
    if (!n->is_float) return (int64_t)n->int_value;
    if (!isfinite(n->float_value)) {
        coc_log(COC_ERROR, "Runtime error at line %d: in action '%s': expects a finite number", line, who);
        exit(1);
    }
    if (n->float_value > (double)LLONG_MAX || n->float_value < (double)LLONG_MIN) {
        coc_log(COC_ERROR, "Runtime error at line %d: in action '%s' number out of int64 range", line, who);
        exit(1);
    }
    return (int64_t)n->float_value;
}

static inline size_t number_to_string(const Number *n, char *buf, size_t buf_size, const char *who, int line) {
    if (n->is_float) {
        coc_log(COC_ERROR, 
                "Runtime error at line %d: in action '%s': cannot treat a floating-point value as a string (packed int64 required); got float=%g",
                line, who, n->float_value);
        exit(1);
    }
    uint64_t value = (uint64_t)n->int_value;
    size_t len = 0;
    for (size_t i = 0; i < PACK_LEN; i++) {
        unsigned char c = (value >> (i * PACK_LEN)) & 0xFF;
        if (c == '\0') break;
        if (len >= buf_size) {
            coc_log(COC_ERROR,
                    "Runtime error at line %d: in action '%s': buffer too small for number-to-string conversion (need %zu bytes at least, have %zu)",
                    line, who, len + 1, buf_size);
            exit(1);
        }
        buf[len++] = c;
    }
    if (n->is_extra) {
        for (size_t i = 0; i < EXTRA_LEN; i++) {
            char c = n->extra[i];
            if (c == '\0') break;
            if (len >= buf_size) {
                coc_log(COC_ERROR,
                        "Runtime error at line %d: in action '%s': buffer too small for extra number-to-string conversion (need %zu bytes at least, have %zu)",
                        line, who, len + 1, buf_size);
                exit(1);
            }
            buf[len++] = c;
        }
    }
    buf[len] = '\0';
    return len;
}

typedef enum TokenKind{
    tok_eof        = -1,
    tok_identifier = -2,
    tok_number     = -3
} TokenKind;

typedef struct Token {
    Coc_String text;
    Number     number;
    TokenKind  kind;
    int        line;
} Token;

typedef struct Lexer {
    Coc_String src;
    size_t     pos;
    int        line;
} Lexer;

static inline Lexer *lexer_init(Coc_String source) {
    Lexer *lex = (Lexer *)COC_MALLOC(sizeof(Lexer));
    if (lex == NULL) {
    coc_log(COC_FATAL, "Lexer init: malloc() failed");
        exit(1);
    }
    lex->src  = source;
    lex->pos  = 0;
    lex->line = 1;
    return lex;
}

static inline void lexer_free(Lexer *l) {
    coc_str_free(&l->src);
    COC_FREE(l);
}

static inline char lexer_peek(Lexer *l) {
    if (l->pos >= coc_str_size(&l->src)) return '\0';
    return coc_str_data(&l->src)[l->pos];
}

static inline char lexer_peek2(Lexer *l) {
    if (l->pos + 1 >= l->src.size) return '\0';
    return coc_str_data(&l->src)[l->pos + 1];
}

static inline char lexer_get(Lexer *l) {
    if (l->pos >= l->src.size) return '\0';
    return coc_str_data(&l->src)[l->pos++];
}

static inline void skip_whitespace(Lexer *l) {
    while (isspace(lexer_peek(l))) {
        if (lexer_peek(l) == '\n') l->line++;
        l->pos++;
    }
}

static inline void skip_comment(Lexer *l) {
    while (true) {
        if (lexer_peek(l) != '(') return;
        int start_line = l->line;
        int depth = 1;
        lexer_get(l);
        while (depth > 0) {
            switch (lexer_get(l)) {
            case '(': depth++; break;
            case ')': depth--; break;
            case '\n': l->line++; break;
            case '\0':
                coc_log(COC_ERROR,
                        "Lexical error at line %d: unterminated comment",
                        start_line);
                exit(1);
            }
        }
        skip_whitespace(l);
    }
}

static inline bool is_punctuator(char c) {
    static const char punctuators[] = ",.!?:;@#";
    return strchr(punctuators, c) != NULL;
}

static inline bool is_token_boundary(char c) {
    return c == '\0' || isspace(c) || is_punctuator(c) || c == '(';
}

static inline Token lexer_next(Lexer *l) {
    skip_whitespace(l);
    skip_comment(l);
    char c = lexer_peek(l);
    if (c == ')') {
        coc_log(COC_ERROR,
                "Lexical error at line %d: unmatched ')'",
                l->line);
        exit(1);
    }
    if (c == '\0') return (Token){.kind = tok_eof, .line = l->line};
    if (isalpha(c) || c == '_') {
        Coc_String id = {0};
        coc_str_push(&id, lexer_get(l));
        while (isalnum(lexer_peek(l)) || lexer_peek(l) == '_') {
            coc_str_push(&id, lexer_get(l));
        }
        return (Token){.kind = tok_identifier, .text = coc_str_move(&id), .line = l->line};
    }
    if (isdigit(c) || ((c == '-' || c == '+') && isdigit(lexer_peek2(l)))) {
        Coc_String num = {0};
        bool is_float = false;
        bool is_hex = false;
        if (lexer_peek(l) == '-') coc_str_push(&num, lexer_get(l));
        else if (lexer_peek(l) == '+') coc_str_push(&num, lexer_get(l));
        if (lexer_peek(l) == '0' && (lexer_peek2(l) == 'x' || lexer_peek2(l) == 'X')) {
            is_hex = true;
            coc_str_push(&num, lexer_get(l));
            coc_str_push(&num, lexer_get(l));
        }
        while (isdigit(lexer_peek(l)) || (is_hex && isxdigit(lexer_peek(l)))) {
            coc_str_push(&num, lexer_get(l));
        }
        if (!is_hex && lexer_peek(l) == '.' && isdigit(lexer_peek2(l))) {
            is_float = true;
            coc_str_push(&num, lexer_get(l));
            while (isdigit(lexer_peek(l))) {
                coc_str_push(&num, lexer_get(l));
            }
        }
        char next_char = lexer_peek(l);
        if (!is_token_boundary(next_char)) {
            coc_log(COC_ERROR, 
                    "Lexical error at line %d: invalid character '%c' after %s literal",
                    l->line, next_char, is_float ? "floating-point" : (is_hex ? "hexadecimal integer" : "decimal integer"));
            exit(1);
        }
        coc_str_append_null(&num);
        char *end_ptr = NULL;
        errno = 0;
        if (is_float) {
            double float_val = strtod(coc_str_data(&num), &end_ptr);
            if (end_ptr == coc_str_data(&num)) {
                coc_log(COC_ERROR,
                        "Lexical error at line %d: invalid floating-point literal",
                        l->line);
                exit(1);
            }
            if (errno == ERANGE) {
                coc_log(COC_ERROR,
                        "Lexical error at line %d: floating-point literal out of range",
                        l->line);
                exit(errno);
            }
            return (Token){
                .kind = tok_number,
                .number = (Number){
                    .float_value = float_val,
                    .is_float    = true
                },
                .line = l->line
            };
        } else {
            long long int_val = strtoll(coc_str_data(&num), &end_ptr, is_hex ? 16 : 10);
            if (end_ptr == coc_str_data(&num)) {
                coc_log(COC_ERROR,
                        "Lexical error at line %d: invalid %s integer literal",
                        l->line, is_hex ? "hexadecimal" : "decimal");
                exit(1);
            }
            if (errno == ERANGE) {
                coc_log(COC_ERROR,
                        "Lexical error at line %d: %s integer literal out of range",
                        l->line, is_hex ? "hexadecimal" : "decimal");
                exit(errno);
            }
            return (Token){
                .kind = tok_number,
                .number = (Number){
                    .int_value = int_val,
                    .is_float  = false
                },
                .line = l->line
            };
        }
    }
    if (c == '"') {
        lexer_get(l);
        Number num = {0};
        size_t len = 0;
        while (lexer_peek(l) && lexer_peek(l) != '"') {
            if (len >= PACK_LEN) num.is_extra = true;
            if (len >= STRING_LEN) {
                coc_log(COC_ERROR,
                        "Lexical error at line %d: string literal too long (max %lld characters allowed)",
                        l->line, STRING_LEN);
                exit(1);
            }
            char ch = lexer_get(l);
            if (ch == '\n') {
                coc_log(COC_ERROR,
                        "Lexical error at line %d: unescaped '\\n' in string literal",
                        l->line);
                exit(1);
            }
            if (ch == '\\') {
                char esc = lexer_get(l);
                switch (esc) {
                case 'a': ch = '\a'; break;
                case 'b': ch = '\b'; break;
                case 't': ch = '\t'; break;
                case 'n': ch = '\n'; break;
                case 'v': ch = '\v'; break;
                case 'f': ch = '\f'; break;
                case 'r': ch = '\r'; break;
                case '"': ch = '"' ; break;
                case '\\':ch = '\\'; break;
                case '\0':
                    coc_log(COC_ERROR,
                            "Lexical error at line %d: unterminated escape sequence",
                            l->line);
                    exit(1);
                default:
                    coc_log(COC_ERROR,
                            "Lexical error at line %d: invalid escape sequence \\%c",
                            l->line, esc);
                    exit(1);
                }
            }
            if (!num.is_extra) num.int_value |= ((uint64_t)ch & 0xFF) << ((len++) * PACK_LEN);
            else num.extra[(len++) - PACK_LEN] = ch;
        }
        if (lexer_peek(l) != '"') {
            coc_log(COC_ERROR, 
                    "Lexical error at line %d: unterminated string (missing '\"')",
                    l->line);
            exit(1);
        }
        lexer_get(l);
        return (Token){
            .kind   = tok_number,
            .number = num,
            .line   = l->line
        };
    }
    
    if (is_punctuator(c)) {
        return (Token){.kind = (TokenKind)lexer_get(l), .line = l->line};
    } else {
        Coc_String id = {0};
        coc_str_push(&id, lexer_get(l));
        return (Token){.kind = tok_identifier, .text = coc_str_move(&id), .line = l->line};
    }
}

typedef enum OpCode {
    OP_ASSIGN,
    OP_ACT,
    OP_JEQ,
    OP_JMP,
    OP_END
} OpCode;

typedef struct Instruction {
    Coc_String OperandA;
    Coc_String OperandB;
    Coc_String Label;
    Number     number;
    OpCode     op;
    int        line;
    bool       is_B_number;
} Instruction;

typedef struct Program {
    Instruction *items;
    size_t       size;
    size_t       capacity;
} Program;

typedef struct LabelEntry {
    Coc_String key;
    int        value;
    bool       is_used;
} LabelEntry;

typedef struct LabelHashTable {
    LabelEntry *items;
    LabelEntry *new_items;
    size_t      size;
    size_t      capacity;
} LabelHashTable;

typedef struct Parser {
    Lexer         *lex;
    Token          cur_tok;
    Program        instructions;
    LabelHashTable labels;
} Parser;

static inline Parser *parser_init(Lexer *lex) {
    Parser *parser = (Parser *)COC_MALLOC(sizeof(Parser));
    if (parser == NULL) {
    coc_log(COC_FATAL, "Parser init: malloc() failed");
        exit(1);
    }
    parser->lex          = lex;
    parser->cur_tok      = lexer_next(lex);
    parser->instructions = (Program){0};
    parser->labels       = (LabelHashTable){0};
    return parser;
}

static inline void parser_free(Parser *p) {
    lexer_free(p->lex);
    COC_FREE(p);
}

static inline void parser_error(const char *msg, int line) {
    coc_log(COC_ERROR, "Syntax error at line %d: expected %s", line, msg);
    exit(1);
}

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
    if (p->cur_tok.kind != k) parser_error(msg, p->cur_tok.line);
    Token t = p->cur_tok;
    parser_next(p);
    return t;
}

#define emit_assign(p, first, second, line) do {    \
    Instruction inst = {0};                         \
    inst.op = OP_ASSIGN;                            \
    inst.OperandA = coc_str_move(&first.text);      \
    inst.line = line;                               \
    if (second.kind == tok_number) {                \
        inst.number = second.number;                \
        inst.is_B_number = true;                    \
    } else {                                        \
        inst.OperandB = coc_str_move(&second.text); \
        inst.is_B_number = false;                   \
    }                                               \
    coc_vec_append(&p->instructions, inst);         \
} while (0)

#define emit_act(p, first, second, line) do {   \
    Instruction inst = {0};                     \
    inst.op = OP_ACT;                           \
    inst.OperandA = coc_str_move(&first.text);  \
    inst.OperandB = coc_str_move(&second.text); \
    inst.line = line;                           \
    coc_vec_append(&p->instructions, inst);     \
} while (0)

#define emit_label(p, label, line) do {                                \
    coc_ht_insert_move(&p->labels, &label.text, p->instructions.size); \
} while (0)

#define emit_jmp(p, label, line) do {       \
    Instruction inst = {0};                 \
    inst.op = OP_JMP;                       \
    inst.Label = coc_str_move(&label.text); \
    inst.line = line;                       \
    coc_vec_append(&p->instructions, inst); \
} while (0)

#define emit_jeq(p, first, second, label, line) do { \
    Instruction inst = {0};                          \
    inst.op = OP_JEQ;                                \
    inst.OperandA = coc_str_move(&first.text);       \
    inst.line = line;                                \
    if (second.kind == tok_number) {                 \
        inst.number = second.number;                 \
        inst.is_B_number = true;                     \
    } else {                                         \
        inst.OperandB = coc_str_move(&second.text);  \
        inst.is_B_number = false;                    \
    }                                                \
    inst.Label = coc_str_move(&label.text);          \
    coc_vec_append(&p->instructions, inst);          \
} while (0)

#define emit_end(p) do {                   \
    Instruction end = {0};                 \
    end.op = OP_END;                       \
    coc_vec_append(&p->instructions, end); \
} while (0)


static inline void parse_statement(Parser *p) {
    int line = p->cur_tok.line;
    Token first = parser_expect(p, tok_identifier, "the first identifier");
    if (parser_accept(p, (TokenKind)':')) {
        emit_label(p, first, line);
        return;
    }
    if (parser_accept(p, (TokenKind)';')) {
        emit_jmp(p, first, line);
        return;
    }
    if (parser_accept(p, (TokenKind)'#')) {
    if (parser_accept(p, (TokenKind)':')) {
        Token end_label = first;
        end_label.text = coc_str_copy(&first.text);
        coc_str_append(&end_label.text, "End");
        emit_jmp(p, end_label, line);
        emit_label(p, first, line);
        return;
    }
    if (parser_accept(p, (TokenKind)';')) {
        coc_str_append(&first.text, "End");
        emit_label(p, first, line);
        return;
    }
        parser_error("':' or ';' at the end of the statement", line);
    }
    parser_expect(p, (TokenKind)',', "',' after the first identifier");

    Token second = p->cur_tok;
    if (second.kind == tok_identifier || second.kind == tok_number) {
        parser_next(p);
        if (parser_accept(p, (TokenKind)'.')) {
            emit_assign(p, first, second, line);
            return;
        }

        if (parser_accept(p, (TokenKind)'?')) {
            Token label = parser_expect(p, tok_identifier, "label");
            if (parser_accept(p, (TokenKind)';')) {
                emit_jeq(p, first, second, label, line);
                return;
            }
            parser_error("';' at the end of the statement", line);
        }

        if (parser_accept(p, (TokenKind)'!')) {
            if (second.kind == tok_number)
                parser_error("identifier (action name) before '!', but got number", line);
            if (parser_accept(p, (TokenKind)'@')) {
                Token extra = p->cur_tok;
                if (extra.kind == tok_identifier || extra.kind == tok_number) {
                    parser_next(p);
                    if (parser_accept(p, (TokenKind)'.')) {
                        Token temp = first;
                        temp.text = coc_str_copy(&first.text);
                        emit_assign(p, temp, extra, line);
                    } else parser_error("'.' at the end of the statement", line);
                } else parser_error("identifier or number after '@'", line);
            }
            emit_act(p, first, second, line);
            return;
        }
        parser_error("'.', '?' or '!' at the end of the statement", line);
    }
    parser_error("identifier or number after ','", line);
}

static inline void parse_program(Parser *p) {
    while (p->cur_tok.kind != tok_eof) parse_statement(p);
    emit_end(p);
}

typedef struct VarEntry {
    Coc_String key;
    Number     value;
    bool       is_used;
} VarEntry;

typedef struct VarHashTable {
    VarEntry *items;
    VarEntry *new_items;
    size_t    size;
    size_t    capacity;
} VarHashTable;

typedef struct VM VM;

typedef void (*Action)(VM *vm, Number *);

typedef struct ActEntry {
    Coc_String key;
    Action     value;
    bool       is_used;
} ActEntry;

typedef struct ActHashTable {
    ActEntry *items;
    ActEntry *new_items;
    size_t    size;
    size_t    capacity;
} ActHashTable;

struct VM {
    LabelHashTable labels;
    VarHashTable   vars;
    ActHashTable   acts;
    Program        prog;
    int            pc;
};

#define vm_msg(...) do { coc_log(COC_INFO, __VA_ARGS__); } while (0)

static inline VM *vm_init(Parser *p) {
    VM *vm = (VM *)COC_MALLOC(sizeof(VM));
    if (vm == NULL) {
        coc_log(COC_FATAL, "VM init: malloc() failed");
        exit(1);
    }
    coc_vec_move(&vm->prog, &p->instructions);
    coc_vec_move(&vm->labels, &p->labels);
    vm->vars = (VarHashTable){0};
    vm->acts = (ActHashTable){0};
    vm->pc   = 0;
    parser_free(p);
    return vm;
}

static inline void vm_free(VM *vm) {
    coc_vec_free(&vm->prog);
    coc_ht_free(&vm->labels);
    coc_ht_free(&vm->acts);
    coc_ht_free(&vm->vars);
    COC_FREE(vm);
}

static inline void register_act(VM *vm, const char *name, Action act) {
    Coc_String act_name = {0};
    coc_str_append(&act_name, name);
    coc_ht_insert_move(&vm->acts, &act_name, act);
}

static inline int vm_get_line_number(VM *vm) {
    return vm->prog.items[vm->pc].line;
}

static inline Number *vm_get_var(VM *vm, Coc_String *var_name) {
    Number *value = NULL;
    coc_ht_find(&vm->vars, var_name, value);
    if (value == NULL) {
        coc_str_append_null(var_name);
        coc_log(COC_ERROR, "Runtime error at line %d: variable '%s' not found",
                vm_get_line_number(vm), coc_str_data(var_name));
        exit(1);
    }
    return value;
}

static inline Action *vm_get_action(VM *vm, Coc_String *act_name) {
    Action *act = NULL;
    coc_ht_find(&vm->acts, act_name, act);
    if (act == NULL) {
        coc_str_append_null(act_name);
        coc_log(COC_ERROR, "Runtime error at line %d: action '%s' not found",
                vm_get_line_number(vm), coc_str_data(act_name));
        exit(1);
    }
    return act;
}

static inline int vm_get_label(VM *vm, Coc_String *label) {
    int *pos = NULL;
    coc_ht_find(&vm->labels, label, pos);
    if (pos == NULL) {
        coc_str_append_null(label);
        coc_log(COC_ERROR, "Runtime error at line %d: label '%s' not found",
                vm_get_line_number(vm), coc_str_data(label));
        exit(1);
    }
    return *pos;
}

static inline void vm_assign(VM *vm, Instruction *inst) {
    Number *value = NULL;
    if (inst->is_B_number) value = &inst->number;
    else value = vm_get_var(vm, &inst->OperandB);
    coc_ht_insert_copy(&vm->vars, &inst->OperandA, *value);
    vm->pc++;
}

static inline void vm_act(VM *vm, Instruction *inst) {
    Number *a = vm_get_var(vm, &inst->OperandA);
    Action *act = vm_get_action(vm, &inst->OperandB);
    (*act)(vm, a);
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
    const double eps = 1e-9;
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

void run(VM *vm) {
    int n = vm->prog.size;
    while (vm->pc < n) {
        Instruction *inst = &vm->prog.items[vm->pc];
        switch (inst->op) {
        case OP_ASSIGN: vm_assign(vm, inst); break;
        case OP_ACT:    vm_act(vm, inst)   ; break;
        case OP_JEQ:    vm_jeq(vm, inst)   ; break;
        case OP_JMP:    vm_jmp(vm, inst)   ; break;
        case OP_END:    vm->pc++           ; break;
        }
    }
}

static inline void vm_call_label(VM *vm, Coc_String *label) {
    vm->pc = vm_get_label(vm, label);
    run(vm);
}

static inline void vm_check_labels(VM *vm) {
    for (size_t i = 0; i < vm->prog.size; i++) {
        Instruction *inst = &vm->prog.items[i];
        if (inst->op != OP_JMP && inst->op != OP_JEQ) continue;
        int *pos = NULL;
        coc_ht_find(&vm->labels, &inst->Label, pos);
        if (pos == NULL) {
            coc_str_append_null(&inst->Label);
            coc_log(COC_ERROR,
                    "Semantic error at line %d: label '%s' not defined",
                    inst->line, coc_str_data(&inst->Label));
            exit(1);
        }
    }
}

static inline void act_inc(VM *vm, Number *n) {
    COC_UNUSED(vm);
    if (n->is_float) n->float_value += 1.0;
    else n->int_value += 1;
}

static inline void act_dec(VM *vm, Number *n) {
    COC_UNUSED(vm);
    if (n->is_float) n->float_value -= 1.0;
    else n->int_value -= 1;
}

static inline void act_double(VM *vm, Number *n) {
    COC_UNUSED(vm);
    if (n->is_float) n->float_value *= 2;
    else n->int_value *= 2;
}

static inline void act_halve(VM *vm, Number *n) {
    COC_UNUSED(vm);
    if (n->is_float) n->float_value /= 2;
    else n->int_value /=2;
}

static inline void act_neg(VM *vm, Number *n) {
    COC_UNUSED(vm);
    if (n->is_float) n->float_value = -n->float_value;
    else n->int_value = -n->int_value;
}

static inline void act_abs(VM *vm, Number *n) {
    COC_UNUSED(vm);
    if (n->is_float) {
        n->float_value = fabs(n->float_value);
        return;
    }
    if (n->int_value == LLONG_MIN) {
        n->is_float = true;
        n->float_value = 9223372036854775808.0;
        return;
    }
    if (n->int_value < 0) n->int_value = -n->int_value;
}

static inline void act_not(VM *vm, Number *n) {
    n->int_value = !number_trunc_i64(n, "not", vm_get_line_number(vm));
    n->is_float = false;
}

static inline void act_isodd(VM *vm, Number *n) {
    long long value = number_trunc_i64(n, "isodd", vm_get_line_number(vm));
    n->int_value = (long long)((uint64_t)value & 1ull);
    n->is_float = false;
}

static inline void act_isneg(VM *vm, Number *n) {
    long long result;
    if (n->is_float) {
        if (isnan(n->float_value)) {
            coc_log(COC_ERROR, 
                    "Runtime error at line %d: isneg expects a number (got NaN)",
                    vm_get_line_number(vm));
            exit(1);
        }
        result = (n->float_value < 0.0);
    } else {
        result = (n->int_value < 0);
    }
    n->int_value = result;
    n->is_float = false;
}

static inline void act_toint(VM *vm, Number *n) {
    if (!n->is_float) return;
    n->int_value = number_trunc_i64(n, "toint", vm_get_line_number(vm));
    n->is_float = false;
}

static inline void act_input(VM *vm, Number *n) {
    COC_UNUSED(vm);
    char buf[128];
    if (!fgets(buf, sizeof(buf), stdin)) {
        coc_log(COC_FATAL, "Input error: fgets() failed");
        exit(1);
    }
    bool is_float = false;
    bool is_hex = false;
    if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) is_hex = true;
    for (char *p = buf; *p; p++) {
        if (*p == '.') {
            is_float = true;
            break;
        }
    }
    errno = 0;
    char *end_ptr = NULL;
    if (is_float) {
        double f = strtod(buf, &end_ptr);
        if (end_ptr == buf) {
            coc_log(COC_ERROR, "Input error: invalid floating-point literal");
            exit(1);
        }
        if (*end_ptr != '\0' && *end_ptr != '\r' && *end_ptr != '\n') {
            coc_log(COC_ERROR, "Input error: invalid character '%c' in floating-point literal", *end_ptr);
            exit(1);
        }
        if (errno == ERANGE) {
            coc_log(COC_ERROR, "Input error : floating-point literal out of range");
            exit(errno);
        }
        n->float_value = f;
        n->is_float = true;
    } else {
        long long v = strtoll(buf, &end_ptr, is_hex ? 16 : 10);
        if (end_ptr == buf) {
            coc_log(COC_ERROR,
                    "Input error: invalid %s integer literal",
                    is_hex ? "hexadecimal" : "decimal");
            exit(1);
        }
        if (*end_ptr != '\0' && *end_ptr != '\r' && *end_ptr != '\n') {
            coc_log(COC_ERROR,
                    "Input error: invalid character '%c' in %s integer literal",
                    *end_ptr, is_hex ? "hexadecimal" : "decimal");
            exit(1);
        }
        if (errno == ERANGE) {
            coc_log(COC_ERROR,
                    "Input error : %s integer literal out of range",
                    is_hex ? "hexadecimal" : "decimal");
            exit(errno);
        }
        n->int_value = v;
        n->is_float = false;
    }
}

static inline void act_print(VM *vm, Number *n) {
    COC_UNUSED(vm);
    if (n->is_float) printf("%g\n", n->float_value);
    else printf("%lld\n", n->int_value);
}

static inline void act_putn(VM *vm, Number *n) {
    COC_UNUSED(vm);
    if (n->is_float) printf("%g", n->float_value);
    else printf("%lld", n->int_value);
}

static inline void act_getc(VM *vm, Number *n) {
    COC_UNUSED(vm);
    int c = getchar();
    if (c == EOF) {
        coc_log(COC_FATAL, "Input error: getchar() failed");
        exit(1);
    } 
    n->int_value = c;
    n->is_float = false;
    while ((c = getchar()) != '\n' && c != EOF);
}

static inline void act_putc(VM *vm, Number *n) {
    int value = number_trunc_i64(n, "putc", vm_get_line_number(vm));
    putchar(value);
}

static inline void act_gets(VM *vm, Number *n) {
    COC_UNUSED(vm);
    char buf[32];
    if (!fgets(buf, sizeof(buf), stdin)) {
        coc_log(COC_FATAL, "Input error: fgets() failed");
        exit(1);
    }
    uint64_t value = 0;
    bool is_extra = false;
    size_t len = 0;
    for (; buf[len] != '\0'; len++) {
        if (!is_extra) value = (value << PACK_LEN) | (uint64_t)buf[len];
        else n->extra[len - PACK_LEN] = buf[len];
    }
    n->int_value = value;
    n->is_extra  = is_extra;
    n->is_float  = false;
}

static inline void act_puts(VM *vm, Number *n) {
    char buf[16];
    number_to_string(n, buf, sizeof(buf), "puts", vm_get_line_number(vm));
    printf("%s", buf);
}

static inline void act_putl(VM *vm, Number *n) {
    char buf[16];
    number_to_string(n, buf, sizeof(buf), "putl", vm_get_line_number(vm));
    puts(buf);
}

static inline void act_putx(VM *vm, Number *n) {
    uint64_t value = (uint64_t)number_trunc_i64(n, "putx", vm_get_line_number(vm));
    printf("%016llx", value);
}

static inline void act_eval(VM *vm, Number *n) {
    double vars[52];
    char expr[EVAL_EXPR_MAX + 2];
    size_t len = number_to_string(n, expr, sizeof(expr), "eval", vm_get_line_number(vm));
    for (size_t i = 0; i < len; i++) {
        Coc_String var_name = {0};
        char c = expr[i];
        if (isalpha(c)) {
            coc_str_push(&var_name, c);
            Number *value = vm_get_var(vm, &var_name);
            int idx = isupper(c) ? c - 'A' : c - 'a' + 26;
            vars[idx] = value->is_float ? value->float_value : (double)value->int_value;
        }
    }
    char ctx[64];
    snprintf(ctx, sizeof(ctx), "Runtime error at line %d: ", vm_get_line_number(vm));
    n->float_value = eval_run(expr, vars, ctx);
    n->is_float = true;
}

static inline void register_builtin_actions(VM *vm) {
    int start = __LINE__;
    register_act(vm, "inc"   , act_inc);
    register_act(vm, "dec"   , act_dec);
    register_act(vm, "double", act_double);
    register_act(vm, "halve" , act_halve);
    register_act(vm, "neg"   , act_neg);
    register_act(vm, "abs"   , act_abs);
    register_act(vm, "not"   , act_not);
    register_act(vm, "isodd" , act_isodd);
    register_act(vm, "isneg" , act_isneg);
    register_act(vm, "input" , act_input);
    register_act(vm, "toint" , act_toint);
    register_act(vm, "print" , act_print);
    register_act(vm, "putn"  , act_putn);
    register_act(vm, "getc"  , act_getc);
    register_act(vm, "putc"  , act_putc);
    register_act(vm, "gets"  , act_gets);
    register_act(vm, "puts"  , act_puts);
    register_act(vm, "putl"  , act_putl);
    register_act(vm, "putx"  , act_putx);
    register_act(vm, "eval"  , act_eval);
    int end = __LINE__;
    coc_log(COC_DEBUG, "Register %d builtin actions", end - start + 1);
}

static inline VM *run_file(const char *filename, void (*register_user_actions)(VM *)) {
    Coc_String source = {0};
    if (coc_read_entire_file(filename, &source) != 0) return NULL;
    Lexer *lex = lexer_init(source);
    Parser *parser = parser_init(lex);
    parse_program(parser);
    coc_log(COC_DEBUG,
        "Compile finished: %zu instructions, %zu labels",
        parser->instructions.size, parser->labels.size);
    VM *vm = vm_init(parser);
    register_builtin_actions(vm);
    if (register_user_actions != NULL) {
        coc_log(COC_DEBUG, "Register user actions");
        register_user_actions(vm);
    }
    vm_check_labels(vm);
    run(vm);
    return vm;
}

#endif // PUNCTA_H_

/*
Recent Revision History:

1.0.8 (2026-01-15)

Added:
- STRING_LEN, PACK_LEN, EXTRA_LEN macros
- extra[EXTRA_LEN], is_extra (in Number struct)

Changed:
- number_to_string(), act_gets(), compatible with new Number struct

1.0.7 (2026-01-10)

Fixed:
- Compatibility with Coc_String APIs (coc.h v1.4.0)

1.0.6 (2026-01-10)

Changed:
- Instruction struct memory layout

1.0.5 (2026-01-08)

Changed:
- Number struct memory layout

1.0.4 (2026-01-05)

Fixed:
- String literal handling (supports arbitrary encodings)

1.0.3 (2025-12-31)

Fixed:
- '@' syntax sugar bug

1.0.2 (2025-12-31)

Added:
- emit_assign(), emit_act(), emit_jmp(), emit_jeq(), emit_label(), emit_end()

1.0.1 (2025-12-29)

Added:
- UNUSED macro

1.0.0 (2025-12-28)

Added:
- Versioned release

(2025-12-26)

Added:
- syntax sugars: `@value / @var`, `label#: / label#;`

Removed:
- Label placeholders in VM introductions

(2025-12-18)

Added:
- VM signature to act interface
- number_to_string()
- act_putn(): no-newline numeric output

Changed:
- String storage order: big-endian -> little-endian

(2025-12-16)

Added:
- Built-in actions:
  act_double(), act_halve(), act_neg(), act_abs();
  act_isodd(), act_isneg(); act_toint(); act_putl()
- Semantic label checking
- Nested comments

Fixed:
- Numeric literal validation in lexer

(2025-12-15)

Added:
- Detailed error reporting
- comment `()`
- string literal `""`

(2025-12-05)

Added:
- Hex literal support
- act_puts(), print 64-bit integer as 8-byte strings
- vm_get_var(), vm_get_action(), vm_get_label(), vm_call_label()

Changed:
- run_file() return VM pointer

*/
