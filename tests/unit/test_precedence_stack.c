/**
 * @file
 * @brief Unit testy precedenčního zásobníku a precedenční tabulky.
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
/* AI prompt: spoj staré testy src/parser/tests/precedence_test.c a token_stack_test.c
 * do tests/unit/ podle aktuálního src/precedence_stack.h a tokenů TOK_* z src/token.h. Ověř:
 * - převod tokenů na precedenční symboly (operandy, operátory, závorky, EOF, neplatný token);
 * - prázdný zásobník, LIFO pořadí a růst kapacity;
 * - stackTopTerminal přeskočí neterminál a hranici handle;
 * - precedenceStackPushAfterTopTerminal vloží hranici hned za nejvyšší terminál;
 * - precedenceStackIsFinished;
 * - relace v tabulce: priorita, levá asociativita + - * /, neasociativní relační
 *   operátory, vyšší priorita relačních operátorů než == a !=, závorky, unární minus.
 * Model: Claude Opus 5.5, Claude Code.
 */
#include "../../src/precedence_stack.h"

#include <assert.h>
#include <stdlib.h>

static void test_token_mapping(void) {
    assert(tokenToPrecSymbol(TOK_EOF) == PREC_END);
    assert(tokenToPrecSymbol(TOK_IDENTIFIER) == PREC_OPERAND);
    assert(tokenToPrecSymbol(TOK_INT_LITERAL) == PREC_OPERAND);
    assert(tokenToPrecSymbol(TOK_DOUBLE_LITERAL) == PREC_OPERAND);
    assert(tokenToPrecSymbol(TOK_STRING_LITERAL) == PREC_OPERAND);
    assert(tokenToPrecSymbol(TOK_PLUS) == PREC_PLUS);
    assert(tokenToPrecSymbol(TOK_MINUS) == PREC_MINUS);
    assert(tokenToPrecSymbol(TOK_STAR) == PREC_MUL);
    assert(tokenToPrecSymbol(TOK_SLASH) == PREC_DIV);
    assert(tokenToPrecSymbol(TOK_LT) == PREC_LT);
    assert(tokenToPrecSymbol(TOK_LE) == PREC_LE);
    assert(tokenToPrecSymbol(TOK_GT) == PREC_GT);
    assert(tokenToPrecSymbol(TOK_GE) == PREC_GE);
    assert(tokenToPrecSymbol(TOK_EQ) == PREC_EQ);
    assert(tokenToPrecSymbol(TOK_NEQ) == PREC_NEQ);
    assert(tokenToPrecSymbol(TOK_LPAREN) == PREC_LPAREN);
    assert(tokenToPrecSymbol(TOK_RPAREN) == PREC_RPAREN);
    assert(tokenToPrecSymbol(TOK_KW_THEN) == PREC_INVALID);
    assert(tokenToPrecSymbol(TOK_ASSIGN) == PREC_INVALID);
}

static void test_empty_stack(void) {
    PrecedenceStack stack;
    PrecedenceSymbol value;

    assert(precedenceStackInit(&stack) == EXIT_SUCCESS);
    assert(stack.size == 0);
    assert(precedenceStackIsEmpty(&stack));
    assert(precedenceStackPop(&stack, &value) != EXIT_SUCCESS);
    assert(precedenceStackPeek(&stack, &value) != EXIT_SUCCESS);
    assert(stackTopTerminal(&stack, &value) != EXIT_SUCCESS);
    precedenceStackDestroy(&stack);
}

static void test_lifo_and_growth(void) {
    PrecedenceStack stack;
    PrecedenceSymbol value;

    assert(precedenceStackInit(&stack) == EXIT_SUCCESS);
    size_t initial_capacity = stack.capacity;

    assert(precedenceStackPush(&stack, PREC_PLUS) == EXIT_SUCCESS);
    assert(precedenceStackPush(&stack, PREC_MUL) == EXIT_SUCCESS);
    assert(precedenceStackPeek(&stack, &value) == EXIT_SUCCESS && value == PREC_MUL);
    assert(stack.size == 2);
    assert(precedenceStackPop(&stack, &value) == EXIT_SUCCESS && value == PREC_MUL);
    assert(precedenceStackPop(&stack, &value) == EXIT_SUCCESS && value == PREC_PLUS);
    assert(precedenceStackIsEmpty(&stack));

    for (size_t i = 0; i < initial_capacity + 1; i++) {
        assert(precedenceStackPush(&stack, PREC_OPERAND) == EXIT_SUCCESS);
    }
    assert(stack.size == initial_capacity + 1);
    assert(stack.capacity > initial_capacity);
    for (size_t i = 0; i < initial_capacity + 1; i++) {
        assert(precedenceStackPop(&stack, &value) == EXIT_SUCCESS && value == PREC_OPERAND);
    }
    assert(precedenceStackIsEmpty(&stack));
    precedenceStackDestroy(&stack);
}

