/* Chopcap interpreter: a straightforward tree-walking evaluator. */
#include "chopcap.h"
#include <math.h>
#include <libgen.h>

#define MAX_DEPTH 900

static void exec_stmt(Interp *in, Node *n, Env *env);

void interp_init(Interp *in) {
    memset(in, 0, sizeof(Interp));
    in->globals = env_new(NULL);
    in->retval = v_nothing();
    in->flow = FLOW_NORMAL;
    install_builtins(in);
}

void interp_free(Interp *in) {
    v_release(in->retval);
    env_release(in->globals);
    free(in->errmsg);
    free(in->errhint);
    free(in->errsnippet);
    free(in->errfile);
    free(in->basedir);
    memset(in, 0, sizeof(Interp));
}

static bool is_num(Value v) { return v.type == V_INT || v.type == V_FLOAT; }
static double numof(Value v) { return v.type == V_INT ? (double)v.as.i : v.as.f; }

static Value make_func(Node *decl, Env *closure) {
    Obj *o = xmalloc(sizeof(Obj));
    memset(o, 0, sizeof(Obj));
    o->refs = 1;
    o->kind = V_FUNC;
    o->decl = decl;
    o->closure = env_retain(closure);
    Value v; v.type = V_FUNC; v.as.obj = o;
    return v;
}

static Value text_concat(Value a, Value b) {
    char *sa = v_to_text(a), *sb = v_to_text(b);
    size_t la = strlen(sa), lb = strlen(sb);
    char *out = xmalloc(la + lb + 1);
    memcpy(out, sa, la);
    memcpy(out + la, sb, lb);
    out[la + lb] = 0;
    free(sa); free(sb);
    return v_text_take(out, la + lb);
}

static const char *opname(TokType op) {
    switch (op) {
    case T_PLUS: return "+"; case T_MINUS: return "-";
    case T_STAR: return "*"; case T_SLASH: return "/";
    case T_PERCENT: return "%"; case T_CARET: return "^";
    case T_LT: return "<"; case T_LE: return "<=";
    case T_GT: return ">"; case T_GE: return ">=";
    default: return "?";
    }
}

static bool contains_value(Value hay, Value needle) {
    for (int i = 0; i < hay.as.obj->count; i++)
        if (v_equal(hay.as.obj->items[i], needle)) return true;
    return false;
}

