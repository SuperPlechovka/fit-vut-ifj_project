/**
 * @file
 * @brief Unit testy pravidel <expr_or_call>, <call>, <args> a <term> (parseExprOrCall).
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
/* AI prompt: vytvoř unit test pro parseExprOrCall podle docs/ll_grammar.md (pravidla 44-54).
 * Ověř:
 * - volání bez argumentů, s jedním a s více argumenty (identifikátor, Int, Double, String);
 * - pořadí argumentů v seznamu přes next a pozici uzlu volání (pozice identifikátoru);
 * - rozhodnutí LL(2): id + ( je volání, jinak výraz (a, a + b, (f), 5);
 * - po volání zůstane v parser.current token za ) (i operátor - FUNEXP se neřeší),
 *   po výrazu token za výrazem;
 * - syntaktické chyby: neuzavřené volání, chybějící čárka, prázdný argument, čárka navíc,
 *   výraz nebo volání jako argument (bez FUNEXP jsou argumenty jen termy) - kód 2;
 * - lexikální chybu v argumentu - kód 1.
 * Model: Claude Opus 5.5, Claude Code.
 */
#include "../../src/parser.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *input;
    Lexer lexer;
    Parser parser;
    ParseError error;
    Node *result;
    ErrorCode code;
} CallRun;

static void run_parse(CallRun *run, const char *text) {
    memset(run, 0, sizeof(*run));
    run->input = tmpfile();
    assert(run->input != NULL);
    assert(fputs(text, run->input) >= 0);
    rewind(run->input);

    assert(lexer_init(&run->lexer, run->input) == LEXER_OK);
    assert(parserInit(&run->parser, &run->lexer, &run->error) == ERR_OK);
    run->code = parseExprOrCall(&run->parser, &run->error, &run->result);
}

static void finish(CallRun *run) {
    nodeFree(run->result);
    parserDestroy(&run->parser);
    lexer_destroy(&run->lexer);
    fclose(run->input);
}

static void append_text(char *buffer, size_t size, const char *text) {
    strncat(buffer, text, size - strlen(buffer) - 1);
}

/* Převede uzel na text: volání jako "f(a, 1)", výrazy zjednodušeně ("a", "<binary>"). */
static void node_to_text(const Node *node, char *buffer, size_t size) {
    char tmp[64];

    if (node == NULL) {
        append_text(buffer, size, "NULL");
        return;
    }

    switch (node->kind) {
    case N_LIT_INT:
        snprintf(tmp, sizeof(tmp), "%lld", node->ival);
        append_text(buffer, size, tmp);
        break;
    case N_LIT_DOUBLE:
        snprintf(tmp, sizeof(tmp), "%g", node->dval);
        append_text(buffer, size, tmp);
        break;
    case N_LIT_STRING:
        append_text(buffer, size, "\"");
        append_text(buffer, size, node->name);
        append_text(buffer, size, "\"");
        break;
    case N_LIT_UNIT:
        append_text(buffer, size, "()");
        break;
    case N_VAR:
        append_text(buffer, size, node->name);
        break;
    case N_BINARY:
        append_text(buffer, size, "<binary>");
        break;
    case N_UNARY:
        append_text(buffer, size, "<unary>");
        break;
    case N_CALL:
        append_text(buffer, size, node->name);
        append_text(buffer, size, "(");
        for (const Node *arg = node->args; arg != NULL; arg = arg->next) {
            node_to_text(arg, buffer, size);
            if (arg->next != NULL) {
                append_text(buffer, size, ", ");
            }
        }
        append_text(buffer, size, ")");
        break;
    default:
        snprintf(tmp, sizeof(tmp), "<kind %d>", (int)node->kind);
        append_text(buffer, size, tmp);
        break;
    }
}

