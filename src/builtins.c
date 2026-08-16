/* Chopcap built-in functions and standard library modules.
 *
 * Everything here is written against the same tiny native-function
 * signature, so adding a library function is a one-line registration. */
#include "chopcap.h"
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* Argument helpers                                                    */
/* ------------------------------------------------------------------ */

static const char *ORDINALS[] = {"first", "second", "third", "fourth", "fifth", "sixth"};

static const char *ordinal(int i) { return i < 6 ? ORDINALS[i] : "next"; }

static bool want_text(Interp *in, Value v, const char *fn, int i, int line) {
    if (v.type == V_TEXT) return true;
    raise(in, line, "`%s` needs text as its %s value, but got %s.",
          fn, ordinal(i), v_type_name(v));
    return false;
}

static bool want_list(Interp *in, Value v, const char *fn, int i, int line) {
    if (v.type == V_LIST) return true;
    raise(in, line, "`%s` needs a list as its %s value, but got %s.",
          fn, ordinal(i), v_type_name(v));
    return false;
}

static bool want_num(Interp *in, Value v, const char *fn, int i, int line, double *out) {
    if (v.type == V_INT)   { *out = (double)v.as.i; return true; }
    if (v.type == V_FLOAT) { *out = v.as.f; return true; }
    raise(in, line, "`%s` needs a number as its %s value, but got %s.",
          fn, ordinal(i), v_type_name(v));
    return false;
}

static bool want_int(Interp *in, Value v, const char *fn, int i, int line, long long *out) {
    if (v.type == V_INT) { *out = v.as.i; return true; }
    if (v.type == V_FLOAT && v.as.f == floor(v.as.f)) { *out = (long long)v.as.f; return true; }
    raise(in, line, "`%s` needs a whole number as its %s value, but got %s.",
          fn, ordinal(i), v_type_name(v));
    return false;
}

bool arg_number(Interp *in, Value v, double *out, const char *fname, int idx, int line) {
    return want_num(in, v, fname, idx, line, out);
}

/* Wraps a double back into an int when it is exact, so `math.floor(2.7)`
 * reads as `2` rather than `2.0`. */
static Value num_result(double d) {
    if (d == floor(d) && fabs(d) < 9.0e15 && !isinf(d)) return v_int((long long)d);
    return v_float(d);
}

/* ------------------------------------------------------------------ */
/* Global built-ins                                                    */
/* ------------------------------------------------------------------ */

static Value bi_len(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    switch (a[0].type) {
    case V_TEXT: return v_int((long long)a[0].as.obj->len);
    case V_LIST: return v_int(a[0].as.obj->count);
    default:
        raise_hint(in, line, "`len` works with text and lists.",
                   "cannot measure the length of %s.", v_type_name(a[0]));
        return v_nothing();
    }
}

static Value bi_text(Interp *in, Value *a, int argc, int line) {
    (void)in; (void)argc; (void)line;
    char *s = v_to_text(a[0]);
    return v_text_take(s, strlen(s));
}

static Value bi_number(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (a[0].type == V_INT || a[0].type == V_FLOAT) return v_retain(a[0]);
    if (a[0].type == V_BOOL) return v_int(a[0].as.b ? 1 : 0);
    if (a[0].type == V_TEXT) {
        const char *s = a[0].as.obj->text;
        while (*s == ' ' || *s == '\t') s++;
        char *end = NULL;
        double d = strtod(s, &end);
        if (end == s) {
            raise_hint(in, line, "Only text that looks like a number can be converted.",
                       "cannot turn \"%s\" into a number.", a[0].as.obj->text);
            return v_nothing();
        }
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
        if (*end) {
            raise_hint(in, line, "Only text that looks like a number can be converted.",
                       "cannot turn \"%s\" into a number.", a[0].as.obj->text);
            return v_nothing();
        }
        if (d == floor(d) && !strchr(a[0].as.obj->text, '.') &&
            !strchr(a[0].as.obj->text, 'e') && fabs(d) < 9.0e15)
            return v_int((long long)d);
        return v_float(d);
    }
    raise(in, line, "cannot turn %s into a number.", v_type_name(a[0]));
    return v_nothing();
}

static Value bi_whole(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (a[0].type == V_INT) return v_retain(a[0]);
    if (a[0].type == V_FLOAT) return v_int((long long)(a[0].as.f < 0 ? ceil(a[0].as.f) : floor(a[0].as.f)));
    if (a[0].type == V_TEXT) {
        Value n = bi_number(in, a, 1, line);
        if (in->flow == FLOW_ERROR) return v_nothing();
        Value r = n.type == V_FLOAT ? v_int((long long)n.as.f) : v_retain(n);
        v_release(n);
        return r;
    }
    raise(in, line, "cannot turn %s into a whole number.", v_type_name(a[0]));
    return v_nothing();
}