static Value binop(Interp *in, TokType op, Value a, Value b, int line) {
    switch (op) {
    case T_EQ: return v_bool(v_equal(a, b));
    case T_NE: return v_bool(!v_equal(a, b));
    case T_IN:
        if (b.type == V_LIST) return v_bool(contains_value(b, a));
        if (b.type == V_TEXT && a.type == V_TEXT)
            return v_bool(a.as.obj->len == 0 || strstr(b.as.obj->text, a.as.obj->text) != NULL);
        raise_hint(in, line, "`in` works with a list, or with text inside text.",
                   "cannot look for %s inside %s.", v_type_name(a), v_type_name(b));
        return v_nothing();
    default: break;
    }

    if (op == T_PLUS) {
        if (is_num(a) && is_num(b)) {
            if (a.type == V_INT && b.type == V_INT) return v_int(a.as.i + b.as.i);
            return v_float(numof(a) + numof(b));
        }
        if (a.type == V_TEXT || b.type == V_TEXT) return text_concat(a, b);
        if (a.type == V_LIST && b.type == V_LIST) {
            Value out = v_list();
            for (int i = 0; i < a.as.obj->count; i++) list_push(out, v_retain(a.as.obj->items[i]));
            for (int i = 0; i < b.as.obj->count; i++) list_push(out, v_retain(b.as.obj->items[i]));
            return out;
        }
        raise_hint(in, line, "You can add numbers, join text, or join two lists.",
                   "cannot add %s and %s.", v_type_name(a), v_type_name(b));
        return v_nothing();
    }

    if (op == T_LT || op == T_LE || op == T_GT || op == T_GE) {
        if (a.type == V_TEXT && b.type == V_TEXT) {
            int c = strcmp(a.as.obj->text, b.as.obj->text);
            switch (op) {
            case T_LT: return v_bool(c < 0);
            case T_LE: return v_bool(c <= 0);
            case T_GT: return v_bool(c > 0);
            default:   return v_bool(c >= 0);
            }
        }
        if (!is_num(a) || !is_num(b)) {
            raise_hint(in, line, "Compare numbers with numbers, or text with text.",
                       "cannot compare %s with %s using `%s`.",
                       v_type_name(a), v_type_name(b), opname(op));
            return v_nothing();
        }
        double x = numof(a), y = numof(b);
        switch (op) {
        case T_LT: return v_bool(x < y);
        case T_LE: return v_bool(x <= y);
        case T_GT: return v_bool(x > y);
        default:   return v_bool(x >= y);
        }
    }

    if (!is_num(a) || !is_num(b)) {
        raise_hint(in, line, "This operator only works with numbers.",
                   "cannot use `%s` with %s and %s.",
                   opname(op), v_type_name(a), v_type_name(b));
        return v_nothing();
    }

    switch (op) {
    case T_MINUS:
        if (a.type == V_INT && b.type == V_INT) return v_int(a.as.i - b.as.i);
        return v_float(numof(a) - numof(b));
    case T_STAR:
        if (a.type == V_INT && b.type == V_INT) return v_int(a.as.i * b.as.i);
        return v_float(numof(a) * numof(b));
    case T_SLASH: {
        double y = numof(b);
        if (y == 0.0) {
            raise_hint(in, line, "Check the value on the right of the `/`.",
                       "cannot divide by zero.");
            return v_nothing();
        }
        if (a.type == V_INT && b.type == V_INT && b.as.i != 0 && a.as.i % b.as.i == 0)
            return v_int(a.as.i / b.as.i);
        return v_float(numof(a) / y);
    }
    case T_PERCENT: {
        double y = numof(b);
        if (y == 0.0) {
            raise_hint(in, line, "Check the value on the right of the `%`.",
                       "cannot take the remainder of a division by zero.");
            return v_nothing();
        }
        if (a.type == V_INT && b.type == V_INT) return v_int(a.as.i % b.as.i);
        return v_float(fmod(numof(a), y));
    }
    case T_CARET: {
        double r = pow(numof(a), numof(b));
        if (a.type == V_INT && b.type == V_INT && b.as.i >= 0 &&
            r == (double)(long long)r && fabs(r) < 9.0e18)
            return v_int((long long)r);
        return v_float(r);
    }
    default:
        raise(in, line, "unsupported operator.");
        return v_nothing();
    }
}

/* Normalises an index, allowing -1 to mean "the last item". */
static bool resolve_index(Interp *in, Value idx, int count, const char *what,
                          int line, int *out) {
    if (idx.type != V_INT) {
        raise_hint(in, line, "Positions must be whole numbers, starting at 0.",
                   "cannot use %s as a position in %s.", v_type_name(idx), what);
        return false;
    }
    long long i = idx.as.i;
    if (i < 0) i += count;
    if (i < 0 || i >= count) {
        if (count == 0)
            raise_hint(in, line, "It is empty, so there is nothing at any position.",
                       "position %lld is outside this %s.", idx.as.i, what);
        else
            raise_hint(in, line, "Valid positions run from 0 to the length minus one.",
                       "position %lld is outside this %s of length %d.",
                       idx.as.i, what, count);
        return false;
    }
    *out = (int)i;
    return true;
}

static Value read_line_value(void) {
    size_t cap = 128, len = 0;
    char *buf = xmalloc(cap);
    int c;
    while ((c = fgetc(stdin)) != EOF && c != '\n') {
        if (len + 1 >= cap) { cap *= 2; buf = xrealloc(buf, cap); }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return v_nothing(); }
    if (len && buf[len - 1] == '\r') len--;
    buf[len] = 0;
    return v_text_take(buf, len);
}

