/* Chopcap parser: a recursive-descent parser producing a small AST. */
#include "chopcap.h"

typedef struct {
    Interp *in;
    Token *t;
    int pos;
    bool failed;
} P;

static Node *statement(P *p);
static Node *expression(P *p);
static Node *block_after_colon(P *p, const char *what);

static Node *node_new(NodeType type, int line, int col) {
    Node *n = xmalloc(sizeof(Node));
    memset(n, 0, sizeof(Node));
    n->type = type; n->line = line; n->col = col;
    n->literal = v_nothing();
    return n;
}

static void kid_add(Node *n, Node *k) {
    n->kids = xrealloc(n->kids, sizeof(Node *) * (n->nkids + 1));
    n->kids[n->nkids++] = k;
}

void node_free(Node *n) {
    if (!n) return;
    for (int i = 0; i < n->nkids; i++) node_free(n->kids[i]);
    free(n->kids);
    for (int i = 0; i < n->nparams; i++) free(n->params[i]);
    free(n->params);
    free(n->name);
    free(n->name2);
    v_release(n->literal);
    node_free(n->a); node_free(n->b); node_free(n->c); node_free(n->d);
    free(n);
}

static Token *cur(P *p)  { return &p->t[p->pos]; }
static TokType tt(P *p)  { return p->t[p->pos].type; }
static bool check(P *p, TokType type) { return tt(p) == type; }

static Token *advance(P *p) {
    Token *t = &p->t[p->pos];
    if (t->type != T_EOF) p->pos++;
    return t;
}

static bool match(P *p, TokType type) {
    if (check(p, type)) { advance(p); return true; }
    return false;
}

static const char *describe(Token *t) {
    static char buf[80];
    switch (t->type) {
    case T_EOF:     return "the end of the file";
    case T_NEWLINE: return "the end of the line";
    case T_INDENT:  return "an indented line";
    case T_DEDENT:  return "the end of the block";
    case T_TEXT:    return "a text value";
    case T_INT: case T_FLOAT: return "a number";
    case T_COLON:   return "`:`";
    case T_ASSIGN:  return "`=`";
    case T_LPAREN:  return "`(`";
    case T_RPAREN:  return "`)`";
    case T_LBRACKET: return "`[`";
    case T_RBRACKET: return "`]`";
    case T_COMMA:   return "`,`";
    case T_DOT:     return "`.`";
    default:
        if (t->text) { snprintf(buf, sizeof buf, "`%s`", t->text); return buf; }
        return "that";
    }
}

/* Reports a parse error once; later calls are ignored. */
static void perr(P *p, Token *t, const char *hint, const char *fmt, ...);

#include <stdarg.h>
static void perr(P *p, Token *t, const char *hint, const char *fmt, ...) {
    if (p->failed) return;
    p->failed = true;
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    if (hint) raise_hint(p->in, t->line, hint, "%s", msg);
    else      raise(p->in, t->line, "%s", msg);
}

static bool expect(P *p, TokType type, const char *hint, const char *what) {
    if (match(p, type)) return true;
    perr(p, cur(p), hint, "expected %s but found %s.", what, describe(cur(p)));
    return false;
}

static void skip_newlines(P *p) { while (check(p, T_NEWLINE)) advance(p); }

/* `to` and `by` are only special inside a counting `for`, so they stay
 * usable as ordinary variable and parameter names everywhere else. */