static Value bi_type(Interp *in, Value *a, int argc, int line) {
    (void)in; (void)argc; (void)line;
    return v_text(v_type_name(a[0]));
}

static Value bi_push(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "push", 0, line)) return v_nothing();
    list_push(a[0], v_retain(a[1]));
    return v_retain(a[0]);
}

static Value bi_pop(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "pop", 0, line)) return v_nothing();
    Obj *o = a[0].as.obj;
    if (o->count == 0) {
        raise_hint(in, line, "Check the list is not empty before taking from it.",
                   "cannot take the last item from an empty list.");
        return v_nothing();
    }
    return o->items[--o->count];   /* ownership moves to the caller */
}

/* ------------------------------------------------------------------ */
/* math                                                                */
/* ------------------------------------------------------------------ */

static Value m_sqrt(Interp *in, Value *a, int argc, int line) {
    (void)argc; double x;
    if (!want_num(in, a[0], "sqrt", 0, line, &x)) return v_nothing();
    if (x < 0) {
        raise_hint(in, line, "Square roots need a value of 0 or more.",
                   "cannot take the square root of a negative number.");
        return v_nothing();
    }
    return num_result(sqrt(x));
}

static Value m_abs(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (a[0].type == V_INT) return v_int(a[0].as.i < 0 ? -a[0].as.i : a[0].as.i);
    double x;
    if (!want_num(in, a[0], "abs", 0, line, &x)) return v_nothing();
    return v_float(fabs(x));
}

static Value m_floor(Interp *in, Value *a, int argc, int line) {
    (void)argc; double x;
    if (!want_num(in, a[0], "floor", 0, line, &x)) return v_nothing();
    return v_int((long long)floor(x));
}

static Value m_ceil(Interp *in, Value *a, int argc, int line) {
    (void)argc; double x;
    if (!want_num(in, a[0], "ceil", 0, line, &x)) return v_nothing();
    return v_int((long long)ceil(x));
}

static Value m_round(Interp *in, Value *a, int argc, int line) {
    double x;
    if (!want_num(in, a[0], "round", 0, line, &x)) return v_nothing();
    long long places = 0;
    if (argc > 1 && !want_int(in, a[1], "round", 1, line, &places)) return v_nothing();
    if (places <= 0) return v_int((long long)llround(x));
    double f = pow(10.0, (double)places);
    return v_float(round(x * f) / f);
}

static Value m_pow(Interp *in, Value *a, int argc, int line) {
    (void)argc; double x, y;
    if (!want_num(in, a[0], "pow", 0, line, &x)) return v_nothing();
    if (!want_num(in, a[1], "pow", 1, line, &y)) return v_nothing();
    return num_result(pow(x, y));
}

static Value m_min(Interp *in, Value *a, int argc, int line) {
    double best = 0, x;
    for (int i = 0; i < argc; i++) {
        if (!want_num(in, a[i], "min", i, line, &x)) return v_nothing();
        if (i == 0 || x < best) best = x;
    }
    return num_result(best);
}

static Value m_max(Interp *in, Value *a, int argc, int line) {
    double best = 0, x;
    for (int i = 0; i < argc; i++) {
        if (!want_num(in, a[i], "max", i, line, &x)) return v_nothing();
        if (i == 0 || x > best) best = x;
    }
    return num_result(best);
}

static Value m_sin(Interp *in, Value *a, int argc, int line) {
    (void)argc; double x;
    if (!want_num(in, a[0], "sin", 0, line, &x)) return v_nothing();
    return v_float(sin(x));
}
static Value m_cos(Interp *in, Value *a, int argc, int line) {
    (void)argc; double x;
    if (!want_num(in, a[0], "cos", 0, line, &x)) return v_nothing();
    return v_float(cos(x));
}
static Value m_tan(Interp *in, Value *a, int argc, int line) {
    (void)argc; double x;
    if (!want_num(in, a[0], "tan", 0, line, &x)) return v_nothing();
    return v_float(tan(x));
}
static Value m_log(Interp *in, Value *a, int argc, int line) {
    double x;
    if (!want_num(in, a[0], "log", 0, line, &x)) return v_nothing();
    if (x <= 0) {
        raise(in, line, "`log` needs a value greater than zero.");
        return v_nothing();
    }
    if (argc > 1) {
        double base;
        if (!want_num(in, a[1], "log", 1, line, &base)) return v_nothing();
        if (base <= 0 || base == 1) {
            raise(in, line, "`log` needs a base greater than zero and not 1.");
            return v_nothing();
        }
        return num_result(log(x) / log(base));
    }
    return v_float(log(x));
}

/* ------------------------------------------------------------------ */
/* text                                                                */
/* ------------------------------------------------------------------ */