Value call_value(Interp *in, Value fn, Value *args, int argc, int line) {
    if (fn.type == V_NATIVE) {
        Obj *o = fn.as.obj;
        if (argc < o->arity_min || (o->arity_max >= 0 && argc > o->arity_max)) {
            if (o->arity_min == o->arity_max)
                raise(in, line, "`%s` needs %d value%s but got %d.",
                      o->nname, o->arity_min, o->arity_min == 1 ? "" : "s", argc);
            else
                raise(in, line, "`%s` needs between %d and %d values but got %d.",
                      o->nname, o->arity_min, o->arity_max, argc);
            return v_nothing();
        }
        return o->fn(in, args, argc, line);
    }
    if (fn.type != V_FUNC) {
        raise_hint(in, line, "Only functions can be called with `(...)`.",
                   "%s is not a function.", v_type_name(fn));
        return v_nothing();
    }
    Node *decl = fn.as.obj->decl;
    if (argc != decl->nparams) {
        raise(in, line, "`%s` needs %d value%s but got %d.",
              decl->name, decl->nparams, decl->nparams == 1 ? "" : "s", argc);
        return v_nothing();
    }
    if (++in->depth > MAX_DEPTH) {
        in->depth--;
        raise_hint(in, line, "A function is probably calling itself without ever stopping.",
                   "`%s` went too deep.", decl->name);
        return v_nothing();
    }
    Env *local = env_new(fn.as.obj->closure);
    for (int i = 0; i < argc; i++) env_define(local, decl->params[i], v_retain(args[i]));
    exec_block(in, decl->a, local);
    Value result = v_nothing();
    if (in->flow == FLOW_RETURN) {
        result = in->retval;
        in->retval = v_nothing();
        in->flow = FLOW_NORMAL;
    } else if (in->flow == FLOW_BREAK || in->flow == FLOW_SKIP) {
        in->flow = FLOW_NORMAL;
    }
    env_release(local);
    in->depth--;
    return result;
}

Value eval(Interp *in, Node *n, Env *env) {
    if (in->flow == FLOW_ERROR) return v_nothing();
    switch (n->type) {
    case N_LITERAL:
        return v_retain(n->literal);

    case N_NAME: {
        Value *slot = env_lookup(env, n->name);
        if (!slot) {
            char *near = nearest_name(env, n->name);
            if (near) {
                char hint[160];
                snprintf(hint, sizeof hint, "Did you mean `%s`?", near);
                raise_hint(in, n->line, hint, "`%s` does not exist.", n->name);
                free(near);
            } else {
                char hint[160];
                snprintf(hint, sizeof hint, "Give it a value first, like `%s = 1`.", n->name);
                raise_hint(in, n->line, hint, "`%s` does not exist.", n->name);
            }
            return v_nothing();
        }
        return v_retain(*slot);
    }

    case N_LISTLIT: {
        Value list = v_list();
        for (int i = 0; i < n->nkids; i++) {
            Value item = eval(in, n->kids[i], env);
            if (in->flow == FLOW_ERROR) { v_release(item); v_release(list); return v_nothing(); }
            list_push(list, item);
        }
        return list;
    }

    case N_ASK: {
        if (n->a) {
            Value prompt = eval(in, n->a, env);
            if (in->flow == FLOW_ERROR) return v_nothing();
            char *s = v_to_text(prompt);
            fputs(s, stdout);
            free(s);
            v_release(prompt);
        }
        fflush(stdout);
        return read_line_value();
    }

    case N_AND: {
        Value a = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) return v_nothing();
        if (!v_truthy(a)) return a;
        v_release(a);
        return eval(in, n->b, env);
    }

    case N_OR: {
        Value a = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) return v_nothing();
        if (v_truthy(a)) return a;
        v_release(a);
        return eval(in, n->b, env);
    }

    case N_UNARY: {
        Value a = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) return v_nothing();
        Value r = v_nothing();
        if (n->op == T_NOT) {
            r = v_bool(!v_truthy(a));
        } else {
            if (a.type == V_INT) r = v_int(-a.as.i);
            else if (a.type == V_FLOAT) r = v_float(-a.as.f);
            else raise(in, n->line, "cannot make %s negative.", v_type_name(a));
        }
        v_release(a);
        return r;
    }

    case N_BINARY: {
        Value a = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) return v_nothing();
        Value b = eval(in, n->b, env);
        if (in->flow == FLOW_ERROR) { v_release(a); return v_nothing(); }
        Value r = binop(in, n->op, a, b, n->line);
        v_release(a); v_release(b);
        return r;
    }

    case N_INDEX: {
        Value obj = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) return v_nothing();
        Value idx = eval(in, n->b, env);
        if (in->flow == FLOW_ERROR) { v_release(obj); return v_nothing(); }
        Value r = v_nothing();
        int i;
        if (obj.type == V_LIST) {
            if (resolve_index(in, idx, obj.as.obj->count, "list", n->line, &i))
                r = v_retain(obj.as.obj->items[i]);
        } else if (obj.type == V_TEXT) {
            if (resolve_index(in, idx, (int)obj.as.obj->len, "text", n->line, &i))
                r = v_text_len(obj.as.obj->text + i, 1);
        } else {
            raise_hint(in, n->line, "Only lists and text can be used with `[...]`.",
                       "cannot take a position out of %s.", v_type_name(obj));
        }
        v_release(obj); v_release(idx);
        return r;
    }

    case N_DOT: {
        Value obj = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) return v_nothing();
        Value r = v_nothing();
        if (obj.type == V_MODULE) {
            Value *slot = module_get(obj, n->name);
            if (slot) r = v_retain(*slot);
            else raise(in, n->line, "the module `%s` has nothing called `%s`.",
                       obj.as.obj->modname, n->name);
        } else {
            raise_hint(in, n->line, "`.` is used to reach inside a module, like `math.sqrt`.",
                       "%s has no parts to reach with `.`.", v_type_name(obj));
        }
        v_release(obj);
        return r;
    }

    case N_CALL: {
        Value fn = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) return v_nothing();
        Value *args = n->nkids ? xmalloc(sizeof(Value) * n->nkids) : NULL;
        int argc = 0;
        for (int i = 0; i < n->nkids; i++) {
            args[argc] = eval(in, n->kids[i], env);
            if (in->flow == FLOW_ERROR) {
                for (int k = 0; k <= argc; k++) if (k < argc) v_release(args[k]);
                free(args);
                v_release(fn);
                return v_nothing();
            }
            argc++;
        }
        Value r = call_value(in, fn, args, argc, n->line);
        for (int i = 0; i < argc; i++) v_release(args[i]);
        free(args);
        v_release(fn);
        return r;
    }

    default:
        raise(in, n->line, "this expression is not something Chopcap can run.");
        return v_nothing();
    }
}

