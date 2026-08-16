/* Unit tests for the pieces below the language surface: the lexer, the
 * parser, values and environments.  Program behaviour is covered by the
 * golden-output tests in tests/programs. */
#include "../src/chopcap.h"

static int checks = 0, failures = 0;

static void check(bool cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

/* ------------------------------------------------------------------ */
/* Lexer                                                               */
/* ------------------------------------------------------------------ */

static void test_lexer_basics(void) {
    Interp in;
    interp_init(&in);
    TokenList t;
    check(lex(&in, "say \"hi\"\n", &t), "lexes a say statement");
    check(t.count >= 3, "produces tokens");
    check(t.items[0].type == T_SAY, "say is a keyword");
    check(t.items[1].type == T_TEXT, "text literal token");
    check(strcmp(t.items[1].text, "hi") == 0, "text literal contents");
    check(t.items[2].type == T_NEWLINE, "newline ends the line");
    check(t.items[t.count - 1].type == T_EOF, "ends with EOF");
    toklist_free(&t);
    interp_free(&in);
}

static void test_lexer_numbers(void) {
    Interp in;
    interp_init(&in);
    TokenList t;
    check(lex(&in, "1 2.5 1_000 3e2\n", &t), "lexes numbers");
    check(t.items[0].type == T_INT && t.items[0].ival == 1, "integer literal");
    check(t.items[1].type == T_FLOAT && t.items[1].fval == 2.5, "decimal literal");
    check(t.items[2].type == T_INT && t.items[2].ival == 1000, "underscores are ignored");
    check(t.items[3].type == T_FLOAT && t.items[3].fval == 300.0, "exponent literal");
    toklist_free(&t);
    interp_free(&in);
}

static void test_lexer_indentation(void) {
    Interp in;
    interp_init(&in);
    TokenList t;
    check(lex(&in, "if true:\n    say 1\nsay 2\n", &t), "lexes an indented block");
    int indents = 0, dedents = 0;
    for (int i = 0; i < t.count; i++) {
        if (t.items[i].type == T_INDENT) indents++;
        if (t.items[i].type == T_DEDENT) dedents++;
    }
    check(indents == 1, "one INDENT token");
    check(dedents == 1, "one DEDENT token");
    toklist_free(&t);
    interp_free(&in);
}

static void test_lexer_errors(void) {
    Interp in;
    interp_init(&in);
    TokenList t;
    check(!lex(&in, "say \"unclosed\n", &t), "unterminated text is rejected");
    check(in.flow == FLOW_ERROR, "error flow is set");
    check(in.errline == 1, "error carries the line number");
    interp_free(&in);

    interp_init(&in);
    check(!lex(&in, "say 1 !\n", &t), "stray ! is rejected");
    interp_free(&in);

    interp_init(&in);
    check(!lex(&in, "say $\n", &t), "unknown character is rejected");
    interp_free(&in);

    interp_init(&in);
    check(!lex(&in, "if true:\n    say 1\n  say 2\n", &t), "bad dedent is rejected");
    interp_free(&in);
}

static void test_lexer_comments(void) {
    Interp in;
    interp_init(&in);
    TokenList t;
    check(lex(&in, "# just a comment\nsay 1 # trailing\n", &t), "lexes comments");
    check(t.items[0].type == T_SAY, "comment-only lines vanish");
    for (int i = 0; i < t.count; i++)
        check(t.items[i].type != T_NAME || strstr(t.items[i].text, "comment") == NULL,
              "comment text is not tokenised");
    toklist_free(&t);
    interp_free(&in);
}

/* ------------------------------------------------------------------ */
/* Parser                                                              */
/* ------------------------------------------------------------------ */

static Node *parse_str(Interp *in, const char *src, TokenList *t) {
    if (!lex(in, src, t)) return NULL;
    return parse(in, t);
}

static void test_parser_shapes(void) {
    Interp in;
    interp_init(&in);
    TokenList t;

    Node *p = parse_str(&in, "x = 1 + 2\n", &t);
    check(p != NULL, "parses an assignment");
    check(p->nkids == 1 && p->kids[0]->type == N_ASSIGN, "assignment node");
    check(strcmp(p->kids[0]->name, "x") == 0, "assignment target name");
    check(p->kids[0]->a->type == N_BINARY, "right hand side is a binary node");
    toklist_free(&t);
    interp_free(&in);

    interp_init(&in);
    p = parse_str(&in, "if a:\n    say 1\nelse:\n    say 2\n", &t);
    check(p && p->kids[0]->type == N_IF, "parses if/else");
    check(p && p->kids[0]->c != NULL, "if has an else branch");
    toklist_free(&t);
    interp_free(&in);

    interp_init(&in);
    p = parse_str(&in, "fun add(a, b):\n    return a + b\n", &t);
    check(p && p->kids[0]->type == N_FUN, "parses a function");
    check(p && p->kids[0]->nparams == 2, "function has two parameters");
    check(p && strcmp(p->kids[0]->params[1], "b") == 0, "second parameter name");
    toklist_free(&t);
    interp_free(&in);

    interp_init(&in);
    p = parse_str(&in, "for i in 1 to 3:\n    say i\n", &t);
    check(p && p->kids[0]->type == N_FORRANGE, "parses a counting loop");
    toklist_free(&t);
    interp_free(&in);

    interp_init(&in);
    p = parse_str(&in, "for x in items:\n    say x\n", &t);
    check(p && p->kids[0]->type == N_FOR, "parses a list loop");
    toklist_free(&t);
    interp_free(&in);
}

static void test_parser_precedence(void) {
    Interp in;
    interp_init(&in);
    TokenList t;
    /* 1 + 2 * 3 must parse as 1 + (2 * 3) */
    Node *p = parse_str(&in, "x = 1 + 2 * 3\n", &t);
    check(p != NULL, "parses precedence sample");
    Node *sum = p->kids[0]->a;
    check(sum->type == N_BINARY && sum->op == T_PLUS, "addition is the root");
    check(sum->b->type == N_BINARY && sum->b->op == T_STAR, "multiplication binds tighter");
    toklist_free(&t);
    interp_free(&in);

    interp_init(&in);
    p = parse_str(&in, "x = a or b and c\n", &t);
    check(p && p->kids[0]->a->type == N_OR, "`or` is the loosest operator");
    check(p && p->kids[0]->a->b->type == N_AND, "`and` binds tighter than `or`");
    toklist_free(&t);
    interp_free(&in);
}

static void test_parser_errors(void) {
    Interp in;
    TokenList t;

    interp_init(&in);
    check(parse_str(&in, "if true\n    say 1\n", &t) == NULL, "missing colon is rejected");
    check(in.errline == 1, "parse error reports its line");
    toklist_free(&t);
    interp_free(&in);

    interp_init(&in);
    check(parse_str(&in, "say (1 + 2\n", &t) == NULL, "unclosed parenthesis is rejected");
    toklist_free(&t);
    interp_free(&in);

    interp_init(&in);
    check(parse_str(&in, "1 + 1 = 2\n", &t) == NULL, "assigning to an expression is rejected");
    toklist_free(&t);
    interp_free(&in);

    interp_init(&in);
    check(parse_str(&in, "else:\n    say 1\n", &t) == NULL, "a stray else is rejected");
    toklist_free(&t);
    interp_free(&in);
}

/* ------------------------------------------------------------------ */
/* Values and environments                                             */
/* ------------------------------------------------------------------ */

static void test_values(void) {
    Value a = v_int(3), b = v_float(3.0), c = v_text("hi");
    check(v_equal(a, b), "3 equals 3.0");
    check(!v_equal(a, c), "a number does not equal text");
    check(v_truthy(a) && !v_truthy(v_int(0)), "numbers are truthy unless zero");
    check(!v_truthy(v_text("")) && v_truthy(c), "text is truthy unless empty");
    check(!v_truthy(v_nothing()), "nothing is falsy");
    check(strcmp(v_type_name(a), "number") == 0, "int type name");
    check(strcmp(v_type_name(b), "decimal") == 0, "float type name");
    check(strcmp(v_type_name(c), "text") == 0, "text type name");

    char *s = v_to_text(c);
    check(strcmp(s, "hi") == 0, "text displays unquoted");
    free(s);
    s = v_repr(c);
    check(strcmp(s, "\"hi\"") == 0, "text inside a list shows quotes");
    free(s);

    Value list = v_list();
    list_push(list, v_int(1));
    list_push(list, v_text("two"));
    s = v_to_text(list);
    check(strcmp(s, "[1, \"two\"]") == 0, "lists display their items");
    free(s);
    check(v_truthy(list), "a list with items is truthy");
    v_release(list);
    v_release(c);
}

static void test_env(void) {
    Env *g = env_new(NULL);
    env_define(g, "x", v_int(1));
    check(env_lookup(g, "x") != NULL, "defined names are found");
    check(env_lookup(g, "y") == NULL, "undefined names are not");

    Env *child = env_new(g);
    check(env_lookup(child, "x") != NULL, "children see their parent");
    check(env_assign(child, "x", v_int(9)), "assignment reaches the parent");
    check(env_lookup(g, "x")->as.i == 9, "the parent value changed");
    check(!env_assign(child, "z", v_int(1)), "assigning an unknown name fails");

    env_define(child, "x", v_int(5));
    check(env_lookup(child, "x")->as.i == 5, "a child can shadow a name");
    check(env_lookup(g, "x")->as.i == 9, "shadowing leaves the parent alone");

    char *near = nearest_name(g, "xx");
    free(near);
    env_define(g, "username", v_text("theo"));
    near = nearest_name(g, "name");
    check(near && strcmp(near, "username") == 0, "did-you-mean finds a longer relative");
    free(near);
    near = nearest_name(g, "usernme");
    check(near && strcmp(near, "username") == 0, "did-you-mean finds a typo");
    free(near);
    near = nearest_name(g, "completely_unrelated_thing");
    check(near == NULL, "did-you-mean stays quiet when nothing is close");
    free(near);

    env_release(child);
    env_release(g);
}

int main(void) {
    test_lexer_basics();
    test_lexer_numbers();
    test_lexer_indentation();
    test_lexer_errors();
    test_lexer_comments();
    test_parser_shapes();
    test_parser_precedence();
    test_parser_errors();
    test_values();
    test_env();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