static Value t_upper(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "upper", 0, line)) return v_nothing();
    size_t n = a[0].as.obj->len;
    char *out = xmalloc(n + 1);
    for (size_t i = 0; i < n; i++) out[i] = (char)toupper((unsigned char)a[0].as.obj->text[i]);
    out[n] = 0;
    return v_text_take(out, n);
}

static Value t_lower(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "lower", 0, line)) return v_nothing();
    size_t n = a[0].as.obj->len;
    char *out = xmalloc(n + 1);
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)a[0].as.obj->text[i]);
    out[n] = 0;
    return v_text_take(out, n);
}

static Value t_trim(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "trim", 0, line)) return v_nothing();
    const char *s = a[0].as.obj->text;
    size_t n = a[0].as.obj->len, start = 0;
    while (start < n && isspace((unsigned char)s[start])) start++;
    while (n > start && isspace((unsigned char)s[n - 1])) n--;
    return v_text_len(s + start, n - start);
}

static Value t_split(Interp *in, Value *a, int argc, int line) {
    if (!want_text(in, a[0], "split", 0, line)) return v_nothing();
    const char *s = a[0].as.obj->text;
    size_t n = a[0].as.obj->len;
    Value out = v_list();
    if (argc < 2 || (a[1].type == V_TEXT && a[1].as.obj->len == 0)) {
        /* Split on runs of whitespace. */
        size_t i = 0;
        while (i < n) {
            while (i < n && isspace((unsigned char)s[i])) i++;
            size_t start = i;
            while (i < n && !isspace((unsigned char)s[i])) i++;
            if (i > start) list_push(out, v_text_len(s + start, i - start));
        }
        return out;
    }
    if (!want_text(in, a[1], "split", 1, line)) { v_release(out); return v_nothing(); }
    const char *sep = a[1].as.obj->text;
    size_t sl = a[1].as.obj->len;
    size_t start = 0;
    for (size_t i = 0; i + sl <= n; ) {
        if (memcmp(s + i, sep, sl) == 0) {
            list_push(out, v_text_len(s + start, i - start));
            i += sl;
            start = i;
        } else i++;
    }
    list_push(out, v_text_len(s + start, n - start));
    return out;
}

static Value join_values(Value list, const char *sep, size_t sl) {
    size_t cap = 32, len = 0;
    char *out = xmalloc(cap);
    out[0] = 0;
    for (int i = 0; i < list.as.obj->count; i++) {
        char *piece = v_to_text(list.as.obj->items[i]);
        size_t pl = strlen(piece);
        size_t need = len + pl + (i ? sl : 0) + 1;
        while (need > cap) { cap *= 2; out = xrealloc(out, cap); }
        if (i && sl) { memcpy(out + len, sep, sl); len += sl; }
        memcpy(out + len, piece, pl);
        len += pl;
        out[len] = 0;
        free(piece);
    }
    return v_text_take(out, len);
}

static Value t_join(Interp *in, Value *a, int argc, int line) {
    if (!want_list(in, a[0], "join", 0, line)) return v_nothing();
    const char *sep = "";
    size_t sl = 0;
    if (argc > 1) {
        if (!want_text(in, a[1], "join", 1, line)) return v_nothing();
        sep = a[1].as.obj->text;
        sl = a[1].as.obj->len;
    }
    return join_values(a[0], sep, sl);
}

static Value t_replace(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    for (int i = 0; i < 3; i++) if (!want_text(in, a[i], "replace", i, line)) return v_nothing();
    const char *s = a[0].as.obj->text;
    size_t n = a[0].as.obj->len;
    const char *from = a[1].as.obj->text;
    size_t fl = a[1].as.obj->len;
    const char *to = a[2].as.obj->text;
    size_t tl = a[2].as.obj->len;
    if (fl == 0) return v_retain(a[0]);
    size_t cap = n + 16, len = 0;
    char *out = xmalloc(cap);
    for (size_t i = 0; i < n; ) {
        if (i + fl <= n && memcmp(s + i, from, fl) == 0) {
            while (len + tl + 1 > cap) { cap *= 2; out = xrealloc(out, cap); }
            memcpy(out + len, to, tl);
            len += tl;
            i += fl;
        } else {
            while (len + 2 > cap) { cap *= 2; out = xrealloc(out, cap); }
            out[len++] = s[i++];
        }
    }
    out[len] = 0;
    return v_text_take(out, len);
}

static long long text_find(Value hay, Value needle) {
    const char *s = hay.as.obj->text;
    size_t n = hay.as.obj->len;
    const char *f = needle.as.obj->text;
    size_t fl = needle.as.obj->len;
    if (fl == 0) return 0;
    if (fl > n) return -1;
    for (size_t i = 0; i + fl <= n; i++)
        if (memcmp(s + i, f, fl) == 0) return (long long)i;
    return -1;
}

