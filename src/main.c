/* The `chopcap` command line tool: run a file, evaluate a snippet,
 * or start the interactive prompt. */
#include "chopcap.h"
#include <unistd.h>
#include <libgen.h>

char *chopcap_read_file(const char *path);

static const char *USAGE =
"Chopcap " CHOPCAP_VERSION " — programming without the clutter.\n"
"\n"
"Usage:\n"
"  chopcap                     start the interactive prompt\n"
"  chopcap <file.chop>         run a Chopcap program\n"
"  chopcap run <file.chop>     the same thing, spelled out\n"
"  chopcap -e \"say 1 + 1\"      run a line of Chopcap directly\n"
"  chopcap repl                start the interactive prompt\n"
"\n"
"Options:\n"
"  -h, --help                  show this help\n"
"  -v, --version               show the version\n"
"\n"
"Learn more:  https://github.com/ilikemacos/chopcap\n";

static void set_basedir(Interp *in, const char *path) {
    char *copy = xstrdup(path);
    char *dir = dirname(copy);
    free(in->basedir);
    in->basedir = xstrdup(dir);
    free(copy);
}

static int run_file(const char *path) {
    char *src = chopcap_read_file(path);
    if (!src) {
        fprintf(stderr, "\nChopcap Error\n\n  cannot open the file `%s`.\n"
                        "  Check the name and that you are in the right folder.\n\n", path);
        return 66;
    }
    Interp in;
    interp_init(&in);
    set_basedir(&in, path);

    /* Show just the file name in errors, not the whole path. */
    char *copy = xstrdup(path);
    char *base = basename(copy);
    int code = run_source(&in, src, base);
    free(copy);

    interp_free(&in);
    free(src);
    return code;
}

static int run_eval(const char *code) {
    Interp in;
    interp_init(&in);
    in.basedir = xstrdup(".");
    int rc = run_source(&in, code, NULL);
    interp_free(&in);
    return rc;
}

/* ------------------------------------------------------------------ */
/* REPL                                                                */
/* ------------------------------------------------------------------ */

static char *read_line_or_null(const char *prompt) {
    fputs(prompt, stdout);
    fflush(stdout);
    size_t cap = 128, len = 0;
    char *buf = xmalloc(cap);
    int c;
    while ((c = fgetc(stdin)) != EOF && c != '\n') {
        if (len + 1 >= cap) { cap *= 2; buf = xrealloc(buf, cap); }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = 0;
    return buf;
}

static bool blank(const char *s) {
    for (; *s; s++) if (*s != ' ' && *s != '\t' && *s != '\r') return false;
    return true;
}

static bool ends_with_colon(const char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) n--;
    return n > 0 && s[n - 1] == ':';
}

/* Runs one REPL chunk, echoing the value of a lone expression. */
static void repl_eval(Interp *in, const char *src) {
    in->src = src;
    in->filename = NULL;
    in->flow = FLOW_NORMAL;

    TokenList toks;
    if (!lex(in, src, &toks)) { report_error(in); in->flow = FLOW_NORMAL; return; }
    Node *program = parse(in, &toks);
    if (!program) { toklist_free(&toks); report_error(in); in->flow = FLOW_NORMAL; return; }

    if (program->nkids == 1 && program->kids[0]->type == N_EXPRSTMT) {
        Value v = eval(in, program->kids[0]->a, in->globals);
        if (in->flow == FLOW_ERROR) report_error(in);
        else if (v.type != V_NOTHING) {
            char *s = v_repr(v);
            printf("%s\n", s);
            free(s);
        }
        v_release(v);
    } else {
        exec_block(in, program, in->globals);
        if (in->flow == FLOW_ERROR) report_error(in);
    }
    in->flow = FLOW_NORMAL;
    toklist_free(&toks);
    /* The AST is kept alive on purpose: functions defined here point at it. */
}

static int run_repl(void) {
    Interp in;
    interp_init(&in);
    in.repl = true;
    in.basedir = xstrdup(".");

    bool tty = isatty(0);
    if (tty) {
        printf("Chopcap %s\n", CHOPCAP_VERSION);
        printf("Type some Chopcap, or `exit` to quit. `help` shows a reminder.\n\n");
    }

    size_t cap = 0, len = 0;
    char *buffer = NULL;
    bool continuing = false;

    for (;;) {
        char *line = read_line_or_null(tty ? (continuing ? "... " : ">>> ") : "");
        if (!line) break;

        if (!continuing) {
            char *t = line;
            while (*t == ' ' || *t == '\t') t++;
            if (strcmp(t, "exit") == 0 || strcmp(t, "quit") == 0) { free(line); break; }
            if (strcmp(t, "help") == 0) {
                printf("\n  say \"hello\"           print something\n"
                       "  x = 5                 remember a value\n"
                       "  if x > 3:             start a block (blank line ends it)\n"
                       "  fun add(a, b):        define a function\n"
                       "  use math              load a library\n"
                       "  exit                  leave Chopcap\n\n");
                free(line);
                continue;
            }
            if (blank(line)) { free(line); continue; }
        }

        size_t ll = strlen(line);
        if (len + ll + 2 > cap) { cap = (len + ll + 2) * 2; buffer = xrealloc(buffer, cap); }
        memcpy(buffer + len, line, ll);
        len += ll;
        buffer[len++] = '\n';
        buffer[len] = 0;

        if (ends_with_colon(line)) { continuing = true; free(line); continue; }
        if (continuing && !blank(line)) { free(line); continue; }
        free(line);

        continuing = false;
        repl_eval(&in, buffer);
        len = 0;
        if (buffer) buffer[0] = 0;
    }

    if (len > 0) repl_eval(&in, buffer);
    free(buffer);
    if (tty) printf("\nBye.\n");
    interp_free(&in);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) return run_repl();

    const char *a1 = argv[1];
    if (strcmp(a1, "--version") == 0 || strcmp(a1, "-v") == 0 || strcmp(a1, "version") == 0) {
        printf("Chopcap %s\n", CHOPCAP_VERSION);
        return 0;
    }
    if (strcmp(a1, "--help") == 0 || strcmp(a1, "-h") == 0 || strcmp(a1, "help") == 0) {
        fputs(USAGE, stdout);
        return 0;
    }
    if (strcmp(a1, "repl") == 0) return run_repl();
    if (strcmp(a1, "-e") == 0 || strcmp(a1, "--eval") == 0) {
        if (argc < 3) {
            fprintf(stderr, "chopcap: -e needs some Chopcap to run, like: chopcap -e 'say 1 + 1'\n");
            return 64;
        }
        return run_eval(argv[2]);
    }
    if (strcmp(a1, "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "chopcap: run needs a file, like: chopcap run hello.chop\n");
            return 64;
        }
        return run_file(argv[2]);
    }
    if (a1[0] == '-') {
        fprintf(stderr, "chopcap: `%s` is not a known option.\n\n", a1);
        fputs(USAGE, stderr);
        return 64;
    }
    return run_file(a1);
}
