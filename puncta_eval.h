#ifndef PUNCTA_EVAL_H_
#define PUNCTA_EVAL_H_
#include <math.h>
#include "coc.h"

#define EVAL_EXPR_MAX 8
#define EVAL_NODE_MAX (2 * EVAL_EXPR_MAX + 8)
#define EVAL_TOKEN_MAX EVAL_EXPR_MAX + 8

typedef enum Eval_TokenKind {
    eval_tok_eof = -1,
    eval_tok_var = -2,
    eval_tok_num = -3,

    eval_tok_add = '+',
    eval_tok_sub = '-',
    eval_tok_mul = '*',
    eval_tok_div = '/',
    eval_tok_mod = '%',
    eval_tok_pow = '^',
    eval_tok_gt  = '>',
    eval_tok_lt  = '<',
    eval_tok_eq  = '=',
    eval_tok_neq = '#'
} Eval_TokenKind;

typedef struct Eval_Token {
    Eval_TokenKind kind;
    int pos;
    int num;
    int var;
} Eval_Token;

static inline void eval_error(const char *msg, const char *expr, int pos, const char *ctx) {
    coc_log(COC_ERROR, "%sEval error at pos %d in \"%s\": %s", ctx, pos, expr, msg);
    exit(1);
}

static inline void eval_skip_whitespace(const char *s, int *pos) {
    while (isspace(s[*pos])) (*pos)++;
}

static inline Eval_Token eval_next_token(const char *s, int *pos, const char *ctx) {
    eval_skip_whitespace(s, pos);
    char c = s[*pos];
    if (c == '\0') return (Eval_Token){.kind = eval_tok_eof, .pos = *pos};
    if (isalpha(c)) {
        (*pos)++;
        return (Eval_Token){
            .kind = eval_tok_var,
            .pos = *pos,
            .var = isupper(c) ? c - 'A' : c - 'a' + 26
        };
    }
    if (isdigit(c)) {
        (*pos)++;
        return (Eval_Token){
            .kind = eval_tok_num,
            .pos = *pos,
            .num = c - '0'
        };
    }
    (*pos)++;
    if (strchr("+-*/%><#=!&|?:^", c) == NULL) {
        coc_log(COC_ERROR,
                "%sEval error at pos %d in \"%s\": unexpected character %c",
                ctx, *pos, s, c);
        exit(1);
    }
    return (Eval_Token){.kind = (Eval_TokenKind)c};
}

typedef enum Eval_NodeKind {
    EVAL_NODE_VAR,
    EVAL_NODE_NUM,
    EVAL_NODE_UNARY,
    EVAL_NODE_BINARY,
    EVAL_NODE_TERNARY
} Eval_NodeKind;

typedef struct Eval_Node Eval_Node;
struct Eval_Node {
    Eval_NodeKind kind;
    int pos;
    union {
        struct { int var; } var;
        struct { int num; } num;
        struct {
            Eval_TokenKind op; 
            Eval_Node *a;
        } unary;
        struct {
            Eval_TokenKind op;
            Eval_Node *l;
            Eval_Node *r;
        } binary;
        struct {
            Eval_TokenKind op;
            Eval_Node *c;
            Eval_Node *t;
            Eval_Node *f;
        } ternary;
    } as;
};

typedef struct Eval_NodePool {
    Eval_Node pool[EVAL_NODE_MAX];
    int used;
} Eval_NodePool;

static inline Eval_Node *eval_new_node(Eval_NodePool *p, const char *expr, int pos, const char *ctx) {
    if (p->used >= EVAL_NODE_MAX) eval_error("expression too complex", expr, pos, ctx);
    Eval_Node *n = &p->pool[p->used++];
    memset(n, 0, sizeof(*n));
    n->pos = pos;
    return n;
}

typedef struct Eval_Parser {
    Eval_NodePool *pool;
    const char *ctx;
    const char *expr;
    Eval_Token *tokens;
    int token_cnt;
    int cur;
} Eval_Parser;

static inline Eval_Token eval_parser_peek(Eval_Parser *p) {
    return p->tokens[p->cur];
}

static inline Eval_Token eval_parser_take(Eval_Parser *p) {
    return p->tokens[p->cur++];
}

static inline Eval_Node *eval_parse_ternary(Eval_Parser *p);

static inline Eval_Node *eval_parse_factor(Eval_Parser *p) {
    Eval_Token t = eval_parser_peek(p);
    int cur_pos = t.pos;
    if (t.kind == eval_tok_var) {
        eval_parser_take(p);
        Eval_Node *n = eval_new_node(p->pool, p->expr, cur_pos, p->ctx);
        n->kind = EVAL_NODE_VAR;
        n->as.var.var = t.var;
        return n;
    }
    if (t.kind == eval_tok_num) {
        eval_parser_take(p);
        Eval_Node *n = eval_new_node(p->pool, p->expr, cur_pos, p->ctx);
        n->kind = EVAL_NODE_NUM;
        n->as.num.num = t.num;
        return n;
    }
    eval_error("expected variable (A-Z, a-z) or digit (0-9)", p->expr, cur_pos, p->ctx);
    return NULL;
}

