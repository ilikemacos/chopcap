/* Chopcap lexer: turns source text into tokens, including the
 * INDENT / DEDENT tokens that give Chopcap its indentation-based blocks. */
#include "chopcap.h"
#include <ctype.h>

typedef struct {
    Interp *in;
    const char *s;
    size_t i, n;
    int line, col;
    int paren;              /* bracket depth; newlines are ignored inside */
    int indents[64];
    int nindents;
    Token *out;
    int count, cap;
    bool failed;
} Lexer;

static void emit(Lexer *L, Token t) {
    if (L->count == L->cap) {
        L->cap = L->cap ? L->cap * 2 : 64;
        L->out = xrealloc(L->out, sizeof(Token) * L->cap);
    }
    L->out[L->count++] = t;
}

static Token mk(Lexer *L, TokType type, int line, int col) {
    Token t;
    memset(&t, 0, sizeof t);
    t.type = type; t.line = line; t.col = col;
    (void)L;
    return t;
}

static struct { const char *word; TokType type; } KEYWORDS[] = {
    {"say", T_SAY}, {"ask", T_ASK}, {"if", T_IF}, {"else", T_ELSE},
    {"while", T_WHILE}, {"for", T_FOR}, {"in", T_IN},
    {"fun", T_FUN}, {"return", T_RETURN}, {"break", T_BREAK}, {"skip", T_SKIP},
    {"and", T_AND}, {"or", T_OR}, {"not", T_NOT},
    {"true", T_TRUE}, {"false", T_FALSE}, {"nothing", T_NOTHING},
    {"use", T_USE}, {"as", T_AS}, {"try", T_TRY}, {"catch", T_CATCH},
    {"error", T_ERROR},
    {NULL, T_EOF}
};

static char peek(Lexer *L)  { return L->i < L->n ? L->s[L->i] : 0; }
static char peek2(Lexer *L) { return L->i + 1 < L->n ? L->s[L->i + 1] : 0; }

static char advance(Lexer *L) {
    char c = L->s[L->i++];
    if (c == '\n') { L->line++; L->col = 1; } else { L->col++; }
    return c;
}

static bool name_start(char c) { return isalpha((unsigned char)c) || c == '_'; }
static bool name_char(char c)  { return isalnum((unsigned char)c) || c == '_'; }

/* Reads the indentation of a fresh logical line and emits INDENT/DEDENT. */
static bool handle_indent(Lexer *L) {
    int width = 0;
    size_t save = L->i;
    while (L->i < L->n) {
        char c = L->s[L->i];
        if (c == ' ') { width++; advance(L); }
        else if (c == '\t') { width = (width / 4 + 1) * 4; advance(L); }
        else break;
    }
    (void)save;
    /* Blank line or comment-only line: no indentation bookkeeping. */
    char c = peek(L);
    if (c == 0 || c == '\n' || c == '#' || (c == '\r' && peek2(L) == '\n')) return true;

    int cur = L->indents[L->nindents - 1];
    if (width > cur) {
        if (L->nindents >= 63) {
            raise(L->in, L->line, "this code is nested too deeply.");
            return false;
        }
        L->indents[L->nindents++] = width;
        emit(L, mk(L, T_INDENT, L->line, 1));
    } else if (width < cur) {
        while (L->nindents > 1 && width < L->indents[L->nindents - 1]) {
            L->nindents--;
            emit(L, mk(L, T_DEDENT, L->line, 1));
        }
        if (width != L->indents[L->nindents - 1]) {
            raise_hint(L->in, L->line,
                       "Every line in the same block must start in the same column.",
                       "this line's indentation does not match any open block.");
            return false;
        }
    }
    return true;
}

static bool lex_text(Lexer *L, int line, int col) {
    char quote = advance(L); /* consume opening quote */
    size_t cap = 16, len = 0;
    char *buf = xmalloc(cap);
    for (;;) {
        if (L->i >= L->n || peek(L) == '\n') {
            free(buf);
            raise_hint(L->in, line,
                       "Text values must open and close on the same line with \".",
                       "this text value is missing its closing quote.");
            return false;
        }
        char c = advance(L);
        if (c == quote) break;
        if (c == '\\') {
            char e = advance(L);
            switch (e) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case '\\': c = '\\'; break;
            case '"': c = '"'; break;
            case '\'': c = '\''; break;
            default:
                free(buf);
                raise_hint(L->in, line,
                           "Chopcap knows \\n, \\t, \\r, \\\\, \\\" and \\'.",
                           "`\\%c` is not an escape Chopcap understands.", e);
                return false;
            }
        }
        if (len + 1 >= cap) { cap *= 2; buf = xrealloc(buf, cap); }
        buf[len++] = c;
    }
    buf[len] = 0;
    Token t = mk(L, T_TEXT, line, col);
    t.text = buf; t.textlen = len;
    emit(L, t);
    return true;
}

