/**
 * @file
 * @brief Unit testy precedenční syntaktické analýzy výrazů (parseExpression).
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
/* AI prompt: vytvoř unit test pro parseExpression podle src/parser.h.
 * Ověř:
 * - prioritu a levou asociativitu operátorů (a + b * c, 10 - 3 - 2, 2 * 6 / 3);
 * - závorky a vnořené závorky;
 * - vyšší prioritu relačních operátorů než == a != (a < b == c);
 * - unární minus (-a, a * -b, a - -b, -(a + b));
 * - literál Unit () jako samostatný výraz, v závorkách, jako operand a jeho pozici;
 * - literály Int, Double a String a identifikátory;
 * - ukončení výrazu tokenem, který do výrazu nepatří (then, do, NEWLINE, =),
 *   který po návratu zůstane v parser.current;
 * - pozice listů AST;
 * - syntaktické chyby (a < b < c, a == b == c, a +, a b, (a, a), prázdný výraz,
 *   binární operátor na začátku výrazu, () za operandem) - kód 2;
 * - lexikální chybu uvnitř výrazu - kód 1.
 * Model: Claude Opus 5.5, Claude Code.
 */
#include "../../src/expression_parser.h"

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
} ExprRun;

static void run_expression(ExprRun *run, const char *text) {
    memset(run, 0, sizeof(*run));
    run->input = tmpfile();
    assert(run->input != NULL);
    assert(fputs(text, run->input) >= 0);
    rewind(run->input);

    assert(lexer_init(&run->lexer, run->input) == LEXER_OK);
    assert(parserInit(&run->parser, &run->lexer, &run->error) == ERR_OK);
    run->code = parseExpression(&run->parser, &run->error, &run->result);
}