/* Vstup se musí zparsovat na `expected` a zastavit se na tokenu `next`. */
static void expect_tree(const char *text, const char *expected, TokenType next) {
    CallRun run;
    run_parse(&run, text);

    if (run.code != ERR_OK) {
        fprintf(stderr, "'%s': očekáváno %s, chyba %d (%s)\n", text, expected, run.code,
                run.error.message != NULL ? run.error.message : "");
    }
    assert(run.code == ERR_OK);

    char buffer[256] = {0};
    node_to_text(run.result, buffer, sizeof(buffer));
    if (strcmp(buffer, expected) != 0) {
        fprintf(stderr, "'%s': očekáváno %s, dostal %s\n", text, expected, buffer);
    }
    assert(strcmp(buffer, expected) == 0);

    if (run.parser.current.type != next) {
        fprintf(stderr, "'%s': po parsování očekáván token %d, je %d\n", text, (int)next,
                (int)run.parser.current.type);
    }
    assert(run.parser.current.type == next);

    finish(&run);
}

static void expect_error(const char *text, ErrorCode expected) {
    CallRun run;
    run_parse(&run, text);

    if (run.code != expected) {
        fprintf(stderr, "'%s': očekávána chyba %d, dostal %d\n", text, expected, run.code);
    }
    assert(run.code == expected);
    assert((int)run.error.code == (int)expected);
    assert(run.result == NULL);

    finish(&run);
}

static void test_call_arguments(void) {
    expect_tree("f()", "f()", TOK_EOF);
    expect_tree("f(x)", "f(x)", TOK_EOF);
    expect_tree("f(1)", "f(1)", TOK_EOF);
    expect_tree("f(1.5)", "f(1.5)", TOK_EOF);
    expect_tree("f(\"s\")", "f(\"s\")", TOK_EOF);
    expect_tree("f(a, 1, 2.5, \"s\")", "f(a, 1, 2.5, \"s\")", TOK_EOF);
    expect_tree("print(x, y, z)", "print(x, y, z)", TOK_EOF);
}

static void test_call_end(void) {
    expect_tree("f(a)\nx", "f(a)", TOK_NEWLINE);
    expect_tree("f(a); x", "f(a)", TOK_SEMICOLON);
    /* volání uvnitř výrazu (FUNEXP) neřeší parseExprOrCall, + nechá pro volajícího */
    expect_tree("f(a) + 1", "f(a)", TOK_PLUS);
}

static void test_call_position(void) {
    CallRun run;
    run_parse(&run, "foo(a,  b)");
    assert(run.code == ERR_OK);
    assert(run.result != NULL && run.result->kind == N_CALL);
    assert(run.result->line == 1 && run.result->col == 1);
    assert(run.result->args != NULL && run.result->args->col == 5);
    assert(run.result->args->next != NULL && run.result->args->next->col == 9);
    assert(run.result->args->next->next == NULL);
    finish(&run);
}

/* LL(2): id následovaný ( je volání, cokoli jiného je výraz */
static void test_expression_branch(void) {
    expect_tree("a", "a", TOK_EOF);
    expect_tree("5", "5", TOK_EOF);
    expect_tree("a + b", "<binary>", TOK_EOF);
    expect_tree("-a", "<unary>", TOK_EOF);
    expect_tree("(f)", "f", TOK_EOF);
    expect_tree("()", "()", TOK_EOF);
    expect_tree("a then", "a", TOK_KW_THEN);
}

static void test_errors(void) {
    expect_error("f(", ERR_SYNTAX);
    expect_error("f(a", ERR_SYNTAX);
    expect_error("f(a b)", ERR_SYNTAX);
    expect_error("f(,)", ERR_SYNTAX);
    expect_error("f(a,)", ERR_SYNTAX);
    expect_error("f(a,,b)", ERR_SYNTAX);
    expect_error("f(a + b)", ERR_SYNTAX);
    expect_error("f(g(x))", ERR_SYNTAX);
    expect_error("f(1.2.3)", ERR_LEXICAL);
}

int main(void) {
    test_call_arguments();
    test_call_end();
    test_call_position();
    test_expression_branch();
    test_errors();
    return 0;
}
