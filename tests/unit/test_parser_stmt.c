/**
 * @file
 * @brief Unit testy jednoduchých příkazů (parseSimpleStatement).
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
/* AI prompt: vytvoř unit test pro parseSimpleStatement podle docs/ll_grammar.md
 * (pravidla 32-41). Ověř:
 * - definici proměnné val/var s typem i bez něj (N_DECL, is_var, type, init);
 * - přiřazení (N_ASSIGN) a rozhodnutí LL(2) podle tokenu = za identifikátorem;
 * - return s hodnotou i bez ní (N_RETURN, left == NULL);
 * - samostatný výraz nebo volání jako příkaz (uzel výrazu bez obalu);
 * - že se příkaz zastaví na NEWLINE nebo ; a nesní je (oddělovač patří k <statement>);
 * - pozici uzlu příkazu = pozice prvního tokenu příkazu;
 * - syntaktické chyby (chybějící id, =, typ, výraz; špatný typ; výraz jako argument) - kód 2;
 * - lexikální chybu - kód 1.
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
} StmtRun;

static void run_parse(StmtRun *run, const char *text) {
    memset(run, 0, sizeof(*run));
    run->input = tmpfile();
    assert(run->input != NULL);
    assert(fputs(text, run->input) >= 0);
    rewind(run->input);

    assert(lexer_init(&run->lexer, run->input) == LEXER_OK);
    assert(parserInit(&run->parser, &run->lexer, &run->error) == ERR_OK);
    run->code = parseSimpleStatement(&run->parser, &run->error, &run->result);
}

static void finish(StmtRun *run) {
    nodeFree(run->result);
    parserDestroy(&run->parser);
    lexer_destroy(&run->lexer);
    fclose(run->input);
}

static void append_text(char *buffer, size_t size, const char *text) {
    strncat(buffer, text, size - strlen(buffer) - 1);
}

static const char *op_text(OpKind op) {
    switch (op) {
    case OP_ADD:
        return "+";
    case OP_SUB:
        return "-";
    case OP_MUL:
        return "*";
    case OP_DIV:
        return "/";
    case OP_LT:
        return "<";
    case OP_LE:
        return "<=";
    case OP_GT:
        return ">";
    case OP_GE:
        return ">=";
    case OP_EQ:
        return "==";
    case OP_NE:
        return "!=";
    default:
        return "?";
    }
}

static const char *type_text(TypeKind type) {
    switch (type) {
    case TY_INT:
        return "Int";
    case TY_DOUBLE:
        return "Double";
    case TY_STRING:
        return "String";
    case TY_UNIT:
        return "Unit";
    default:
        return "?";
    }
}

/* Převede příkaz na text: "val x: Int = (a + 1)", "x = f(a)", "return", "return x". */
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
    case N_UNARY:
        append_text(buffer, size, "(-");
        node_to_text(node->left, buffer, size);
        append_text(buffer, size, ")");
        break;
    case N_BINARY:
        append_text(buffer, size, "(");
        node_to_text(node->left, buffer, size);
        append_text(buffer, size, " ");
        append_text(buffer, size, op_text(node->op));
        append_text(buffer, size, " ");
        node_to_text(node->right, buffer, size);
        append_text(buffer, size, ")");
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
    case N_DECL:
        append_text(buffer, size, node->is_var ? "var " : "val ");
        append_text(buffer, size, node->name);
        if (node->type != TY_UNKNOWN) {
            append_text(buffer, size, ": ");
            append_text(buffer, size, type_text(node->type));
        }
        append_text(buffer, size, " = ");
        node_to_text(node->left, buffer, size);
        break;
    case N_ASSIGN:
        append_text(buffer, size, node->name);
        append_text(buffer, size, " = ");
        node_to_text(node->left, buffer, size);
        break;
    case N_RETURN:
        append_text(buffer, size, "return");
        if (node->left != NULL) {
            append_text(buffer, size, " ");
            node_to_text(node->left, buffer, size);
        }
        break;
    default:
        snprintf(tmp, sizeof(tmp), "<kind %d>", (int)node->kind);
        append_text(buffer, size, tmp);
        break;
    }
}

/* Vstup se musí zparsovat na `expected` a zastavit se na tokenu `next`. */
static void expect_stmt(const char *text, const char *expected, TokenType next) {
    StmtRun run;
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
    StmtRun run;
    run_parse(&run, text);

    if (run.code != expected) {
        fprintf(stderr, "'%s': očekávána chyba %d, dostal %d\n", text, expected, run.code);
    }
    assert(run.code == expected);
    assert((int)run.error.code == (int)expected);
    assert(run.result == NULL);

    finish(&run);
}

static void test_var_def(void) {
    expect_stmt("val x = 5", "val x = 5", TOK_EOF);
    expect_stmt("var x = 5", "var x = 5", TOK_EOF);
    expect_stmt("val x: Int = 5", "val x: Int = 5", TOK_EOF);
    expect_stmt("var d: Double = 1.5", "var d: Double = 1.5", TOK_EOF);
    expect_stmt("val s: String = \"a\"", "val s: String = \"a\"", TOK_EOF);
    expect_stmt("var u: Unit = ()", "var u: Unit = ()", TOK_EOF);
    expect_stmt("val y = a + b * c", "val y = (a + (b * c))", TOK_EOF);
    expect_stmt("val n = readInt()", "val n = readInt()", TOK_EOF);
    expect_stmt("var r: Int = f(a, 1)", "var r: Int = f(a, 1)", TOK_EOF);
    expect_stmt("var x = f(a + b)", "var x = f((a + b))", TOK_EOF);
}