/* ------------------------------------------------------------------ */
/* `use` — loading modules                                             */
/* ------------------------------------------------------------------ */

static char *read_whole_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = xmalloc((size_t)size + 1);
    size_t got = fread(buf, 1, (size_t)size, f);
    buf[got] = 0;
    fclose(f);
    return buf;
}

char *chopcap_read_file(const char *path) { return read_whole_file(path); }

static void exec_use(Interp *in, Node *n, Env *env) {
    if (n->op == T_NAME) {
        Value mod = load_module(in, n->name, n->line);
        if (in->flow == FLOW_ERROR) return;
        env_define(env, n->name2 ? n->name2 : n->name, mod);
        return;
    }
    /* use "path.chop" as name */
    char path[1024];
    if (n->name[0] == '/')
        snprintf(path, sizeof path, "%s", n->name);
    else
        snprintf(path, sizeof path, "%s/%s", in->basedir ? in->basedir : ".", n->name);

    char *src = read_whole_file(path);
    if (!src) {
        raise_hint(in, n->line, "Check the file name and that it sits next to this program.",
                   "cannot open the file `%s`.", n->name);
        return;
    }
    if (++in->depth > 32) {
        in->depth--;
        free(src);
        raise(in, n->line, "files are loading each other in a circle.");
        return;
    }

    const char *oldsrc = in->src;
    const char *oldfile = in->filename;
    in->src = src;
    in->filename = n->name;

    TokenList toks;
    Node *program = NULL;
    Env *modenv = env_new(in->globals);
    if (lex(in, src, &toks)) {
        program = parse(in, &toks);
        if (program) exec_block(in, program, modenv);
    }

    in->src = oldsrc;
    in->filename = oldfile;
    in->depth--;

    if (in->flow == FLOW_ERROR) {
        node_free(program);
        toklist_free(&toks);
        env_release(modenv);
        free(src);
        return;
    }
    Value mod = v_module(n->name2);
    for (int i = 0; i < modenv->count; i++)
        module_set(mod, modenv->slots[i].name, v_retain(modenv->slots[i].value));
    env_define(env, n->name2, mod);

    /* The AST must outlive the module: its functions point into it. */
    (void)program;
    toklist_free(&toks);
    env_release(modenv);
}