static void test_top_terminal_and_boundary(void) {
    PrecedenceStack stack;
    PrecedenceSymbol value;

    /* zásobník: $ E +  -> nejvyšší terminál je + */
    assert(precedenceStackInit(&stack) == EXIT_SUCCESS);
    assert(precedenceStackPush(&stack, PREC_END) == EXIT_SUCCESS);
    assert(precedenceStackPush(&stack, PREC_NONTERMINAL) == EXIT_SUCCESS);
    assert(stackTopTerminal(&stack, &value) == EXIT_SUCCESS && value == PREC_END);
    assert(precedenceStackPush(&stack, PREC_PLUS) == EXIT_SUCCESS);
    assert(precedenceStackPush(&stack, PREC_NONTERMINAL) == EXIT_SUCCESS);
    assert(stackTopTerminal(&stack, &value) == EXIT_SUCCESS && value == PREC_PLUS);

    /* $ E + E  -> $ E + < E  (hranice hned za nejvyšší terminál) */
    assert(precedenceStackPushAfterTopTerminal(&stack, PREC_BOUNDARY) == EXIT_SUCCESS);
    assert(stack.size == 5);
    assert(stack.items[2] == PREC_PLUS);
    assert(stack.items[3] == PREC_BOUNDARY);
    assert(stack.items[4] == PREC_NONTERMINAL);
    assert(stackTopTerminal(&stack, &value) == EXIT_SUCCESS && value == PREC_PLUS);
    precedenceStackDestroy(&stack);
}

static void test_is_finished(void) {
    PrecedenceStack stack;

    assert(precedenceStackInit(&stack) == EXIT_SUCCESS);
    assert(precedenceStackPush(&stack, PREC_END) == EXIT_SUCCESS);
    assert(!precedenceStackIsFinished(&stack));
    assert(precedenceStackPush(&stack, PREC_NONTERMINAL) == EXIT_SUCCESS);
    assert(precedenceStackIsFinished(&stack));
    assert(precedenceStackPush(&stack, PREC_PLUS) == EXIT_SUCCESS);
    assert(!precedenceStackIsFinished(&stack));
    precedenceStackDestroy(&stack);
}

static void test_priority(void) {
    assert(getPrecedenceRelation(PREC_END, PREC_OPERAND) == REL_SHIFT);
    assert(getPrecedenceRelation(PREC_END, PREC_PLUS) == REL_SHIFT);
    assert(getPrecedenceRelation(PREC_OPERAND, PREC_END) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_PLUS, PREC_MUL) == REL_SHIFT);
    assert(getPrecedenceRelation(PREC_MUL, PREC_PLUS) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_PLUS, PREC_LT) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_LT, PREC_PLUS) == REL_SHIFT);
}

static void test_left_associativity(void) {
    assert(getPrecedenceRelation(PREC_PLUS, PREC_PLUS) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_PLUS, PREC_MINUS) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_MINUS, PREC_PLUS) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_MINUS, PREC_MINUS) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_MUL, PREC_MUL) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_MUL, PREC_DIV) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_DIV, PREC_MUL) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_DIV, PREC_DIV) == REL_REDUCE);
}

static void test_relational_operators(void) {
    /* bez asociativity uvnitř skupiny */
    assert(getPrecedenceRelation(PREC_LT, PREC_LT) == REL_ERROR);
    assert(getPrecedenceRelation(PREC_LT, PREC_LE) == REL_ERROR);
    assert(getPrecedenceRelation(PREC_LE, PREC_GT) == REL_ERROR);
    assert(getPrecedenceRelation(PREC_GT, PREC_GE) == REL_ERROR);
    assert(getPrecedenceRelation(PREC_EQ, PREC_NEQ) == REL_ERROR);
    assert(getPrecedenceRelation(PREC_NEQ, PREC_EQ) == REL_ERROR);

    /* < <= > >= mají vyšší prioritu než == != */
    assert(getPrecedenceRelation(PREC_EQ, PREC_LT) == REL_SHIFT);
    assert(getPrecedenceRelation(PREC_NEQ, PREC_GE) == REL_SHIFT);
    assert(getPrecedenceRelation(PREC_LT, PREC_EQ) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_GE, PREC_NEQ) == REL_REDUCE);
}

static void test_parentheses_and_unary(void) {
    assert(getPrecedenceRelation(PREC_LPAREN, PREC_RPAREN) == REL_EQUAL);
    assert(getPrecedenceRelation(PREC_RPAREN, PREC_RPAREN) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_RPAREN, PREC_PLUS) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_LPAREN, PREC_END) == REL_ERROR);
    assert(getPrecedenceRelation(PREC_END, PREC_RPAREN) == REL_ERROR);

    assert(getPrecedenceRelation(PREC_UMINUS, PREC_UMINUS) == REL_SHIFT);
    assert(getPrecedenceRelation(PREC_UMINUS, PREC_MUL) == REL_REDUCE);
    assert(getPrecedenceRelation(PREC_MUL, PREC_UMINUS) == REL_SHIFT);
}

int main(void) {
    test_token_mapping();
    test_empty_stack();
    test_lifo_and_growth();
    test_top_terminal_and_boundary();
    test_is_finished();
    test_priority();
    test_left_associativity();
    test_relational_operators();
    test_parentheses_and_unary();
    return 0;
}