static bool lex_number(Lexer *L, int line, int col) {
    size_t start = L->i;
    while (isdigit((unsigned char)peek(L)) || peek(L) == '_') advance(L);
    bool isfloat = false;
    if (peek(L) == '.' && isdigit((unsigned char)peek2(L))) {
        isfloat = true;
        advance(L);
        while (isdigit((unsigned char)peek(L)) || peek(L) == '_') advance(L);
    }
    if (peek(L) == 'e' || peek(L) == 'E') {
        size_t save = L->i;
        int saveline = L->line, savecol = L->col;
        advance(L);
        if (peek(L) == '+' || peek(L) == '-') advance(L);
        if (isdigit((unsigned char)peek(L))) {
            isfloat = true;
            while (isdigit((unsigned char)peek(L))) advance(L);
        } else {
            L->i = save; L->line = saveline; L->col = savecol;
        }
    }
    /* Copy digits, dropping readability underscores. */
    char tmp[128];
    size_t k = 0;
    for (size_t j = start; j < L->i && k < sizeof(tmp) - 1; j++)
        if (L->s[j] != '_') tmp[k++] = L->s[j];
    tmp[k] = 0;

    Token t = mk(L, isfloat ? T_FLOAT : T_INT, line, col);
    if (isfloat) t.fval = strtod(tmp, NULL);
    else t.ival = strtoll(tmp, NULL, 10);
    emit(L, t);
    return true;
}

bool lex(Interp *in, const char *src, TokenList *out) {
    Lexer L;
    memset(&L, 0, sizeof L);
    L.in = in; L.s = src; L.n = strlen(src);
    L.line = 1; L.col = 1;
    L.indents[0] = 0; L.nindents = 1;

    bool at_line_start = true;

    while (L.i < L.n) {
        if (at_line_start && L.paren == 0) {
            if (!handle_indent(&L)) goto fail;
            at_line_start = false;
            if (L.i >= L.n) break;
        }
        char c = peek(&L);
        int line = L.line, col = L.col;

        if (c == '\r') { advance(&L); continue; }
        if (c == ' ' || c == '\t') { advance(&L); continue; }
        if (c == '#') { while (L.i < L.n && peek(&L) != '\n') advance(&L); continue; }
        if (c == '\n') {
            advance(&L);
            if (L.paren == 0) {
                if (L.count && L.out[L.count - 1].type != T_NEWLINE &&
                    L.out[L.count - 1].type != T_INDENT &&
                    L.out[L.count - 1].type != T_DEDENT)
                    emit(&L, mk(&L, T_NEWLINE, line, col));
                at_line_start = true;
            }
            continue;
        }
        if (c == '"' || c == '\'') { if (!lex_text(&L, line, col)) goto fail; continue; }
        if (isdigit((unsigned char)c)) { if (!lex_number(&L, line, col)) goto fail; continue; }
        if (name_start(c)) {
            size_t start = L.i;
            while (name_char(peek(&L))) advance(&L);
            size_t len = L.i - start;
            char *word = xmalloc(len + 1);
            memcpy(word, L.s + start, len);
            word[len] = 0;
            TokType kw = T_NAME;
            for (int k = 0; KEYWORDS[k].word; k++)
                if (strcmp(KEYWORDS[k].word, word) == 0) { kw = KEYWORDS[k].type; break; }
            Token t = mk(&L, kw, line, col);
            t.text = word; t.textlen = len;
            emit(&L, t);
            continue;
        }

        advance(&L);
        TokType type;
        switch (c) {
        case '+': type = T_PLUS; break;
        case '-': type = T_MINUS; break;
        case '*': type = T_STAR; break;
        case '/': type = T_SLASH; break;
        case '%': type = T_PERCENT; break;
        case '^': type = T_CARET; break;
        case ':': type = T_COLON; break;
        case ',': type = T_COMMA; break;
        case '.': type = T_DOT; break;
        case '(': type = T_LPAREN; L.paren++; break;
        case ')': type = T_RPAREN; if (L.paren) L.paren--; break;
        case '[': type = T_LBRACKET; L.paren++; break;
        case ']': type = T_RBRACKET; if (L.paren) L.paren--; break;
        case '=':
            if (peek(&L) == '=') { advance(&L); type = T_EQ; } else type = T_ASSIGN;
            break;
        case '!':
            if (peek(&L) == '=') { advance(&L); type = T_NE; }
            else {
                raise_hint(in, line, "Use `not` for negation, as in `not ready`.",
                           "`!` is not a Chopcap operator.");
                goto fail;
            }
            break;
        case '<':
            if (peek(&L) == '=') { advance(&L); type = T_LE; } else type = T_LT;
            break;
        case '>':
            if (peek(&L) == '=') { advance(&L); type = T_GE; } else type = T_GT;
            break;
        default:
            raise(in, line, "`%c` is not something Chopcap understands here.", c);
            goto fail;
        }
        emit(&L, mk(&L, type, line, col));
    }

    if (L.count && L.out[L.count - 1].type != T_NEWLINE)
        emit(&L, mk(&L, T_NEWLINE, L.line, L.col));
    while (L.nindents > 1) { L.nindents--; emit(&L, mk(&L, T_DEDENT, L.line, 1)); }
    emit(&L, mk(&L, T_EOF, L.line, L.col));

    out->items = L.out;
    out->count = L.count;
    return true;

fail:
    for (int i = 0; i < L.count; i++) free(L.out[i].text);
    free(L.out);
    out->items = NULL;
    out->count = 0;
    return false;
}

void toklist_free(TokenList *t) {
    if (!t->items) return;
    for (int i = 0; i < t->count; i++) free(t->items[i].text);
    free(t->items);
    t->items = NULL;
    t->count = 0;
}