static inline Eval_Node *eval_parse_pow(Eval_Parser *p) {
    Eval_Node *left = eval_parse_factor(p);
    if (eval_parser_peek(p).kind == '^') {
        Eval_Token t = eval_parser_take(p);
        Eval_Node *right = eval_parse_pow(p);
        Eval_Node *n = eval_new_node(p->pool, p->expr, t.pos, p->ctx);
        n->kind = EVAL_NODE_BINARY;
        n->as.binary.op = t.kind;
        n->as.binary.l = left;
        n->as.binary.r = right;
        return n;
    }
    return left;
}

static inline Eval_Node *eval_parse_unary(Eval_Parser *p) {
    Eval_Token t = eval_parser_peek(p);
    int cur_pos = t.pos;
    if (t.kind == '!' || t.kind == '-') {
        eval_parser_take(p);
        Eval_Node *child = eval_parse_pow(p);
        Eval_Node *n = eval_new_node(p->pool, p->expr, cur_pos, p->ctx);
        n->kind = EVAL_NODE_UNARY;
        n->as.unary.op = t.kind;
        n->as.unary.a = child;
        return n;
    }
    return eval_parse_pow(p);
}

static inline Eval_Node *eval_parse_mul(Eval_Parser *p) {
    Eval_Node *left = eval_parse_unary(p);
    while (true) {
        Eval_TokenKind k = eval_parser_peek(p).kind;
        if (k != '*' && k != '/' && k != '%') break;
        Eval_Token op = eval_parser_take(p);
        Eval_Node *right = eval_parse_unary(p);
        Eval_Node *n = eval_new_node(p->pool, p->expr, op.pos, p->ctx);
        n->kind = EVAL_NODE_BINARY;
        n->as.binary.op = op.kind;
        n->as.binary.l = left;
        n->as.binary.r = right;
        left = n;
    }
    return left;
}

static inline Eval_Node *eval_parse_add(Eval_Parser *p) {
    Eval_Node *left = eval_parse_mul(p);
    while (true) {
        Eval_TokenKind k = eval_parser_peek(p).kind;
        if (k != '+' && k != '-') break;
        Eval_Token op = eval_parser_take(p);
        Eval_Node *right = eval_parse_mul(p);
        Eval_Node *n = eval_new_node(p->pool, p->expr, op.pos, p->ctx);
        n->kind = EVAL_NODE_BINARY;
        n->as.binary.op = op.kind;
        n->as.binary.l = left;
        n->as.binary.r = right;
        left = n;
    }
    return left;
}

static inline bool eval_is_cmp(Eval_TokenKind k) {
    return k == '>' || k == '<' || k == '=' || k == '#';
}

static inline Eval_Node *eval_parse_cmp(Eval_Parser *p) {
    Eval_Node *left = eval_parse_add(p);
    if (!eval_is_cmp(eval_parser_peek(p).kind)) return left;
    Eval_Token op = eval_parser_take(p);
    Eval_Node *right = eval_parse_add(p);
    Eval_Node *n = eval_new_node(p->pool, p->expr, op.pos, p->ctx);
    n->kind = EVAL_NODE_BINARY;
    n->as.binary.op = op.kind;
    n->as.binary.l = left;
    n->as.binary.r = right;
    if (eval_is_cmp(eval_parser_peek(p).kind)) {
        eval_error("chained comparisons are not supported; use '&' to combine (e.g., A<B & B<C)",
                   p->expr, eval_parser_peek(p).pos, p->ctx);
    }
    return n;
}

static inline Eval_Node *eval_parse_and(Eval_Parser *p) {
    Eval_Node *left = eval_parse_cmp(p);
    while (true) {
        if (eval_parser_peek(p).kind != '&') break;
        Eval_Token op = eval_parser_take(p);
        Eval_Node *right = eval_parse_cmp(p);
        Eval_Node *n = eval_new_node(p->pool, p->expr, op.pos, p->ctx);
        n->kind = EVAL_NODE_BINARY;
        n->as.binary.op = op.kind;
        n->as.binary.l = left;
        n->as.binary.r = right;
        left = n;
    }
    return left;
}

static inline Eval_Node *eval_parse_or(Eval_Parser *p) {
    Eval_Node *left = eval_parse_and(p);
    while (true) {
        if (eval_parser_peek(p).kind != '|') break;
        Eval_Token op = eval_parser_take(p);
        Eval_Node *right = eval_parse_and(p);
        Eval_Node *n = eval_new_node(p->pool, p->expr, op.pos, p->ctx);
        n->kind = EVAL_NODE_BINARY;
        n->as.binary.op = op.kind;
        n->as.binary.l = left;
        n->as.binary.r = right;
        left = n;
    }
    return left;
}