static void finish(ExprRun *run) {
    nodeFree(run->result);
    parserDestroy(&run->parser);
    lexer_destroy(&run->lexer);
    fclose(run->input);
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

/* Převede strom výrazu na plně uzávorkovaný text, např. "(a + (b * c))". */
static void ast_to_text(const Node *node, char *buffer, size_t size) {
    size_t used = strlen(buffer);
    assert(used < size);
    char *out = buffer + used;
    size_t left = size - used;

    if (node == NULL) {
        snprintf(out, left, "NULL");
        return;
    }

    switch (node->kind) {
    case N_LIT_INT:
        snprintf(out, left, "%lld", node->ival);
        break;
    case N_LIT_DOUBLE:
        snprintf(out, left, "%g", node->dval);
        break;
    case N_LIT_STRING:
        snprintf(out, left, "\"%s\"", node->name);
        break;
    case N_LIT_UNIT:
        snprintf(out, left, "()");
        break;
    case N_VAR:
        snprintf(out, left, "%s", node->name);
        break;
    case N_UNARY:
        snprintf(out, left, "(-");
        ast_to_text(node->left, buffer, size);
        strncat(buffer, ")", size - strlen(buffer) - 1);
        break;
    case N_BINARY:
        snprintf(out, left, "(");
        ast_to_text(node->left, buffer, size);
        used = strlen(buffer);
        snprintf(buffer + used, size - used, " %s ", op_text(node->op));
        ast_to_text(node->right, buffer, size);
        strncat(buffer, ")", size - strlen(buffer) - 1);
        break;
    case N_CALL:
        snprintf(out, left, "%s(", node->name);
        for (const Node *arg = node->args; arg != NULL; arg = arg->next) {
            ast_to_text(arg, buffer, size);
            if (arg->next != NULL) {
                strncat(buffer, ", ", size - strlen(buffer) - 1);
            }
        }
        strncat(buffer, ")", size - strlen(buffer) - 1);
        break;

    default:
        snprintf(out, left, "<kind %d>", (int)node->kind);
        break;
    }
}

/* Výraz se musí zparsovat na `expected` a zastavit se na tokenu `next`. */
static void expect_tree(const char *text, const char *expected, TokenType next) {
    ExprRun run;
    run_expression(&run, text);

    if (run.code != ERR_OK) {
        fprintf(stderr, "'%s': očekáván strom %s, chyba %d (%s)\n", text, expected, run.code,
                run.error.message != NULL ? run.error.message : "");
    }
    assert(run.code == ERR_OK);

    char buffer[256] = {0};
    ast_to_text(run.result, buffer, sizeof(buffer));
    if (strcmp(buffer, expected) != 0) {
        fprintf(stderr, "'%s': očekáváno %s, dostal %s\n", text, expected, buffer);
    }
    assert(strcmp(buffer, expected) == 0);
    assert(run.parser.current.type == next);

    finish(&run);
}

static void expect_error(const char *text, ErrorCode expected) {
    ExprRun run;
    run_expression(&run, text);

    if (run.code != expected) {
        fprintf(stderr, "'%s': očekávána chyba %d, dostal %d\n", text, expected, run.code);
    }
    assert(run.code == expected);
    assert((int)run.error.code == (int)expected);
    assert(run.result == NULL);

    finish(&run);
}

static void test_precedence_and_associativity(void) {
    expect_tree("a + b * c", "(a + (b * c))", TOK_EOF);
    expect_tree("a * b + c", "((a * b) + c)", TOK_EOF);
    expect_tree("10 - 3 - 2", "((10 - 3) - 2)", TOK_EOF);
    expect_tree("2 * 6 / 3", "((2 * 6) / 3)", TOK_EOF);
    expect_tree("a - b + c", "((a - b) + c)", TOK_EOF);
    expect_tree("a + b < c * d", "((a + b) < (c * d))", TOK_EOF);
    expect_tree("a < b == c", "((a < b) == c)", TOK_EOF);
    expect_tree("a != b >= c", "(a != (b >= c))", TOK_EOF);
}

static void test_parentheses(void) {
    expect_tree("(a + b) * c", "((a + b) * c)", TOK_EOF);
    expect_tree("a * (b + c)", "(a * (b + c))", TOK_EOF);
    expect_tree("((a))", "a", TOK_EOF);
    expect_tree("((a + b) * (c - d))", "((a + b) * (c - d))", TOK_EOF);
}

static void test_unary_minus(void) {
    expect_tree("-a", "(-a)", TOK_EOF);
    expect_tree("a * -b", "(a * (-b))", TOK_EOF);
    expect_tree("a - -b", "(a - (-b))", TOK_EOF);
    expect_tree("-(a + b)", "(-(a + b))", TOK_EOF);
    expect_tree("-a * b", "((-a) * b)", TOK_EOF);
}

static void test_operands(void) {
    expect_tree("x", "x", TOK_EOF);
    expect_tree("42", "42", TOK_EOF);
    expect_tree("1.5 + \"s\"", "(1.5 + \"s\")", TOK_EOF);
}

static void test_unit_literal(void) {
    expect_tree("()", "()", TOK_EOF);
    expect_tree("( )", "()", TOK_EOF);
    expect_tree("(())", "()", TOK_EOF);
    expect_tree("() + a", "(() + a)", TOK_EOF);
    expect_tree("a == ()", "(a == ())", TOK_EOF);
    expect_tree("-()", "(-())", TOK_EOF);
    expect_tree("() then", "()", TOK_KW_THEN);
    expect_tree("()\nx", "()", TOK_NEWLINE);

    /* pozice literálu je pozice otevírací závorky */
    ExprRun run;
    run_expression(&run, "a + ()");
    assert(run.code == ERR_OK);
    assert(run.result != NULL && run.result->kind == N_BINARY);
    assert(run.result->right->kind == N_LIT_UNIT);
    assert(run.result->right->type == TY_UNIT);
    if (run.result->right->line != 1 || run.result->right->col != 5) {
        fprintf(stderr, "'a + ()': pozice () očekávána 1:5, dostal %d:%d\n",
                run.result->right->line, run.result->right->col);
    }
    assert(run.result->right->line == 1 && run.result->right->col == 5);
    finish(&run);
}

static void test_expression_end(void) {
    expect_tree("a < b then", "(a < b)", TOK_KW_THEN);
    expect_tree("x > 0 do", "(x > 0)", TOK_KW_DO);
    expect_tree("a + b\nc", "(a + b)", TOK_NEWLINE);
    expect_tree("a = 1", "a", TOK_ASSIGN);
    expect_tree("a)", "a", TOK_RPAREN);
}

static void test_leaf_position(void) {
    ExprRun run;
    run_expression(&run, "a +  foo");
    assert(run.code == ERR_OK);
    assert(run.result != NULL && run.result->kind == N_BINARY);
    assert(run.result->left->line == 1 && run.result->left->col == 1);
    assert(run.result->right->line == 1 && run.result->right->col == 6);
    finish(&run);
}

static void test_errors(void) {
    expect_error("a < b < c", ERR_SYNTAX);
    expect_error("a == b == c", ERR_SYNTAX);
    expect_error("a +", ERR_SYNTAX);
    expect_error("a b", ERR_SYNTAX);
    expect_error("(a", ERR_SYNTAX);
    expect_error(")", ERR_SYNTAX);
    expect_error("5 ()", ERR_SYNTAX);
    expect_error("() a", ERR_SYNTAX);
    expect_error("+ a", ERR_SYNTAX);
    expect_error("* a", ERR_SYNTAX);
    expect_error("< a", ERR_SYNTAX);
    expect_error("(+ a)", ERR_SYNTAX);
    expect_error("", ERR_SYNTAX);
    expect_error("then", ERR_SYNTAX);
    expect_error("a + 1.2.3", ERR_LEXICAL);
}
static void test_funexp(void) {
    expect_tree("a ()", "a()", TOK_EOF);
    expect_tree("f(a, b + 1) * 2", "(f(a, (b + 1)) * 2)", TOK_EOF);
}

int main(void) {
    test_precedence_and_associativity();
    test_parentheses();
    test_unary_minus();
    test_operands();
    test_unit_literal();
    test_expression_end();
    test_leaf_position();
    test_errors();
    test_funexp();
    return 0;
}