static void test_var_def_fields(void) {
    StmtRun run;

    run_parse(&run, "val x = 5");
    assert(run.code == ERR_OK);
    assert(run.result != NULL && run.result->kind == N_DECL);
    assert(run.result->is_var == 0);
    assert(run.result->type == TY_UNKNOWN);
    assert(strcmp(run.result->name, "x") == 0);
    assert(run.result->left != NULL && run.result->left->kind == N_LIT_INT);
    finish(&run);

    run_parse(&run, "var y: String = s");
    assert(run.code == ERR_OK);
    assert(run.result != NULL && run.result->kind == N_DECL);
    assert(run.result->is_var == 1);
    assert(run.result->type == TY_STRING);
    assert(strcmp(run.result->name, "y") == 0);
    assert(run.result->left != NULL && run.result->left->kind == N_VAR);
    finish(&run);
}

static void test_assign(void) {
    expect_stmt("x = 5", "x = 5", TOK_EOF);
    expect_stmt("x = a - 1", "x = (a - 1)", TOK_EOF);
    expect_stmt("x = f(y)", "x = f(y)", TOK_EOF);
    expect_stmt("x = ()", "x = ()", TOK_EOF);
}

static void test_return(void) {
    expect_stmt("return", "return", TOK_EOF);
    expect_stmt("return\nx", "return", TOK_NEWLINE);
    expect_stmt("return; x", "return", TOK_SEMICOLON);
    expect_stmt("return x", "return x", TOK_EOF);
    expect_stmt("return a + b", "return (a + b)", TOK_EOF);
    expect_stmt("return f(x, 2)", "return f(x, 2)", TOK_EOF);
    expect_stmt("return ()", "return ()", TOK_EOF);
}

/* samostatný výraz nebo volání je taky příkaz; LL(2): id bez = za sebou není přiřazení */
static void test_expression_statement(void) {
    expect_stmt("print(x)", "print(x)", TOK_EOF);
    expect_stmt("f()", "f()", TOK_EOF);
    expect_stmt("x", "x", TOK_EOF);
    expect_stmt("a + b", "(a + b)", TOK_EOF);
    expect_stmt("()", "()", TOK_EOF);
    expect_stmt("x == 5", "(x == 5)", TOK_EOF);
}

/* oddělovač příkazů (NEWLINE, ;) patří k <statement>, parseSimpleStatement ho nesní */
static void test_statement_end(void) {
    expect_stmt("val x = 5\ny", "val x = 5", TOK_NEWLINE);
    expect_stmt("val x = 5; y", "val x = 5", TOK_SEMICOLON);
    expect_stmt("x = f(a)\ny", "x = f(a)", TOK_NEWLINE);
    expect_stmt("x = 1; y = 2", "x = 1", TOK_SEMICOLON);
    expect_stmt("print(x)\n", "print(x)", TOK_NEWLINE);
}

static void test_position(void) {
    StmtRun run;

    run_parse(&run, "val x = 5");
    assert(run.code == ERR_OK);
    assert(run.result->line == 1 && run.result->col == 1);
    finish(&run);

    run_parse(&run, "x = 1; var y = 2");
    assert(run.code == ERR_OK);
    assert(run.result->line == 1 && run.result->col == 1);
    finish(&run);

    run_parse(&run, "return x");
    assert(run.code == ERR_OK);
    assert(run.result->line == 1 && run.result->col == 1);
    finish(&run);
}

static void test_errors(void) {
    /* definice proměnné */
    expect_error("val = 5", ERR_SYNTAX);
    expect_error("val 5 = 5", ERR_SYNTAX);
    expect_error("val val = 5", ERR_SYNTAX);
    expect_error("val x 5", ERR_SYNTAX);
    expect_error("val x", ERR_SYNTAX);
    expect_error("val x =", ERR_SYNTAX);
    expect_error("val x: = 5", ERR_SYNTAX);
    expect_error("val x: Foo = 5", ERR_SYNTAX);
    expect_error("val x: Int", ERR_SYNTAX);
    expect_error("val x: Int 5", ERR_SYNTAX);
    expect_error("var x = 5 +", ERR_SYNTAX);

    /* přiřazení */
    expect_error("x =", ERR_SYNTAX);
    expect_error("x = = 1", ERR_SYNTAX);
    expect_error("x = f(", ERR_SYNTAX);

    /* return */
    expect_error("return )", ERR_SYNTAX);
    expect_error("return a +", ERR_SYNTAX);

    /* lexikální chyba */
    expect_error("val x = 1.2.3", ERR_LEXICAL);
}

int main(void) {
    test_var_def();
    test_var_def_fields();
    test_assign();
    test_return();
    test_expression_statement();
    test_statement_end();
    test_position();
    test_errors();
    return 0;
}