static inline Eval_Node *eval_parse_ternary(Eval_Parser *p) {
    Eval_Node *cond = eval_parse_or(p);
    if (eval_parser_peek(p).kind != '?') return cond;
    Eval_Token q = eval_parser_take(p);
    Eval_Node *t = eval_parse_or(p);
    if (eval_parser_peek(p).kind != ':') {
        eval_error("expected ':'", p->expr, eval_parser_peek(p).pos, p->ctx);
    }
    eval_parser_take(p);
    Eval_Node *f = eval_parse_or(p);
    Eval_Node *n = eval_new_node(p->pool, p->expr, q.pos, p->ctx);
    n->kind = EVAL_NODE_TERNARY;
    n->as.ternary.c = cond;
    n->as.ternary.t = t;
    n->as.ternary.f = f;
    return n;
}

static inline bool truthy(double x) {
    return x != 0.0;
}

static inline bool is_equal_float(double a, double b) {
    const double eps = 1e-9;
    return fabs(a - b) < eps;
}

static inline double eval_node(const Eval_Node *n, const double vars[52], const char *expr, const char *ctx) {
    switch (n->kind) {
        case EVAL_NODE_NUM: return (double)n->as.num.num;
        case EVAL_NODE_VAR: return vars[n->as.var.var];
        case EVAL_NODE_UNARY: {
            double a = eval_node(n->as.unary.a, vars, expr, ctx);
            if (n->as.unary.op == '!') return truthy(a) ? 0.0 : 1.0;
            if (n->as.unary.op == '-') return -a;
            eval_error("unknown unary op", expr, n->pos, ctx);
        }
        case EVAL_NODE_BINARY: {
            Eval_TokenKind op = n->as.binary.op;
            if (op == '&') {
                double l = eval_node(n->as.binary.l, vars, expr, ctx);
                if (!truthy(l)) return 0.0;
                double r = eval_node(n->as.binary.r, vars, expr, ctx);
                return truthy(r) ? 1.0 : 0.0;
            }
            if (op == '|') {
                double l = eval_node(n->as.binary.l, vars, expr, ctx);
                if (truthy(l)) return 1.0;
                double r = eval_node(n->as.binary.r, vars, expr, ctx);
                return truthy(r) ? 1.0 : 0.0;
            }
            double l = eval_node(n->as.binary.l, vars, expr, ctx);
            double r = eval_node(n->as.binary.r, vars, expr, ctx);
            switch (op) {
            case '+': return l + r;
            case '-': return l - r;
            case '*': return l * r;
            case '/':
                if (r == 0.0) eval_error("division by zero", expr, n->pos, ctx);
                return l / r;
            case '%': {
                if (!isfinite(l) || !isfinite(r))
                    eval_error("mod expects finite numbers", expr, n->pos, ctx);
                int64_t a = (int64_t)l;
                int64_t b = (int64_t)r;
                if (b == 0) eval_error("mod by zero", expr, n->pos, ctx);
                return (double)(a % b);
            }
            case '^': {
                if (!isfinite(l) || !isfinite(r))
                    eval_error("pow requires finite numbers", expr, n->pos, ctx);
                if (!(l > 0.0) || !(r > 0.0)) 
                    eval_error("pow base and exponent must be > 0", expr, n->pos, ctx);
                if (r == 0.5) return sqrt(l);
                double res = pow(l, r);
                if (!isfinite(res)) {
                    coc_log(COC_ERROR,
                            "%sEval error at pos %d in \"%s\": %g^%g overflow",
                            ctx, n->pos, expr, l, r);
                    exit(1);
                }
                return res;
            }
            case '>': return (l > r) ? 1.0 : 0.0;
            case '<': return (l < r) ? 1.0 : 0.0;
            case '=': return is_equal_float(l, r) ? 1.0 : 0.0;
            case '#': return !is_equal_float(l, r) ? 1.0 : 0.0;
            default:
                eval_error("unknown binary op", expr, n->pos, ctx);
            }
        }
        case EVAL_NODE_TERNARY: {
            double cond = eval_node(n->as.ternary.c, vars, expr, ctx);
            if (truthy(cond)) return eval_node(n->as.ternary.t, vars, expr, ctx);
            return eval_node(n->as.ternary.f, vars, expr, ctx); 
        }
    }
    eval_error("unknown node kind", expr, n->pos, ctx);
    return 0.0;
}

static inline double eval_run(const char *expr, const double vars[52], const char *ctx) {
    Eval_Token tokens[EVAL_TOKEN_MAX];
    int token_cnt = 0, pos = 0;
    while (true) {
        Eval_Token t = eval_next_token(expr, &pos, ctx);
        if (token_cnt >= EVAL_TOKEN_MAX) eval_error("too many tokens", expr, pos, ctx);
        tokens[token_cnt++] = t;
        if (t.kind == eval_tok_eof) break;
    }
    Eval_NodePool pool = {0};
    Eval_Parser p = {
        .pool = &pool,
        .tokens = tokens, .token_cnt = token_cnt,
        .ctx = ctx,
        .expr = expr,
        .cur = 0 
    };
    Eval_Node *root = eval_parse_ternary(&p);
    if (eval_parser_peek(&p).kind != eval_tok_eof)
        eval_error("trailing garbage", expr, eval_parser_peek(&p).pos, ctx);
    return eval_node(root, vars, expr, ctx);
}

#endif