static bool match_word(P *p, const char *word) {
    if (check(p, T_NAME) && cur(p)->text && strcmp(cur(p)->text, word) == 0) {
        advance(p);
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Expressions                                                         */
/* ------------------------------------------------------------------ */

static Node *primary(P *p) {
    Token *t = cur(p);
    switch (t->type) {
    case T_INT:  advance(p); { Node *n = node_new(N_LITERAL, t->line, t->col); n->literal = v_int(t->ival); return n; }
    case T_FLOAT:advance(p); { Node *n = node_new(N_LITERAL, t->line, t->col); n->literal = v_float(t->fval); return n; }
    case T_TEXT: advance(p); { Node *n = node_new(N_LITERAL, t->line, t->col); n->literal = v_text_len(t->text, t->textlen); return n; }
    case T_TRUE: advance(p); { Node *n = node_new(N_LITERAL, t->line, t->col); n->literal = v_bool(true); return n; }
    case T_FALSE:advance(p); { Node *n = node_new(N_LITERAL, t->line, t->col); n->literal = v_bool(false); return n; }
    case T_NOTHING: advance(p); return node_new(N_LITERAL, t->line, t->col);
    case T_NAME: {
        advance(p);
        Node *n = node_new(N_NAME, t->line, t->col);
        n->name = xstrdup(t->text);
        return n;
    }
    case T_ASK: {
        advance(p);
        Node *n = node_new(N_ASK, t->line, t->col);
        if (!check(p, T_NEWLINE) && !check(p, T_EOF) && !check(p, T_RPAREN) &&
            !check(p, T_COLON) && !check(p, T_COMMA) && !check(p, T_RBRACKET))
            n->a = expression(p);
        return n;
    }
    case T_LPAREN: {
        advance(p);
        Node *n = expression(p);
        if (!expect(p, T_RPAREN, NULL, "`)`")) { node_free(n); return NULL; }
        return n;
    }
    case T_LBRACKET: {
        advance(p);
        Node *n = node_new(N_LISTLIT, t->line, t->col);
        skip_newlines(p);
        if (!check(p, T_RBRACKET)) {
            do {
                skip_newlines(p);
                if (check(p, T_RBRACKET)) break;   /* a trailing comma is fine */
                Node *e = expression(p);
                if (!e) { node_free(n); return NULL; }
                kid_add(n, e);
                skip_newlines(p);
            } while (match(p, T_COMMA));
        }
        if (!expect(p, T_RBRACKET, NULL, "`]` to close this list")) { node_free(n); return NULL; }
        return n;
    }
    default:
        perr(p, t, NULL, "expected a value but found %s.", describe(t));
        return NULL;
    }
}

static Node *postfix(P *p) {
    Node *n = primary(p);
    if (!n) return NULL;
    for (;;) {
        Token *t = cur(p);
        if (match(p, T_LPAREN)) {
            Node *call = node_new(N_CALL, t->line, t->col);
            call->a = n;
            skip_newlines(p);
            if (!check(p, T_RPAREN)) {
                do {
                    skip_newlines(p);
                    if (check(p, T_RPAREN)) break;   /* a trailing comma is fine */
                    Node *arg = expression(p);
                    if (!arg) { node_free(call); return NULL; }
                    kid_add(call, arg);
                    skip_newlines(p);
                } while (match(p, T_COMMA));
            }
            if (!expect(p, T_RPAREN, NULL, "`)` to close this call")) { node_free(call); return NULL; }
            n = call;
        } else if (match(p, T_LBRACKET)) {
            Node *idx = node_new(N_INDEX, t->line, t->col);
            idx->a = n;
            idx->b = expression(p);
            if (!idx->b) { node_free(idx); return NULL; }
            if (!expect(p, T_RBRACKET, NULL, "`]`")) { node_free(idx); return NULL; }
            n = idx;
        } else if (match(p, T_DOT)) {
            if (!check(p, T_NAME)) {
                perr(p, cur(p), NULL, "expected a name after `.` but found %s.", describe(cur(p)));
                node_free(n);
                return NULL;
            }
            Token *m = advance(p);
            Node *dot = node_new(N_DOT, m->line, m->col);
            dot->a = n;
            dot->name = xstrdup(m->text);
            n = dot;
        } else {
            return n;
        }
    }
}

static Node *unary(P *p);

static Node *power(P *p) {
    Node *base = postfix(p);
    if (!base) return NULL;
    Token *t = cur(p);
    if (match(p, T_CARET)) {
        Node *n = node_new(N_BINARY, t->line, t->col);
        n->op = T_CARET;
        n->a = base;
        n->b = unary(p);           /* right associative, binds -x on the right */
        if (!n->b) { node_free(n); return NULL; }
        return n;
    }
    return base;
}

static Node *unary(P *p) {
    Token *t = cur(p);
    if (check(p, T_MINUS) || check(p, T_NOT)) {
        advance(p);
        Node *n = node_new(N_UNARY, t->line, t->col);
        n->op = t->type;
        n->a = unary(p);
        if (!n->a) { node_free(n); return NULL; }
        return n;
    }
    return power(p);
}

static Node *binary_level(P *p, int level);

static bool level_ops(int level, TokType type) {
    switch (level) {
    case 0: return type == T_STAR || type == T_SLASH || type == T_PERCENT;
    case 1: return type == T_PLUS || type == T_MINUS;
    case 2: return type == T_EQ || type == T_NE || type == T_LT ||
                   type == T_LE || type == T_GT || type == T_GE || type == T_IN;
    default: return false;
    }
}

static Node *binary_level(P *p, int level) {
    if (level < 0) return unary(p);
    Node *left = binary_level(p, level - 1);
    if (!left) return NULL;
    while (level_ops(level, tt(p))) {
        Token *t = advance(p);
        Node *n = node_new(N_BINARY, t->line, t->col);
        n->op = t->type;
        n->a = left;
        n->b = binary_level(p, level - 1);
        if (!n->b) { node_free(n); return NULL; }
        left = n;
    }
    return left;
}

static Node *and_expr(P *p) {
    Node *left = binary_level(p, 2);
    if (!left) return NULL;
    while (check(p, T_AND)) {
        Token *t = advance(p);
        Node *n = node_new(N_AND, t->line, t->col);
        n->a = left;
        n->b = binary_level(p, 2);
        if (!n->b) { node_free(n); return NULL; }
        left = n;
    }
    return left;
}

static Node *expression(P *p) {
    Node *left = and_expr(p);
    if (!left) return NULL;
    while (check(p, T_OR)) {
        Token *t = advance(p);
        Node *n = node_new(N_OR, t->line, t->col);
        n->a = left;
        n->b = and_expr(p);
        if (!n->b) { node_free(n); return NULL; }
        left = n;
    }
    return left;
}

/* ------------------------------------------------------------------ */
/* Statements                                                          */
/* ------------------------------------------------------------------ */

static bool end_of_line(P *p) {
    if (check(p, T_EOF) || check(p, T_DEDENT)) return true;
    return expect(p, T_NEWLINE, NULL, "the end of the line");
}

static Node *block_after_colon(P *p, const char *what) {
    char hint[128];
    snprintf(hint, sizeof hint,
             "Put a `:` at the end of the `%s` line, then indent the lines below it.", what);
    if (!expect(p, T_COLON, hint, "`:`")) return NULL;
    if (!expect(p, T_NEWLINE, hint, "the end of the line after `:`")) return NULL;
    skip_newlines(p);
    if (!expect(p, T_INDENT, hint, "an indented block")) return NULL;
    Node *b = node_new(N_BLOCK, cur(p)->line, 1);
    while (!check(p, T_DEDENT) && !check(p, T_EOF)) {
        if (check(p, T_NEWLINE)) { advance(p); continue; }
        Node *s = statement(p);
        if (!s) { node_free(b); return NULL; }
        kid_add(b, s);
    }
    if (!check(p, T_EOF)) expect(p, T_DEDENT, NULL, "the end of the block");
    return b;
}

static Node *if_statement(P *p, Token *t) {
    Node *n = node_new(N_IF, t->line, t->col);
    n->a = expression(p);
    if (!n->a) { node_free(n); return NULL; }
    n->b = block_after_colon(p, "if");
    if (!n->b) { node_free(n); return NULL; }
    skip_newlines(p);
    if (check(p, T_ELSE)) {
        Token *e = advance(p);
        if (check(p, T_IF)) {
            Token *i2 = advance(p);
            n->c = if_statement(p, i2);
        } else {
            n->c = block_after_colon(p, "else");
        }
        if (!n->c) { node_free(n); return NULL; }
        (void)e;
    }
    return n;
}

static Node *statement(P *p) {
    Token *t = cur(p);
    switch (t->type) {
    case T_SAY: {
        advance(p);
        Node *n = node_new(N_SAY, t->line, t->col);
        if (!check(p, T_NEWLINE) && !check(p, T_EOF)) {
            do {
                Node *e = expression(p);
                if (!e) { node_free(n); return NULL; }
                kid_add(n, e);
            } while (match(p, T_COMMA));
        }
        if (!end_of_line(p)) { node_free(n); return NULL; }
        return n;
    }
    case T_IF: {
        advance(p);
        return if_statement(p, t);
    }
    case T_WHILE: {
        advance(p);
        Node *n = node_new(N_WHILE, t->line, t->col);
        n->a = expression(p);
        if (!n->a) { node_free(n); return NULL; }
        n->b = block_after_colon(p, "while");
        if (!n->b) { node_free(n); return NULL; }
        return n;
    }
    case T_FOR: {
        advance(p);
        if (!check(p, T_NAME)) {
            perr(p, cur(p), "Write `for item in things:` or `for i in 1 to 10:`.",
                 "expected a variable name after `for` but found %s.", describe(cur(p)));
            return NULL;
        }
        Token *var = advance(p);
        if (!expect(p, T_IN, "Write `for item in things:`.", "`in`")) return NULL;
        Node *first = expression(p);
        if (!first) return NULL;
        if (match_word(p, "to")) {
            Node *n = node_new(N_FORRANGE, t->line, t->col);
            n->name = xstrdup(var->text);
            n->a = first;
            n->b = expression(p);
            if (!n->b) { node_free(n); return NULL; }
            if (match_word(p, "by")) {
                n->d = expression(p);
                if (!n->d) { node_free(n); return NULL; }
            }
            n->c = block_after_colon(p, "for");
            if (!n->c) { node_free(n); return NULL; }
            return n;
        }
        Node *n = node_new(N_FOR, t->line, t->col);
        n->name = xstrdup(var->text);
        n->a = first;
        n->b = block_after_colon(p, "for");
        if (!n->b) { node_free(n); return NULL; }
        return n;
    }
    case T_FUN: {
        advance(p);
        if (!check(p, T_NAME)) {
            perr(p, cur(p), "Write `fun add(a, b):`.",
                 "expected a function name after `fun` but found %s.", describe(cur(p)));
            return NULL;
        }
        Token *nameTok = advance(p);
        Node *n = node_new(N_FUN, t->line, t->col);
        n->name = xstrdup(nameTok->text);
        if (!expect(p, T_LPAREN, "Write `fun add(a, b):`.", "`(`")) { node_free(n); return NULL; }
        if (!check(p, T_RPAREN)) {
            do {
                if (!check(p, T_NAME)) {
                    perr(p, cur(p), NULL, "expected a parameter name but found %s.", describe(cur(p)));
                    node_free(n);
                    return NULL;
                }
                Token *pt = advance(p);
                n->params = xrealloc(n->params, sizeof(char *) * (n->nparams + 1));
                n->params[n->nparams++] = xstrdup(pt->text);
            } while (match(p, T_COMMA));
        }
        if (!expect(p, T_RPAREN, NULL, "`)`")) { node_free(n); return NULL; }
        n->a = block_after_colon(p, "fun");
        if (!n->a) { node_free(n); return NULL; }
        return n;
    }
    case T_RETURN: {
        advance(p);
        Node *n = node_new(N_RETURN, t->line, t->col);
        if (!check(p, T_NEWLINE) && !check(p, T_EOF) && !check(p, T_DEDENT)) {
            n->a = expression(p);
            if (!n->a) { node_free(n); return NULL; }
        }
        if (!end_of_line(p)) { node_free(n); return NULL; }
        return n;
    }
    case T_BREAK: case T_SKIP: {
        advance(p);
        Node *n = node_new(t->type == T_BREAK ? N_BREAK : N_SKIP, t->line, t->col);
        if (!end_of_line(p)) { node_free(n); return NULL; }
        return n;
    }
    case T_ERROR: {
        advance(p);
        Node *n = node_new(N_ERROR, t->line, t->col);
        n->a = expression(p);
        if (!n->a) { node_free(n); return NULL; }
        if (!end_of_line(p)) { node_free(n); return NULL; }
        return n;
    }
    case T_TRY: {
        advance(p);
        Node *n = node_new(N_TRY, t->line, t->col);
        n->a = block_after_colon(p, "try");
        if (!n->a) { node_free(n); return NULL; }
        skip_newlines(p);
        if (!expect(p, T_CATCH, "Every `try:` block needs a `catch problem:` block after it.", "`catch`")) {
            node_free(n);
            return NULL;
        }
        if (check(p, T_NAME)) {
            Token *cv = advance(p);
            n->name2 = xstrdup(cv->text);
        }
        n->b = block_after_colon(p, "catch");
        if (!n->b) { node_free(n); return NULL; }
        return n;
    }
    case T_USE: {
        advance(p);
        Node *n = node_new(N_USE, t->line, t->col);
        if (check(p, T_TEXT)) {
            Token *path = advance(p);
            n->op = T_TEXT;
            n->name = xstrdup(path->text);
        } else if (check(p, T_NAME)) {
            Token *mod = advance(p);
            n->op = T_NAME;
            n->name = xstrdup(mod->text);
        } else {
            perr(p, cur(p), "Write `use math` or `use \"helpers.chop\" as helpers`.",
                 "expected a module name after `use` but found %s.", describe(cur(p)));
            node_free(n);
            return NULL;
        }
        if (match(p, T_AS)) {
            if (!check(p, T_NAME)) {
                perr(p, cur(p), NULL, "expected a name after `as` but found %s.", describe(cur(p)));
                node_free(n);
                return NULL;
            }
            n->name2 = xstrdup(advance(p)->text);
        } else if (n->op == T_TEXT) {
            perr(p, t, "Write `use \"helpers.chop\" as helpers`.",
                 "loading a file needs a name: add `as something` at the end.");
            node_free(n);
            return NULL;
        }
        if (!end_of_line(p)) { node_free(n); return NULL; }
        return n;
    }
    case T_ELSE:
        perr(p, t, "An `else` must follow an `if` block at the same indentation.",
             "`else` here has no matching `if`.");
        return NULL;
    case T_CATCH:
        perr(p, t, "A `catch` must follow a `try` block at the same indentation.",
             "`catch` here has no matching `try`.");
        return NULL;
    default: break;
    }

    /* Assignment or bare expression. */
    Node *lhs = expression(p);
    if (!lhs) return NULL;
    if (check(p, T_ASSIGN)) {
        Token *eq = advance(p);
        Node *value = expression(p);
        if (!value) { node_free(lhs); return NULL; }
        Node *n;
        if (lhs->type == N_NAME) {
            n = node_new(N_ASSIGN, lhs->line, lhs->col);
            n->name = xstrdup(lhs->name);
            n->a = value;
            node_free(lhs);
        } else if (lhs->type == N_INDEX) {
            n = node_new(N_INDEX_ASSIGN, lhs->line, lhs->col);
            n->a = lhs->a; lhs->a = NULL;
            n->b = lhs->b; lhs->b = NULL;
            n->c = value;
            node_free(lhs);
        } else {
            perr(p, eq, "Only variables and list positions can be assigned to.",
                 "this is not something you can assign a value to.");
            node_free(lhs);
            node_free(value);
            return NULL;
        }
        if (!end_of_line(p)) { node_free(n); return NULL; }
        return n;
    }
    Node *n = node_new(N_EXPRSTMT, lhs->line, lhs->col);
    n->a = lhs;
    if (!end_of_line(p)) { node_free(n); return NULL; }
    return n;
}

Node *parse(Interp *in, TokenList *toks) {
    P p;
    p.in = in; p.t = toks->items; p.pos = 0; p.failed = false;
    Node *program = node_new(N_BLOCK, 1, 1);
    for (;;) {
        while (check(&p, T_NEWLINE) || check(&p, T_DEDENT) || check(&p, T_INDENT)) {
            if (check(&p, T_INDENT)) {
                perr(&p, cur(&p), "Top-level lines should start at the left margin.",
                     "this line is indented but nothing above it opened a block.");
                node_free(program);
                return NULL;
            }
            advance(&p);
        }
        if (check(&p, T_EOF)) break;
        Node *s = statement(&p);
        if (!s) { node_free(program); return NULL; }
        kid_add(program, s);
    }
    return program;
}
