/* Chopcap errors: short, plain-English reports with the offending line.
 * Beginners should never see an internal stack trace. */
#include "chopcap.h"
#include <stdarg.h>
#include <ctype.h>

/* Copies line number `line` (1-based) out of the current source. */
static char *snippet_for(const char *src, int line) {
    if (!src || line < 1) return NULL;
    const char *p = src;
    int cur = 1;
    while (cur < line && *p) {
        if (*p == '\n') cur++;
        p++;
    }
    if (cur != line) return NULL;
    const char *end = p;
    while (*end && *end != '\n') end++;
    while (end > p && (end[-1] == '\r')) end--;
    /* Trim leading whitespace so the snippet lines up under our own indent. */
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    size_t n = (size_t)(end - p);
    char *out = xmalloc(n + 1);
    memcpy(out, p, n);
    out[n] = 0;
    return out;
}

static void set_error(Interp *in, int line, const char *hint, const char *msg) {
    if (in->flow == FLOW_ERROR) return;   /* keep the first, most specific error */
    in->flow = FLOW_ERROR;
    in->errline = line;
    free(in->errmsg);
    free(in->errhint);
    free(in->errsnippet);
    free(in->errfile);
    in->errmsg = xstrdup(msg);
    in->errhint = hint ? xstrdup(hint) : NULL;
    in->errsnippet = snippet_for(in->src, line);
    in->errfile = in->filename ? xstrdup(in->filename) : NULL;
    in->errprinted = false;
}

void raise(Interp *in, int line, const char *fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    set_error(in, line, NULL, msg);
}

void raise_hint(Interp *in, int line, const char *hint, const char *fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    set_error(in, line, hint, msg);
}

void report_error(Interp *in) {
    if (in->flow != FLOW_ERROR || in->errprinted) return;
    in->errprinted = true;
    fflush(stdout);
    fprintf(stderr, "\nChopcap Error\n\n");
    if (in->errline > 0) {
        if (in->errfile)
            fprintf(stderr, "  %s, line %d:\n", in->errfile, in->errline);
        else
            fprintf(stderr, "  line %d:\n", in->errline);
        if (in->errsnippet && in->errsnippet[0])
            fprintf(stderr, "    %s\n", in->errsnippet);
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "  %s\n", in->errmsg ? in->errmsg : "something went wrong.");
    if (in->errhint) fprintf(stderr, "  %s\n", in->errhint);
    fprintf(stderr, "\n");
}

/* ------------------------------------------------------------------ */
/* "Did you mean ...?" via Levenshtein distance                         */
/* ------------------------------------------------------------------ */

static int edit_distance(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la > 64 || lb > 64) return 99;
    int prev[65], curr[65];
    for (size_t j = 0; j <= lb; j++) prev[j] = (int)j;
    for (size_t i = 1; i <= la; i++) {
        curr[0] = (int)i;
        for (size_t j = 1; j <= lb; j++) {
            int cost = tolower((unsigned char)a[i - 1]) == tolower((unsigned char)b[j - 1]) ? 0 : 1;
            int m = prev[j] + 1;
            if (curr[j - 1] + 1 < m) m = curr[j - 1] + 1;
            if (prev[j - 1] + cost < m) m = prev[j - 1] + cost;
            curr[j] = m;
        }
        memcpy(prev, curr, sizeof(int) * (lb + 1));
    }
    return prev[lb];
}

/* Finds the closest name in scope: a small edit distance, or a name that
 * simply contains the one that was typed (`name` -> `username`). */
char *nearest_name(Env *env, const char *name) {
    const char *best = NULL;
    int bestd = 1000;
    size_t len = strlen(name);
    int limit = len <= 2 ? 0 : (len <= 4 ? 1 : (len <= 7 ? 2 : 3));
    if (limit == 0) return NULL;
    for (Env *s = env; s; s = s->parent) {
        for (int i = 0; i < s->count; i++) {
            const char *cand = s->slots[i].name;
            int d = edit_distance(name, cand);
            if (d > limit && strstr(cand, name)) d = 1;   /* a longer relative */
            if (d <= limit && d < bestd) { bestd = d; best = cand; }
        }
    }
    return best ? xstrdup(best) : NULL;
}