static Value t_find(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "find", 0, line)) return v_nothing();
    if (!want_text(in, a[1], "find", 1, line)) return v_nothing();
    return v_int(text_find(a[0], a[1]));
}

static Value t_contains(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "contains", 0, line)) return v_nothing();
    if (!want_text(in, a[1], "contains", 1, line)) return v_nothing();
    return v_bool(text_find(a[0], a[1]) >= 0);
}

static Value t_starts(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "starts", 0, line)) return v_nothing();
    if (!want_text(in, a[1], "starts", 1, line)) return v_nothing();
    size_t pl = a[1].as.obj->len;
    return v_bool(pl <= a[0].as.obj->len && memcmp(a[0].as.obj->text, a[1].as.obj->text, pl) == 0);
}

static Value t_ends(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "ends", 0, line)) return v_nothing();
    if (!want_text(in, a[1], "ends", 1, line)) return v_nothing();
    size_t n = a[0].as.obj->len, pl = a[1].as.obj->len;
    return v_bool(pl <= n && memcmp(a[0].as.obj->text + n - pl, a[1].as.obj->text, pl) == 0);
}

/* Clamps a slice range onto [0, n] so slicing never errors out. */
static void clamp_range(long long *start, long long *end, long long n) {
    if (*start < 0) *start += n;
    if (*end < 0) *end += n;
    if (*start < 0) *start = 0;
    if (*end > n) *end = n;
    if (*end < *start) *end = *start;
}

static Value t_slice(Interp *in, Value *a, int argc, int line) {
    if (!want_text(in, a[0], "slice", 0, line)) return v_nothing();
    long long n = (long long)a[0].as.obj->len, start = 0, end = n;
    if (!want_int(in, a[1], "slice", 1, line, &start)) return v_nothing();
    if (argc > 2 && !want_int(in, a[2], "slice", 2, line, &end)) return v_nothing();
    clamp_range(&start, &end, n);
    return v_text_len(a[0].as.obj->text + start, (size_t)(end - start));
}

static Value t_repeat(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "repeat", 0, line)) return v_nothing();
    long long times;
    if (!want_int(in, a[1], "repeat", 1, line, &times)) return v_nothing();
    if (times < 0) times = 0;
    size_t n = a[0].as.obj->len;
    if (n && (size_t)times > (size_t)(50u * 1024 * 1024) / n) {
        raise(in, line, "`repeat` would make a piece of text that is far too large.");
        return v_nothing();
    }
    size_t total = n * (size_t)times;
    char *out = xmalloc(total + 1);
    for (long long i = 0; i < times; i++) memcpy(out + (size_t)i * n, a[0].as.obj->text, n);
    out[total] = 0;
    return v_text_take(out, total);
}

static Value t_reverse(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "reverse", 0, line)) return v_nothing();
    size_t n = a[0].as.obj->len;
    char *out = xmalloc(n + 1);
    for (size_t i = 0; i < n; i++) out[i] = a[0].as.obj->text[n - 1 - i];
    out[n] = 0;
    return v_text_take(out, n);
}

static Value t_chars(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "chars", 0, line)) return v_nothing();
    Value out = v_list();
    for (size_t i = 0; i < a[0].as.obj->len; i++)
        list_push(out, v_text_len(a[0].as.obj->text + i, 1));
    return out;
}

static Value t_code(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "code", 0, line)) return v_nothing();
    if (a[0].as.obj->len == 0) {
        raise(in, line, "`code` needs at least one character.");
        return v_nothing();
    }
    return v_int((unsigned char)a[0].as.obj->text[0]);
}

static Value t_char(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    long long c;
    if (!want_int(in, a[0], "char", 0, line, &c)) return v_nothing();
    if (c < 0 || c > 255) {
        raise(in, line, "`char` needs a number between 0 and 255.");
        return v_nothing();
    }
    char buf[2] = {(char)c, 0};
    return v_text_len(buf, 1);
}

static Value t_length(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "length", 0, line)) return v_nothing();
    return v_int((long long)a[0].as.obj->len);
}

/* ------------------------------------------------------------------ */
/* lists                                                               */
/* ------------------------------------------------------------------ */

static Value l_length(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "length", 0, line)) return v_nothing();
    return v_int(a[0].as.obj->count);
}

static Value l_add(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "add", 0, line)) return v_nothing();
    list_push(a[0], v_retain(a[1]));
    return v_retain(a[0]);
}

static Value l_insert(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "insert", 0, line)) return v_nothing();
    long long pos;
    if (!want_int(in, a[1], "insert", 1, line, &pos)) return v_nothing();
    Obj *o = a[0].as.obj;
    if (pos < 0) pos += o->count;
    if (pos < 0) pos = 0;
    if (pos > o->count) pos = o->count;
    list_push(a[0], v_nothing());
    for (int i = o->count - 1; i > pos; i--) o->items[i] = o->items[i - 1];
    o->items[pos] = v_retain(a[2]);
    return v_retain(a[0]);
}

