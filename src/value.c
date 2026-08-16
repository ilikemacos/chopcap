/* Chopcap values: tagged unions with reference-counted heap objects. */
#include "chopcap.h"
#include <math.h>

void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) { fprintf(stderr, "chopcap: out of memory\n"); exit(70); }
    return p;
}

void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) { fprintf(stderr, "chopcap: out of memory\n"); exit(70); }
    return q;
}

char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *p = xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

static Obj *obj_new(ValueType kind) {
    Obj *o = xmalloc(sizeof(Obj));
    memset(o, 0, sizeof(Obj));
    o->refs = 1;
    o->kind = kind;
    return o;
}

Value v_nothing(void) { Value v; v.type = V_NOTHING; v.as.i = 0; return v; }
Value v_bool(bool b)  { Value v; v.type = V_BOOL; v.as.b = b; return v; }
Value v_int(long long i) { Value v; v.type = V_INT; v.as.i = i; return v; }
Value v_float(double f)  { Value v; v.type = V_FLOAT; v.as.f = f; return v; }

Value v_text_len(const char *s, size_t n) {
    Obj *o = obj_new(V_TEXT);
    o->text = xmalloc(n + 1);
    if (n) memcpy(o->text, s, n);
    o->text[n] = 0;
    o->len = n;
    Value v; v.type = V_TEXT; v.as.obj = o; return v;
}

Value v_text(const char *s) { return v_text_len(s, strlen(s)); }

Value v_text_take(char *s, size_t n) {
    Obj *o = obj_new(V_TEXT);
    o->text = s; o->len = n;
    Value v; v.type = V_TEXT; v.as.obj = o; return v;
}

Value v_list(void) {
    Obj *o = obj_new(V_LIST);
    o->cap = 4;
    o->items = xmalloc(sizeof(Value) * o->cap);
    Value v; v.type = V_LIST; v.as.obj = o; return v;
}

Value v_module(const char *name) {
    Obj *o = obj_new(V_MODULE);
    o->modname = xstrdup(name);
    o->slotcap = 8;
    o->slots = xmalloc(sizeof(Slot) * o->slotcap);
    Value v; v.type = V_MODULE; v.as.obj = o; return v;
}

Value v_native(const char *name, NativeFn fn, int amin, int amax) {
    Obj *o = obj_new(V_NATIVE);
    o->fn = fn; o->nname = name;
    o->arity_min = amin; o->arity_max = amax;
    Value v; v.type = V_NATIVE; v.as.obj = o; return v;
}

static bool is_obj(Value v) {
    return v.type == V_TEXT || v.type == V_LIST || v.type == V_FUNC ||
           v.type == V_NATIVE || v.type == V_MODULE;
}

Value v_retain(Value v) {
    if (is_obj(v) && v.as.obj) v.as.obj->refs++;
    return v;
}

void v_release(Value v) {
    if (!is_obj(v) || !v.as.obj) return;
    Obj *o = v.as.obj;
    if (--o->refs > 0) return;
    switch (o->kind) {
    case V_TEXT:
        free(o->text);
        break;
    case V_LIST:
        for (int i = 0; i < o->count; i++) v_release(o->items[i]);
        free(o->items);
        break;
    case V_FUNC:
        if (o->closure) env_release(o->closure);
        break;
    case V_MODULE:
        for (int i = 0; i < o->nslots; i++) {
            free(o->slots[i].name);
            v_release(o->slots[i].value);
        }
        free(o->slots);
        free(o->modname);
        break;
    default: break;
    }
    free(o);
}

void list_push(Value list, Value item) {
    Obj *o = list.as.obj;
    if (o->count == o->cap) {
        o->cap *= 2;
        o->items = xrealloc(o->items, sizeof(Value) * o->cap);
    }
    o->items[o->count++] = item;
}

void module_set(Value mod, const char *name, Value v) {
    Obj *o = mod.as.obj;
    for (int i = 0; i < o->nslots; i++) {
        if (strcmp(o->slots[i].name, name) == 0) {
            v_release(o->slots[i].value);
            o->slots[i].value = v;
            return;
        }
    }
    if (o->nslots == o->slotcap) {
        o->slotcap *= 2;
        o->slots = xrealloc(o->slots, sizeof(Slot) * o->slotcap);
    }
    o->slots[o->nslots].name = xstrdup(name);
    o->slots[o->nslots].value = v;
    o->nslots++;
}

Value *module_get(Value mod, const char *name) {
    Obj *o = mod.as.obj;
    for (int i = 0; i < o->nslots; i++)
        if (strcmp(o->slots[i].name, name) == 0) return &o->slots[i].value;
    return NULL;
}

bool v_truthy(Value v) {
    switch (v.type) {
    case V_NOTHING: return false;
    case V_BOOL:    return v.as.b;
    case V_INT:     return v.as.i != 0;
    case V_FLOAT:   return v.as.f != 0.0;
    case V_TEXT:    return v.as.obj->len > 0;
    case V_LIST:    return v.as.obj->count > 0;
    default:        return true;
    }
}

bool v_equal(Value a, Value b) {
    if ((a.type == V_INT || a.type == V_FLOAT) &&
        (b.type == V_INT || b.type == V_FLOAT)) {
        double x = a.type == V_INT ? (double)a.as.i : a.as.f;
        double y = b.type == V_INT ? (double)b.as.i : b.as.f;
        return x == y;
    }
    if (a.type != b.type) return false;
    switch (a.type) {
    case V_NOTHING: return true;
    case V_BOOL:    return a.as.b == b.as.b;
    case V_TEXT:
        return a.as.obj->len == b.as.obj->len &&
               memcmp(a.as.obj->text, b.as.obj->text, a.as.obj->len) == 0;
    case V_LIST: {
        if (a.as.obj == b.as.obj) return true;
        if (a.as.obj->count != b.as.obj->count) return false;
        for (int i = 0; i < a.as.obj->count; i++)
            if (!v_equal(a.as.obj->items[i], b.as.obj->items[i])) return false;
        return true;
    }
    default: return a.as.obj == b.as.obj;
    }
}