/* ------------------------------------------------------------------ */
/* Statements                                                          */
/* ------------------------------------------------------------------ */

void exec_block(Interp *in, Node *block, Env *env) {
    for (int i = 0; i < block->nkids; i++) {
        exec_stmt(in, block->kids[i], env);
        if (in->flow != FLOW_NORMAL) return;
    }
}

static void exec_stmt(Interp *in, Node *n, Env *env) {
    if (in->flow != FLOW_NORMAL) return;
    switch (n->type) {
    case N_BLOCK:
        exec_block(in, n, env);
        return;

    case N_SAY: {
        for (int i = 0; i < n->nkids; i++) {
            Value v = eval(in, n->kids[i], env);
            if (in->flow == FLOW_ERROR) { v_release(v); return; }
            char *s = v_to_text(v);
            if (i) fputc(' ', stdout);
            fputs(s, stdout);
            free(s);
            v_release(v);
        }
        fputc('\n', stdout);
        return;
    }

    case N_ASSIGN: {
        Value v = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) { v_release(v); return; }
        if (!env_assign(env, n->name, v)) env_define(env, n->name, v);
        return;
    }

    case N_INDEX_ASSIGN: {
        Value obj = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) { v_release(obj); return; }
        Value idx = eval(in, n->b, env);
        if (in->flow == FLOW_ERROR) { v_release(obj); v_release(idx); return; }
        Value val = eval(in, n->c, env);
        if (in->flow == FLOW_ERROR) { v_release(obj); v_release(idx); v_release(val); return; }
        if (obj.type != V_LIST) {
            raise_hint(in, n->line, "Only list positions can be changed this way.",
                       "cannot change a position inside %s.", v_type_name(obj));
        } else {
            int i;
            if (resolve_index(in, idx, obj.as.obj->count, "list", n->line, &i)) {
                v_release(obj.as.obj->items[i]);
                obj.as.obj->items[i] = v_retain(val);
            }
        }
        v_release(obj); v_release(idx); v_release(val);
        return;
    }

    case N_EXPRSTMT: {
        Value v = eval(in, n->a, env);
        v_release(v);
        return;
    }

    case N_IF: {
        Value cond = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) { v_release(cond); return; }
        bool truth = v_truthy(cond);
        v_release(cond);
        if (truth) exec_block(in, n->b, env);
        else if (n->c) exec_stmt(in, n->c, env);
        return;
    }

    case N_WHILE: {
        for (;;) {
            Value cond = eval(in, n->a, env);
            if (in->flow == FLOW_ERROR) { v_release(cond); return; }
            bool truth = v_truthy(cond);
            v_release(cond);
            if (!truth) break;
            exec_block(in, n->b, env);
            if (in->flow == FLOW_BREAK) { in->flow = FLOW_NORMAL; break; }
            if (in->flow == FLOW_SKIP) { in->flow = FLOW_NORMAL; continue; }
            if (in->flow != FLOW_NORMAL) return;
        }
        return;
    }

    case N_FOR: {
        Value seq = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) { v_release(seq); return; }
        int count;
        if (seq.type == V_LIST) count = seq.as.obj->count;
        else if (seq.type == V_TEXT) count = (int)seq.as.obj->len;
        else {
            raise_hint(in, n->line, "Use `for i in 1 to 10:` to count instead.",
                       "cannot go through %s one item at a time.", v_type_name(seq));
            v_release(seq);
            return;
        }
        for (int i = 0; i < count; i++) {
            Value item = seq.type == V_LIST ? v_retain(seq.as.obj->items[i])
                                            : v_text_len(seq.as.obj->text + i, 1);
            if (!env_assign(env, n->name, item)) env_define(env, n->name, item);
            exec_block(in, n->b, env);
            if (in->flow == FLOW_BREAK) { in->flow = FLOW_NORMAL; break; }
            if (in->flow == FLOW_SKIP) { in->flow = FLOW_NORMAL; continue; }
            if (in->flow != FLOW_NORMAL) break;
            if (seq.type == V_LIST && count > seq.as.obj->count) count = seq.as.obj->count;
        }
        v_release(seq);
        return;
    }

    case N_FORRANGE: {
        Value a = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) { v_release(a); return; }
        Value b = eval(in, n->b, env);
        if (in->flow == FLOW_ERROR) { v_release(a); v_release(b); return; }
        if (!is_num(a) || !is_num(b)) {
            raise_hint(in, n->line, "Write `for i in 1 to 10:`.",
                       "counting needs two numbers, got %s and %s.",
                       v_type_name(a), v_type_name(b));
            v_release(a); v_release(b);
            return;
        }
        long long start = (long long)numof(a), end = (long long)numof(b);
        v_release(a); v_release(b);
        long long step = 1;
        if (n->d) {
            Value s = eval(in, n->d, env);
            if (in->flow == FLOW_ERROR) { v_release(s); return; }
            if (!is_num(s)) {
                raise_hint(in, n->line, "Write `for i in 10 to 1 by -1:`.",
                           "the step after `by` must be a number, got %s.", v_type_name(s));
                v_release(s);
                return;
            }
            step = (long long)numof(s);
            v_release(s);
            if (step == 0) {
                raise_hint(in, n->line, "A step of 0 would never reach the end.",
                           "the step after `by` cannot be zero.");
                return;
            }
        }
        for (long long i = start; step > 0 ? i <= end : i >= end; i += step) {
            Value item = v_int(i);
            if (!env_assign(env, n->name, item)) env_define(env, n->name, item);
            exec_block(in, n->c, env);
            if (in->flow == FLOW_BREAK) { in->flow = FLOW_NORMAL; break; }
            if (in->flow == FLOW_SKIP) { in->flow = FLOW_NORMAL; continue; }
            if (in->flow != FLOW_NORMAL) break;
        }
        return;
    }

    case N_FUN:
        env_define(env, n->name, make_func(n, env));
        return;

    case N_RETURN: {
        Value v = n->a ? eval(in, n->a, env) : v_nothing();
        if (in->flow == FLOW_ERROR) { v_release(v); return; }
        v_release(in->retval);
        in->retval = v;
        in->flow = FLOW_RETURN;
        return;
    }

    case N_BREAK: in->flow = FLOW_BREAK; return;
    case N_SKIP:  in->flow = FLOW_SKIP;  return;

    case N_ERROR: {
        Value v = eval(in, n->a, env);
        if (in->flow == FLOW_ERROR) { v_release(v); return; }
        char *s = v_to_text(v);
        v_release(v);
        raise(in, n->line, "%s", s);
        free(s);
        return;
    }

    case N_TRY: {
        exec_block(in, n->a, env);
        if (in->flow != FLOW_ERROR) return;
        Value msg = v_text(in->errmsg ? in->errmsg : "unknown problem");
        in->flow = FLOW_NORMAL;
        in->errprinted = false;
        free(in->errmsg);   in->errmsg = NULL;
        free(in->errhint);  in->errhint = NULL;
        free(in->errsnippet); in->errsnippet = NULL;
        free(in->errfile);  in->errfile = NULL;
        Env *cenv = env_new(env);
        if (n->name2) env_define(cenv, n->name2, msg);
        else v_release(msg);
        exec_block(in, n->b, cenv);
        env_release(cenv);
        return;
    }

    case N_USE:
        exec_use(in, n, env);
        return;

    default: {
        Value v = eval(in, n, env);
        v_release(v);
        return;
    }
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int run_source(Interp *in, const char *src, const char *filename) {
    in->src = src;
    in->filename = filename;
    in->flow = FLOW_NORMAL;

    TokenList toks;
    if (!lex(in, src, &toks)) { report_error(in); return 65; }

    Node *program = parse(in, &toks);
    if (!program) { toklist_free(&toks); report_error(in); return 65; }

    exec_block(in, program, in->globals);
    int code = 0;
    if (in->flow == FLOW_ERROR) { report_error(in); code = 70; }
    in->flow = FLOW_NORMAL;

    /* The program AST is intentionally kept alive: user functions hold
     * pointers into it and may still be reachable from globals. */
    (void)program;
    toklist_free(&toks);
    return code;
}