static Value l_remove(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "remove", 0, line)) return v_nothing();
    long long pos;
    if (!want_int(in, a[1], "remove", 1, line, &pos)) return v_nothing();
    Obj *o = a[0].as.obj;
    if (pos < 0) pos += o->count;
    if (pos < 0 || pos >= o->count) {
        raise(in, line, "position %lld is outside this list of length %d.",
              a[1].as.i, o->count);
        return v_nothing();
    }
    Value gone = o->items[pos];
    for (int i = (int)pos; i < o->count - 1; i++) o->items[i] = o->items[i + 1];
    o->count--;
    return gone;
}

static Value l_contains(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "contains", 0, line)) return v_nothing();
    for (int i = 0; i < a[0].as.obj->count; i++)
        if (v_equal(a[0].as.obj->items[i], a[1])) return v_bool(true);
    return v_bool(false);
}

static Value l_find(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "find", 0, line)) return v_nothing();
    for (int i = 0; i < a[0].as.obj->count; i++)
        if (v_equal(a[0].as.obj->items[i], a[1])) return v_int(i);
    return v_int(-1);
}

static Value l_copy(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "copy", 0, line)) return v_nothing();
    Value out = v_list();
    for (int i = 0; i < a[0].as.obj->count; i++) list_push(out, v_retain(a[0].as.obj->items[i]));
    return out;
}

static Value l_reverse(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "reverse", 0, line)) return v_nothing();
    Value out = v_list();
    for (int i = a[0].as.obj->count - 1; i >= 0; i--)
        list_push(out, v_retain(a[0].as.obj->items[i]));
    return out;
}

static Value l_slice(Interp *in, Value *a, int argc, int line) {
    if (!want_list(in, a[0], "slice", 0, line)) return v_nothing();
    long long n = a[0].as.obj->count, start = 0, end = n;
    if (!want_int(in, a[1], "slice", 1, line, &start)) return v_nothing();
    if (argc > 2 && !want_int(in, a[2], "slice", 2, line, &end)) return v_nothing();
    clamp_range(&start, &end, n);
    Value out = v_list();
    for (long long i = start; i < end; i++) list_push(out, v_retain(a[0].as.obj->items[i]));
    return out;
}

static int compare_values(const void *pa, const void *pb) {
    const Value *x = pa, *y = pb;
    bool nx = x->type == V_INT || x->type == V_FLOAT;
    bool ny = y->type == V_INT || y->type == V_FLOAT;
    if (nx && ny) {
        double a = x->type == V_INT ? (double)x->as.i : x->as.f;
        double b = y->type == V_INT ? (double)y->as.i : y->as.f;
        return a < b ? -1 : (a > b ? 1 : 0);
    }
    if (x->type == V_TEXT && y->type == V_TEXT)
        return strcmp(x->as.obj->text, y->as.obj->text);
    return nx ? -1 : (ny ? 1 : 0);
}

static Value l_sort(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "sort", 0, line)) return v_nothing();
    Obj *o = a[0].as.obj;
    bool alltext = true, allnum = true;
    for (int i = 0; i < o->count; i++) {
        if (o->items[i].type != V_TEXT) alltext = false;
        if (o->items[i].type != V_INT && o->items[i].type != V_FLOAT) allnum = false;
    }
    if (!alltext && !allnum && o->count > 1) {
        raise_hint(in, line, "Sort a list of numbers, or a list of text.",
                   "`sort` needs every item to be the same kind of value.");
        return v_nothing();
    }
    Value out = v_list();
    for (int i = 0; i < o->count; i++) list_push(out, v_retain(o->items[i]));
    if (out.as.obj->count > 1)
        qsort(out.as.obj->items, (size_t)out.as.obj->count, sizeof(Value), compare_values);
    return out;
}

static Value l_sum(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "sum", 0, line)) return v_nothing();
    Obj *o = a[0].as.obj;
    bool anyfloat = false;
    double total = 0;
    long long itotal = 0;
    for (int i = 0; i < o->count; i++) {
        if (o->items[i].type == V_INT) { itotal += o->items[i].as.i; total += (double)o->items[i].as.i; }
        else if (o->items[i].type == V_FLOAT) { anyfloat = true; total += o->items[i].as.f; }
        else {
            raise(in, line, "`sum` needs a list of numbers, but found %s.",
                  v_type_name(o->items[i]));
            return v_nothing();
        }
    }
    return anyfloat ? v_float(total) : v_int(itotal);
}

