/* Chopcap — a small, friendly programming language.
 * Copyright (c) 2026 The Chopcap Authors. MIT licensed.
 *
 * This header declares the shared vocabulary of the implementation:
 * values, tokens, AST nodes, environments and the error reporter.
 */
#ifndef CHOPCAP_H
#define CHOPCAP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define CHOPCAP_VERSION "0.1.0"

/* ------------------------------------------------------------------ */
/* Values                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    V_NOTHING,
    V_BOOL,
    V_INT,
    V_FLOAT,
    V_TEXT,
    V_LIST,
    V_FUNC,
    V_NATIVE,
    V_MODULE
} ValueType;

typedef struct Value Value;
typedef struct Obj Obj;
typedef struct Env Env;
typedef struct Node Node;
typedef struct Interp Interp;

struct Value {
    ValueType type;
    union {
        bool b;
        long long i;
        double f;
        Obj *obj;
    } as;
};

typedef Value (*NativeFn)(Interp *in, Value *args, int argc, int line);

typedef struct {
    char *name;
    Value value;
} Slot;

struct Obj {
    int refs;
    ValueType kind;
    /* V_TEXT */
    char *text;
    size_t len;
    /* V_LIST */
    Value *items;
    int count;
    int cap;
    /* V_FUNC */
    Node *decl;
    Env *closure;
    /* V_NATIVE */
    NativeFn fn;
    int arity_min, arity_max; /* -1 max = variadic */
    const char *nname;
    /* V_MODULE */
    Slot *slots;
    int nslots;
    int slotcap;
    char *modname;
};

/* Constructors / memory */
Value v_nothing(void);
Value v_bool(bool b);
Value v_int(long long i);
Value v_float(double f);
Value v_text(const char *s);
Value v_text_len(const char *s, size_t n);
Value v_text_take(char *s, size_t n); /* takes ownership of malloc'd s */
Value v_list(void);
Value v_module(const char *name);
Value v_native(const char *name, NativeFn fn, int amin, int amax);

Value v_retain(Value v);
void v_release(Value v);

void list_push(Value list, Value item);
void module_set(Value mod, const char *name, Value v);
Value *module_get(Value mod, const char *name);

bool v_truthy(Value v);
bool v_equal(Value a, Value b);
char *v_to_text(Value v);          /* malloc'd, human display form */
char *v_repr(Value v);             /* malloc'd, quoted form for lists/REPL */
const char *v_type_name(Value v);

/* ------------------------------------------------------------------ */
/* Lexer                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    T_EOF, T_NEWLINE, T_INDENT, T_DEDENT,
    T_INT, T_FLOAT, T_TEXT, T_NAME,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT, T_CARET,
    T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE,
    T_ASSIGN, T_COLON, T_COMMA, T_DOT,
    T_LPAREN, T_RPAREN, T_LBRACKET, T_RBRACKET,
    /* keywords */
    T_SAY, T_ASK, T_IF, T_ELSE, T_WHILE, T_FOR, T_IN, T_TO, T_BY,
    T_FUN, T_RETURN, T_BREAK, T_SKIP, T_AND, T_OR, T_NOT,
    T_TRUE, T_FALSE, T_NOTHING, T_USE, T_AS, T_TRY, T_CATCH, T_ERROR
} TokType;

typedef struct {
    TokType type;
    char *text;       /* identifier name or string contents */
    size_t textlen;
    long long ival;
    double fval;
    int line;
    int col;
} Token;

typedef struct {
    Token *items;
    int count;
} TokenList;

/* Returns false on failure and reports the error. */
bool lex(Interp *in, const char *src, TokenList *out);
void toklist_free(TokenList *t);

/* ------------------------------------------------------------------ */
/* AST                                                                 */
/* ------------------------------------------------------------------ */

typedef enum {
    N_BLOCK,
    N_SAY, N_ASSIGN, N_INDEX_ASSIGN, N_EXPRSTMT,
    N_IF, N_WHILE, N_FOR, N_FORRANGE,
    N_FUN, N_RETURN, N_BREAK, N_SKIP,
    N_USE, N_TRY, N_ERROR,
    N_LITERAL, N_NAME, N_BINARY, N_UNARY, N_CALL,
    N_INDEX, N_DOT, N_LISTLIT, N_ASK, N_AND, N_OR
} NodeType;

struct Node {
    NodeType type;
    int line;
    int col;
    /* generic children */
    Node **kids;
    int nkids;
    /* payload */
    char *name;       /* identifier / member / module alias */
    char *name2;      /* alias, catch var */
    Value literal;    /* N_LITERAL */
    TokType op;       /* binary / unary operator */
    char **params;    /* N_FUN */
    int nparams;
    Node *a, *b, *c, *d;
};

Node *parse(Interp *in, TokenList *toks); /* NULL on error */
void node_free(Node *n);

/* ------------------------------------------------------------------ */
/* Environments                                                        */
/* ------------------------------------------------------------------ */

struct Env {
    int refs;
    Env *parent;
    Slot *slots;
    int count;
    int cap;
};

Env *env_new(Env *parent);
Env *env_retain(Env *e);
void env_release(Env *e);
Value *env_lookup(Env *e, const char *name);
void env_define(Env *e, const char *name, Value v);
bool env_assign(Env *e, const char *name, Value v);

/* ------------------------------------------------------------------ */
/* Interpreter                                                         */
/* ------------------------------------------------------------------ */

typedef enum { FLOW_NORMAL, FLOW_RETURN, FLOW_BREAK, FLOW_SKIP, FLOW_ERROR } Flow;

struct Interp {
    Env *globals;
    const char *src;        /* current source text, for error snippets */
    const char *filename;
    Flow flow;
    Value retval;
    char *errmsg;           /* set when flow == FLOW_ERROR */
    char *errhint;
    char *errsnippet;       /* the offending source line, captured at raise time */
    char *errfile;
    int errline;
    bool errprinted;        /* friendly report already emitted */
    int depth;              /* call depth guard */
    bool repl;
    char *basedir;          /* directory of the running script, for `use "file"` */
};

void interp_init(Interp *in);
void interp_free(Interp *in);
int  run_source(Interp *in, const char *src, const char *filename);
Value eval(Interp *in, Node *n, Env *env);
void exec_block(Interp *in, Node *block, Env *env);
Value call_value(Interp *in, Value fn, Value *args, int argc, int line);

/* Errors: raise sets flow = FLOW_ERROR with a friendly message. */
void raise(Interp *in, int line, const char *fmt, ...);
void raise_hint(Interp *in, int line, const char *hint, const char *fmt, ...);
void report_error(Interp *in);
char *nearest_name(Env *env, const char *name); /* did-you-mean, or NULL */

/* Builtins & standard library */
void install_builtins(Interp *in);
Value load_module(Interp *in, const char *name, int line); /* v_nothing if unknown */

/* Helpers shared by builtins */
bool arg_number(Interp *in, Value v, double *out, const char *fname, int idx, int line);
char *xstrdup(const char *s);
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);

#endif /* CHOPCAP_H */