const char *v_type_name(Value v) {
    switch (v.type) {
    case V_NOTHING: return "nothing";
    case V_BOOL:    return "boolean";
    case V_INT:     return "number";
    case V_FLOAT:   return "decimal";
    case V_TEXT:    return "text";
    case V_LIST:    return "list";
    case V_FUNC:
    case V_NATIVE:  return "function";
    case V_MODULE:  return "module";
    }
    return "unknown";
}

/* A tiny growable char buffer used to build display strings. */
typedef struct { char *p; size_t len, cap; } Buf;

static void buf_init(Buf *b) { b->cap = 32; b->len = 0; b->p = xmalloc(b->cap); b->p[0] = 0; }

static void buf_add(Buf *b, const char *s, size_t n) {
    while (b->len + n + 1 > b->cap) { b->cap *= 2; b->p = xrealloc(b->p, b->cap); }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = 0;
}

static void buf_str(Buf *b, const char *s) { buf_add(b, s, strlen(s)); }

/* Format a decimal so that beginners see friendly output: 0.5, 3.25, 1e+30. */
static void fmt_float(char *out, size_t n, double d) {
    if (isnan(d)) { snprintf(out, n, "not-a-number"); return; }
    if (isinf(d)) { snprintf(out, n, d > 0 ? "infinity" : "-infinity"); return; }
    snprintf(out, n, "%.10g", d);
}

static void write_value(Buf *b, Value v, bool quoted) {
    char tmp[64];
    switch (v.type) {
    case V_NOTHING: buf_str(b, "nothing"); break;
    case V_BOOL:    buf_str(b, v.as.b ? "true" : "false"); break;
    case V_INT:     snprintf(tmp, sizeof tmp, "%lld", v.as.i); buf_str(b, tmp); break;
    case V_FLOAT:   fmt_float(tmp, sizeof tmp, v.as.f); buf_str(b, tmp); break;
    case V_TEXT:
        if (quoted) {
            buf_str(b, "\"");
            for (size_t i = 0; i < v.as.obj->len; i++) {
                char c = v.as.obj->text[i];
                if (c == '"')       buf_str(b, "\\\"");
                else if (c == '\\') buf_str(b, "\\\\");
                else if (c == '\n') buf_str(b, "\\n");
                else if (c == '\t') buf_str(b, "\\t");
                else buf_add(b, &c, 1);
            }
            buf_str(b, "\"");
        } else {
            buf_add(b, v.as.obj->text, v.as.obj->len);
        }
        break;
    case V_LIST:
        buf_str(b, "[");
        for (int i = 0; i < v.as.obj->count; i++) {
            if (i) buf_str(b, ", ");
            write_value(b, v.as.obj->items[i], true);
        }
        buf_str(b, "]");
        break;
    case V_FUNC:
        buf_str(b, "<function ");
        buf_str(b, v.as.obj->decl && v.as.obj->decl->name ? v.as.obj->decl->name : "anonymous");
        buf_str(b, ">");
        break;
    case V_NATIVE:
        buf_str(b, "<function ");
        buf_str(b, v.as.obj->nname);
        buf_str(b, ">");
        break;
    case V_MODULE:
        buf_str(b, "<module ");
        buf_str(b, v.as.obj->modname);
        buf_str(b, ">");
        break;
    }
}

char *v_to_text(Value v) {
    Buf b; buf_init(&b);
    write_value(&b, v, false);
    return b.p;
}

char *v_repr(Value v) {
    Buf b; buf_init(&b);
    write_value(&b, v, true);
    return b.p;
}

/* ------------------------------------------------------------------ */
/* Environments                                                        */
/* ------------------------------------------------------------------ */

Env *env_new(Env *parent) {
    Env *e = xmalloc(sizeof(Env));
    e->refs = 1;
    e->parent = parent ? env_retain(parent) : NULL;
    e->count = 0;
    e->cap = 8;
    e->slots = xmalloc(sizeof(Slot) * e->cap);
    return e;
}

Env *env_retain(Env *e) { if (e) e->refs++; return e; }

void env_release(Env *e) {
    if (!e) return;
    if (--e->refs > 0) return;
    for (int i = 0; i < e->count; i++) {
        free(e->slots[i].name);
        v_release(e->slots[i].value);
    }
    free(e->slots);
    if (e->parent) env_release(e->parent);
    free(e);
}

Value *env_lookup(Env *e, const char *name) {
    for (Env *s = e; s; s = s->parent)
        for (int i = 0; i < s->count; i++)
            if (strcmp(s->slots[i].name, name) == 0) return &s->slots[i].value;
    return NULL;
}

void env_define(Env *e, const char *name, Value v) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->slots[i].name, name) == 0) {
            v_release(e->slots[i].value);
            e->slots[i].value = v;
            return;
        }
    }
    if (e->count == e->cap) {
        e->cap *= 2;
        e->slots = xrealloc(e->slots, sizeof(Slot) * e->cap);
    }
    e->slots[e->count].name = xstrdup(name);
    e->slots[e->count].value = v;
    e->count++;
}

bool env_assign(Env *e, const char *name, Value v) {
    for (Env *s = e; s; s = s->parent) {
        for (int i = 0; i < s->count; i++) {
            if (strcmp(s->slots[i].name, name) == 0) {
                v_release(s->slots[i].value);
                s->slots[i].value = v;
                return true;
            }
        }
    }
    return false;
}