static Value l_minmax(Interp *in, Value *a, int line, bool wantmin, const char *fname) {
    if (!want_list(in, a[0], fname, 0, line)) return v_nothing();
    Obj *o = a[0].as.obj;
    if (o->count == 0) {
        raise(in, line, "`%s` needs a list with at least one item.", fname);
        return v_nothing();
    }
    int best = 0;
    for (int i = 1; i < o->count; i++) {
        int c = compare_values(&o->items[i], &o->items[best]);
        if (wantmin ? c < 0 : c > 0) best = i;
    }
    return v_retain(o->items[best]);
}

static Value l_min(Interp *in, Value *a, int argc, int line) { (void)argc; return l_minmax(in, a, line, true, "min"); }
static Value l_max(Interp *in, Value *a, int argc, int line) { (void)argc; return l_minmax(in, a, line, false, "max"); }

/* ------------------------------------------------------------------ */
/* random                                                              */
/* ------------------------------------------------------------------ */

static unsigned long long rng_state = 0x2545F4914F6CDD1DULL;
static bool rng_ready = false;

static void rng_ensure(void) {
    if (rng_ready) return;
    rng_state ^= (unsigned long long)time(NULL) * 2654435761u + (unsigned long long)getpid();
    if (rng_state == 0) rng_state = 88172645463325252ULL;
    rng_ready = true;
}

static unsigned long long rng_next(void) {
    rng_ensure();
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static Value r_seed(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    long long s;
    if (!want_int(in, a[0], "seed", 0, line, &s)) return v_nothing();
    rng_state = (unsigned long long)s ^ 0x9E3779B97F4A7C15ULL;
    if (rng_state == 0) rng_state = 1;
    rng_ready = true;
    return v_nothing();
}

static Value r_number(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    long long lo, hi;
    if (!want_int(in, a[0], "number", 0, line, &lo)) return v_nothing();
    if (!want_int(in, a[1], "number", 1, line, &hi)) return v_nothing();
    if (lo > hi) { long long t = lo; lo = hi; hi = t; }
    unsigned long long span = (unsigned long long)(hi - lo) + 1;
    return v_int(lo + (long long)(rng_next() % span));
}

static Value r_decimal(Interp *in, Value *a, int argc, int line) {
    (void)in; (void)a; (void)argc; (void)line;
    return v_float((double)(rng_next() >> 11) / 9007199254740992.0);
}

static Value r_pick(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "pick", 0, line)) return v_nothing();
    if (a[0].as.obj->count == 0) {
        raise(in, line, "`pick` needs a list with at least one item.");
        return v_nothing();
    }
    return v_retain(a[0].as.obj->items[rng_next() % (unsigned long long)a[0].as.obj->count]);
}

static Value r_shuffle(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_list(in, a[0], "shuffle", 0, line)) return v_nothing();
    Value out = v_list();
    for (int i = 0; i < a[0].as.obj->count; i++) list_push(out, v_retain(a[0].as.obj->items[i]));
    Obj *o = out.as.obj;
    for (int i = o->count - 1; i > 0; i--) {
        int j = (int)(rng_next() % (unsigned long long)(i + 1));
        Value t = o->items[i]; o->items[i] = o->items[j]; o->items[j] = t;
    }
    return out;
}

/* ------------------------------------------------------------------ */
/* files                                                               */
/* ------------------------------------------------------------------ */

char *chopcap_read_file(const char *path);

static Value f_read(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "read", 0, line)) return v_nothing();
    char *src = chopcap_read_file(a[0].as.obj->text);
    if (!src) {
        raise_hint(in, line, "Check the name and that the file exists.",
                   "cannot read the file `%s`.", a[0].as.obj->text);
        return v_nothing();
    }
    return v_text_take(src, strlen(src));
}

static Value write_file(Interp *in, Value *a, int line, const char *mode, const char *fname) {
    if (!want_text(in, a[0], fname, 0, line)) return v_nothing();
    if (!want_text(in, a[1], fname, 1, line)) return v_nothing();
    FILE *f = fopen(a[0].as.obj->text, mode);
    if (!f) {
        raise_hint(in, line, "Check the folder exists and that you may write there.",
                   "cannot write to the file `%s`.", a[0].as.obj->text);
        return v_nothing();
    }
    fwrite(a[1].as.obj->text, 1, a[1].as.obj->len, f);
    fclose(f);
    return v_nothing();
}

static Value f_write(Interp *in, Value *a, int argc, int line)  { (void)argc; return write_file(in, a, line, "wb", "write"); }
static Value f_append(Interp *in, Value *a, int argc, int line) { (void)argc; return write_file(in, a, line, "ab", "append"); }

static Value f_exists(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "exists", 0, line)) return v_nothing();
    FILE *f = fopen(a[0].as.obj->text, "rb");
    if (f) { fclose(f); return v_bool(true); }
    return v_bool(false);
}

