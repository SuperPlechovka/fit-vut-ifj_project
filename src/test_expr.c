#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "expr.h"
#include "expr_table.h"

/* --- Mockování Lexeru --- */
typedef struct {
    PrecSym sym;
    const char *lexeme;
} TestToken;

static const TestToken *current_tokens = NULL;
static int token_pos = 0;

void setup_lexer_mock(const TestToken tokens[]) {
    current_tokens = tokens;
    token_pos = 0;
}

PrecSym lexer_to_sym(void) {
    if (!current_tokens || current_tokens[token_pos].sym == PS_END) {
        return PS_END;
    }
    return current_tokens[token_pos++].sym;
}

char *lexer_lexeme(void) {
    int idx = token_pos - 1;
    if (idx >= 0 && current_tokens[idx].lexeme) {
        return strdup(current_tokens[idx].lexeme);
    }
    return NULL;
}

/* --- Testovací Framework --- */
static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(name, tokens, expected_ret) do { \
    setup_lexer_mock(tokens); \
    Node *ast = NULL; \
    int res = parse_expression(&ast); \
    if (res == (expected_ret)) { \
        printf("[PASS] %s\n", name); \
        tests_passed++; \
    } else { \
        printf("[FAIL] %s: očekávaný návratový kód %d, získán %d\n", name, expected_ret, res); \
        tests_failed++; \
    } \
    if (ast) nodeFree(ast); \
} while(0)

int main(void) {
    printf("=== TESTOVÁNÍ PRECEDENČNÍHO PARSERU VÝRAZŮ ===\n\n");

    // 1. Platný výraz: "42"
    TestToken t1[] = {
        {PS_INT, "42"},
        {PS_END, NULL}
    };
    RUN_TEST("Jednoduchá konstanta (42)", t1, 0);

    // 2. Platný výraz: "1 + 2 * 3"
    TestToken t2[] = {
        {PS_INT, "1"}, {PS_PLUS, NULL},
        {PS_INT, "2"}, {PS_MUL, NULL},
        {PS_INT, "3"},
        {PS_END, NULL}
    };
    RUN_TEST("Priorita operátorů (1 + 2 * 3)", t2, 0);

    // 3. Platný výraz se závorkami: "(1 + 2) * 3"
    TestToken t3[] = {
        {PS_LPAR, NULL}, {PS_INT, "1"}, {PS_PLUS, NULL}, {PS_INT, "2"}, {PS_RPAR, NULL},
        {PS_MUL, NULL}, {PS_INT, "3"},
        {PS_END, NULL}
    };
    RUN_TEST("Závorky ((1 + 2) * 3)", t3, 0);

    // 4. Unární minus: "-a * b"
    TestToken t4[] = {
        {PS_MINUS, NULL}, {PS_ID, "a"},
        {PS_MUL, NULL}, {PS_ID, "b"},
        {PS_END, NULL}
    };
    RUN_TEST("Unární minus (-a * b)", t4, 0);

    // 5. Relační operátor: "a != b"
    TestToken t5[] = {
        {PS_ID, "a"}, {PS_NE, NULL}, {PS_ID, "b"},
        {PS_END, NULL}
    };
    RUN_TEST("Nerovnost (a != b)", t5, 0);

    // 6. Syntaktická chyby: Neukončená závorka "(1 + 2"
    TestToken t6[] = {
        {PS_LPAR, NULL}, {PS_INT, "1"}, {PS_PLUS, NULL}, {PS_INT, "2"},
        {PS_END, NULL}
    };
    RUN_TEST("Chyba: Neukončená závorka ((1 + 2)", t6, 2);

    // 7. Syntaktická chyba: Dva operátory za sebou "1 + *"
    TestToken t7[] = {
        {PS_INT, "1"}, {PS_PLUS, NULL}, {PS_MUL, NULL},
        {PS_END, NULL}
    };
    RUN_TEST("Chyba: Dva operátory za sebou (1 + *)", t7, 2);

    // 8. Syntaktická chyba: Zakázané řetězení relací "a < b < c"
    TestToken t8[] = {
        {PS_ID, "a"}, {PS_LT, NULL}, {PS_ID, "b"}, {PS_LT, NULL}, {PS_ID, "c"},
        {PS_END, NULL}
    };
    RUN_TEST("Chyba: Neasociativní relace (a < b < c)", t8, 2);

    printf("\n-----------------------------------\n");
    printf("Výsledek: %d PASS / %d FAIL\n", tests_passed, tests_failed);

    return (tests_failed == 0) ? 0 : 1;
}