static Value f_lines(Interp *in, Value *a, int argc, int line) {
    Value whole = f_read(in, a, argc, line);
    if (in->flow == FLOW_ERROR) return v_nothing();
    Value out = v_list();
    const char *s = whole.as.obj->text;
    size_t n = whole.as.obj->len, start = 0;
    for (size_t i = 0; i <= n; i++) {
        if (i == n || s[i] == '\n') {
            size_t end = i;
            if (end > start && s[end - 1] == '\r') end--;
            if (i == n && start == n) break;  /* ignore trailing newline */
            list_push(out, v_text_len(s + start, end - start));
            start = i + 1;
        }
    }
    v_release(whole);
    return out;
}

static Value f_remove(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    if (!want_text(in, a[0], "remove", 0, line)) return v_nothing();
    if (remove(a[0].as.obj->text) != 0) {
        raise(in, line, "cannot delete the file `%s`.", a[0].as.obj->text);
        return v_nothing();
    }
    return v_nothing();
}

/* ------------------------------------------------------------------ */
/* time                                                                */
/* ------------------------------------------------------------------ */

static Value ti_now(Interp *in, Value *a, int argc, int line) {
    (void)in; (void)a; (void)argc; (void)line;
    return v_float((double)time(NULL));
}

static Value ti_clock(Interp *in, Value *a, int argc, int line) {
    (void)in; (void)a; (void)argc; (void)line;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return v_float((double)ts.tv_sec + (double)ts.tv_nsec / 1e9);
}

static Value ti_sleep(Interp *in, Value *a, int argc, int line) {
    (void)argc;
    double s;
    if (!want_num(in, a[0], "sleep", 0, line, &s)) return v_nothing();
    if (s > 0) {
        struct timespec req;
        req.tv_sec = (time_t)s;
        req.tv_nsec = (long)((s - (double)req.tv_sec) * 1e9);
        nanosleep(&req, NULL);
    }
    return v_nothing();
}

static Value time_text(const char *fmt) {
    time_t t = time(NULL);
    struct tm lt;
    localtime_r(&t, &lt);
    char buf[64];
    strftime(buf, sizeof buf, fmt, &lt);
    return v_text(buf);
}

static Value ti_date(Interp *in, Value *a, int argc, int line) {
    (void)in; (void)a; (void)argc; (void)line;
    return time_text("%Y-%m-%d");
}

static Value ti_stamp(Interp *in, Value *a, int argc, int line) {
    (void)in; (void)a; (void)argc; (void)line;
    return time_text("%Y-%m-%d %H:%M:%S");
}

static Value time_part(int which) {
    time_t t = time(NULL);
    struct tm lt;
    localtime_r(&t, &lt);
    switch (which) {
    case 0: return v_int(lt.tm_year + 1900);
    case 1: return v_int(lt.tm_mon + 1);
    case 2: return v_int(lt.tm_mday);
    case 3: return v_int(lt.tm_hour);
    case 4: return v_int(lt.tm_min);
    default: return v_int(lt.tm_sec);
    }
}

#define TIME_PART(NAME, IDX)                                              \
    static Value NAME(Interp *in, Value *a, int argc, int line) {         \
        (void)in; (void)a; (void)argc; (void)line;                        \
        return time_part(IDX);                                            \
    }
TIME_PART(ti_year, 0)
TIME_PART(ti_month, 1)
TIME_PART(ti_day, 2)
TIME_PART(ti_hour, 3)
TIME_PART(ti_minute, 4)
TIME_PART(ti_second, 5)

/* ------------------------------------------------------------------ */
/* Registration                                                        */
/* ------------------------------------------------------------------ */

typedef struct { const char *name; NativeFn fn; int amin, amax; } Reg;

static void register_all(Value mod, const Reg *regs) {
    for (int i = 0; regs[i].name; i++)
        module_set(mod, regs[i].name, v_native(regs[i].name, regs[i].fn, regs[i].amin, regs[i].amax));
}

static const Reg MATH_FNS[] = {
    {"sqrt", m_sqrt, 1, 1}, {"abs", m_abs, 1, 1}, {"floor", m_floor, 1, 1},
    {"ceil", m_ceil, 1, 1}, {"round", m_round, 1, 2}, {"pow", m_pow, 2, 2},
    {"min", m_min, 1, -1}, {"max", m_max, 1, -1}, {"sin", m_sin, 1, 1},
    {"cos", m_cos, 1, 1}, {"tan", m_tan, 1, 1}, {"log", m_log, 1, 2},
    {NULL, NULL, 0, 0}
};

static const Reg TEXT_FNS[] = {
    {"upper", t_upper, 1, 1}, {"lower", t_lower, 1, 1}, {"trim", t_trim, 1, 1},
    {"split", t_split, 1, 2}, {"join", t_join, 1, 2}, {"replace", t_replace, 3, 3},
    {"contains", t_contains, 2, 2}, {"starts", t_starts, 2, 2}, {"ends", t_ends, 2, 2},
    {"find", t_find, 2, 2}, {"slice", t_slice, 2, 3}, {"repeat", t_repeat, 2, 2},
    {"reverse", t_reverse, 1, 1}, {"chars", t_chars, 1, 1}, {"code", t_code, 1, 1},
    {"char", t_char, 1, 1}, {"length", t_length, 1, 1},
    {NULL, NULL, 0, 0}
};

static const Reg LIST_FNS[] = {
    {"length", l_length, 1, 1}, {"add", l_add, 2, 2}, {"insert", l_insert, 3, 3},
    {"remove", l_remove, 2, 2}, {"contains", l_contains, 2, 2}, {"find", l_find, 2, 2},
    {"copy", l_copy, 1, 1}, {"reverse", l_reverse, 1, 1}, {"slice", l_slice, 2, 3},
    {"sort", l_sort, 1, 1}, {"sum", l_sum, 1, 1}, {"min", l_min, 1, 1},
    {"max", l_max, 1, 1}, {"join", t_join, 1, 2},
    {NULL, NULL, 0, 0}
};

static const Reg RANDOM_FNS[] = {
    {"seed", r_seed, 1, 1}, {"number", r_number, 2, 2}, {"decimal", r_decimal, 0, 0},
    {"pick", r_pick, 1, 1}, {"shuffle", r_shuffle, 1, 1},
    {NULL, NULL, 0, 0}
};

static const Reg FILE_FNS[] = {
    {"read", f_read, 1, 1}, {"write", f_write, 2, 2}, {"append", f_append, 2, 2},
    {"exists", f_exists, 1, 1}, {"lines", f_lines, 1, 1}, {"remove", f_remove, 1, 1},
    {NULL, NULL, 0, 0}
};

static const Reg TIME_FNS[] = {
    {"now", ti_now, 0, 0}, {"clock", ti_clock, 0, 0}, {"sleep", ti_sleep, 1, 1},
    {"date", ti_date, 0, 0}, {"stamp", ti_stamp, 0, 0}, {"year", ti_year, 0, 0},
    {"month", ti_month, 0, 0}, {"day", ti_day, 0, 0}, {"hour", ti_hour, 0, 0},
    {"minute", ti_minute, 0, 0}, {"second", ti_second, 0, 0},
    {NULL, NULL, 0, 0}
};

static const Reg GLOBAL_FNS[] = {
    {"len", bi_len, 1, 1}, {"text", bi_text, 1, 1}, {"number", bi_number, 1, 1},
    {"whole", bi_whole, 1, 1}, {"type", bi_type, 1, 1}, {"push", bi_push, 2, 2},
    {"pop", bi_pop, 1, 1},
    {NULL, NULL, 0, 0}
};

void install_builtins(Interp *in) {
    for (int i = 0; GLOBAL_FNS[i].name; i++)
        env_define(in->globals, GLOBAL_FNS[i].name,
                   v_native(GLOBAL_FNS[i].name, GLOBAL_FNS[i].fn,
                            GLOBAL_FNS[i].amin, GLOBAL_FNS[i].amax));
}

static const char *MODULE_NAMES[] = {"math", "text", "lists", "random", "files", "time", NULL};

Value load_module(Interp *in, const char *name, int line) {
    Value mod = v_module(name);
    if (strcmp(name, "math") == 0) {
        register_all(mod, MATH_FNS);
        module_set(mod, "pi", v_float(3.14159265358979323846));
        module_set(mod, "e", v_float(2.71828182845904523536));
    } else if (strcmp(name, "text") == 0) {
        register_all(mod, TEXT_FNS);
    } else if (strcmp(name, "lists") == 0) {
        register_all(mod, LIST_FNS);
    } else if (strcmp(name, "random") == 0) {
        register_all(mod, RANDOM_FNS);
    } else if (strcmp(name, "files") == 0) {
        register_all(mod, FILE_FNS);
    } else if (strcmp(name, "time") == 0) {
        register_all(mod, TIME_FNS);
    } else {
        v_release(mod);
        char known[256];
        known[0] = 0;
        for (int i = 0; MODULE_NAMES[i]; i++) {
            if (i) strncat(known, ", ", sizeof(known) - strlen(known) - 1);
            strncat(known, MODULE_NAMES[i], sizeof(known) - strlen(known) - 1);
        }
        char hint[320];
        snprintf(hint, sizeof hint, "Chopcap comes with: %s.", known);
        raise_hint(in, line, hint, "there is no module called `%s`.", name);
        return v_nothing();
    }
    return mod;
}
