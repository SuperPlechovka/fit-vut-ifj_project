/**
 * @file
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
#include "precedence_stack.h"
#include <stdlib.h>
#include <string.h>

#define PREC_TERMINAL_COUNT 15
#define INIT_STACK_CAPACITY 128

PrecedenceSymbol tokenToPrecSymbol(TokenType kind) {
    switch (kind) {
    case TOK_PLUS:
        return PREC_PLUS;
    case TOK_MINUS:
        return PREC_MINUS;
    case TOK_STAR:
        return PREC_MUL;
    case TOK_SLASH:
        return PREC_DIV;
    case TOK_LT:
        return PREC_LT;
    case TOK_LE:
        return PREC_LE;
    case TOK_GT:
        return PREC_GT;
    case TOK_GE:
        return PREC_GE;
    case TOK_EQ:
        return PREC_EQ;
    case TOK_NEQ:
        return PREC_NEQ;
    case TOK_LPAREN:
        return PREC_LPAREN;
    case TOK_RPAREN:
        return PREC_RPAREN;
    case TOK_EOF:
        return PREC_END;

    case TOK_IDENTIFIER:
    case TOK_INT_LITERAL:
    case TOK_DOUBLE_LITERAL:
    case TOK_STRING_LITERAL:
        return PREC_OPERAND;

    default:
        return PREC_INVALID;
    }
}

int precedenceStackInit(PrecedenceStack *stack) {
    PrecedenceSymbol *arr = malloc(sizeof(*arr) * INIT_STACK_CAPACITY);
    if (arr == NULL) {
        return EXIT_FAILURE;
    }

    stack->items = arr;
    stack->capacity = INIT_STACK_CAPACITY;
    stack->size = 0;

    return EXIT_SUCCESS;
}

int precedenceStackPush(PrecedenceStack *stack, PrecedenceSymbol symbol) {
    if (stack->size >= stack->capacity) {
        size_t new_capacity = stack->capacity * 2;
        void *result = realloc(stack->items, new_capacity * sizeof *stack->items);
        if (result == NULL) {
            return EXIT_FAILURE;
        }
        stack->items = result;
        stack->capacity = new_capacity;
    }

    stack->items[stack->size] = symbol;
    stack->size++;
    return EXIT_SUCCESS;
}

int precedenceStackPop(PrecedenceStack *stack, PrecedenceSymbol *result) {
    if (stack->size == 0) {
        return EXIT_FAILURE;
    }

    stack->size--;
    *result = stack->items[stack->size];

    return EXIT_SUCCESS;
}
int precedenceStackPeek(const PrecedenceStack *stack, PrecedenceSymbol *result) {
    if (stack == NULL || result == NULL || stack->size == 0) {
        return EXIT_FAILURE;
    }

    *result = stack->items[stack->size - 1];
    return EXIT_SUCCESS;
}

bool precedenceStackIsEmpty(const PrecedenceStack *stack) { return stack->size == 0; }

void precedenceStackDestroy(PrecedenceStack *stack) { free(stack->items); }

static const PrecedenceRelation precedenceTable[PREC_TERMINAL_COUNT][PREC_TERMINAL_COUNT] =
    {
        [PREC_UMINUS] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_REDUCE,
                [PREC_MUL] = REL_REDUCE,
                [PREC_DIV] = REL_REDUCE,
                [PREC_MINUS] = REL_REDUCE,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_END] = REL_REDUCE,
                [PREC_EQ] = REL_REDUCE,
                [PREC_NEQ] = REL_REDUCE,
                [PREC_GT] = REL_REDUCE,
                [PREC_GE] = REL_REDUCE,
                [PREC_LT] = REL_REDUCE,
                [PREC_LE] = REL_REDUCE,
            },

        [PREC_PLUS] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_REDUCE,
                [PREC_MUL] = REL_SHIFT,
                [PREC_DIV] = REL_SHIFT,
                [PREC_MINUS] = REL_REDUCE,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_END] = REL_REDUCE,
                [PREC_EQ] = REL_REDUCE,
                [PREC_NEQ] = REL_REDUCE,
                [PREC_GT] = REL_REDUCE,
                [PREC_GE] = REL_REDUCE,
                [PREC_LT] = REL_REDUCE,
                [PREC_LE] = REL_REDUCE,
            },

        [PREC_MUL] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_REDUCE,
                [PREC_MUL] = REL_REDUCE,
                [PREC_DIV] = REL_REDUCE,
                [PREC_MINUS] = REL_REDUCE,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_END] = REL_REDUCE,
                [PREC_EQ] = REL_REDUCE,
                [PREC_NEQ] = REL_REDUCE,
                [PREC_GT] = REL_REDUCE,
                [PREC_GE] = REL_REDUCE,
                [PREC_LT] = REL_REDUCE,
                [PREC_LE] = REL_REDUCE,
            },

        [PREC_DIV] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_REDUCE,
                [PREC_MUL] = REL_REDUCE,
                [PREC_DIV] = REL_REDUCE,
                [PREC_MINUS] = REL_REDUCE,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_END] = REL_REDUCE,
                [PREC_EQ] = REL_REDUCE,
                [PREC_NEQ] = REL_REDUCE,
                [PREC_GT] = REL_REDUCE,
                [PREC_GE] = REL_REDUCE,
                [PREC_LT] = REL_REDUCE,
                [PREC_LE] = REL_REDUCE,
            },

        [PREC_MINUS] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_REDUCE,
                [PREC_MUL] = REL_SHIFT,
                [PREC_DIV] = REL_SHIFT,
                [PREC_MINUS] = REL_REDUCE,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_END] = REL_REDUCE,
                [PREC_EQ] = REL_REDUCE,
                [PREC_NEQ] = REL_REDUCE,
                [PREC_GT] = REL_REDUCE,
                [PREC_GE] = REL_REDUCE,
                [PREC_LT] = REL_REDUCE,
                [PREC_LE] = REL_REDUCE,
            },

        [PREC_LPAREN] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_SHIFT,
                [PREC_MUL] = REL_SHIFT,
                [PREC_DIV] = REL_SHIFT,
                [PREC_MINUS] = REL_SHIFT,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_RPAREN] = REL_EQUAL,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_EQ] = REL_SHIFT,
                [PREC_NEQ] = REL_SHIFT,
                [PREC_GT] = REL_SHIFT,
                [PREC_GE] = REL_SHIFT,
                [PREC_LT] = REL_SHIFT,
                [PREC_LE] = REL_SHIFT,
            },

        [PREC_RPAREN] =
            {
                [PREC_UMINUS] = REL_REDUCE,
                [PREC_PLUS] = REL_REDUCE,
                [PREC_MUL] = REL_REDUCE,
                [PREC_DIV] = REL_REDUCE,
                [PREC_MINUS] = REL_REDUCE,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_END] = REL_REDUCE,
                [PREC_EQ] = REL_REDUCE,
                [PREC_NEQ] = REL_REDUCE,
                [PREC_GT] = REL_REDUCE,
                [PREC_GE] = REL_REDUCE,
                [PREC_LT] = REL_REDUCE,
                [PREC_LE] = REL_REDUCE,
            },

        [PREC_OPERAND] =
            {
                [PREC_PLUS] = REL_REDUCE,
                [PREC_MUL] = REL_REDUCE,
                [PREC_DIV] = REL_REDUCE,
                [PREC_MINUS] = REL_REDUCE,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_END] = REL_REDUCE,
                [PREC_EQ] = REL_REDUCE,
                [PREC_NEQ] = REL_REDUCE,
                [PREC_GT] = REL_REDUCE,
                [PREC_GE] = REL_REDUCE,
                [PREC_LT] = REL_REDUCE,
                [PREC_LE] = REL_REDUCE,
            },

        [PREC_END] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_SHIFT,
                [PREC_MUL] = REL_SHIFT,
                [PREC_DIV] = REL_SHIFT,
                [PREC_MINUS] = REL_SHIFT,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_EQ] = REL_SHIFT,
                [PREC_NEQ] = REL_SHIFT,
                [PREC_GT] = REL_SHIFT,
                [PREC_GE] = REL_SHIFT,
                [PREC_LT] = REL_SHIFT,
                [PREC_LE] = REL_SHIFT,
            },

        [PREC_EQ] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_SHIFT,
                [PREC_MUL] = REL_SHIFT,
                [PREC_DIV] = REL_SHIFT,
                [PREC_MINUS] = REL_SHIFT,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_END] = REL_REDUCE,
                [PREC_GT] = REL_SHIFT,
                [PREC_GE] = REL_SHIFT,
                [PREC_LT] = REL_SHIFT,
                [PREC_LE] = REL_SHIFT,
            },

        [PREC_NEQ] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_SHIFT,
                [PREC_MUL] = REL_SHIFT,
                [PREC_DIV] = REL_SHIFT,
                [PREC_MINUS] = REL_SHIFT,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_END] = REL_REDUCE,
                [PREC_GT] = REL_SHIFT,
                [PREC_GE] = REL_SHIFT,
                [PREC_LT] = REL_SHIFT,
                [PREC_LE] = REL_SHIFT,
            },

        [PREC_GT] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_SHIFT,
                [PREC_MUL] = REL_SHIFT,
                [PREC_DIV] = REL_SHIFT,
                [PREC_MINUS] = REL_SHIFT,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_END] = REL_REDUCE,
                [PREC_EQ] = REL_REDUCE,
                [PREC_NEQ] = REL_REDUCE,
            },

        [PREC_GE] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_SHIFT,
                [PREC_MUL] = REL_SHIFT,
                [PREC_DIV] = REL_SHIFT,
                [PREC_MINUS] = REL_SHIFT,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_END] = REL_REDUCE,
                [PREC_EQ] = REL_REDUCE,
                [PREC_NEQ] = REL_REDUCE,
            },

        [PREC_LT] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_SHIFT,
                [PREC_MUL] = REL_SHIFT,
                [PREC_DIV] = REL_SHIFT,
                [PREC_MINUS] = REL_SHIFT,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_END] = REL_REDUCE,
                [PREC_EQ] = REL_REDUCE,
                [PREC_NEQ] = REL_REDUCE,
            },

        [PREC_LE] =
            {
                [PREC_UMINUS] = REL_SHIFT,
                [PREC_PLUS] = REL_SHIFT,
                [PREC_MUL] = REL_SHIFT,
                [PREC_DIV] = REL_SHIFT,
                [PREC_MINUS] = REL_SHIFT,
                [PREC_LPAREN] = REL_SHIFT,
                [PREC_RPAREN] = REL_REDUCE,
                [PREC_OPERAND] = REL_SHIFT,
                [PREC_END] = REL_REDUCE,
                [PREC_EQ] = REL_REDUCE,
                [PREC_NEQ] = REL_REDUCE,
            },
};

PrecedenceRelation getPrecedenceRelation(PrecedenceSymbol stackSymbol,
                                         PrecedenceSymbol inputSymbol) {
    if (stackSymbol >= PREC_TERMINAL_COUNT || inputSymbol >= PREC_TERMINAL_COUNT) {
        return REL_ERROR;
    }

    return precedenceTable[stackSymbol][inputSymbol];
}

static bool isTerminal(PrecedenceSymbol symbol) {
    return symbol != PREC_BOUNDARY && symbol != PREC_NONTERMINAL;
}

int stackTopTerminal(const PrecedenceStack *stack, PrecedenceSymbol *result) {
    if (stack == NULL || result == NULL || stack->size == 0) {
        return EXIT_FAILURE;
    }

    for (size_t i = stack->size; i > 0; i--) {
        if (isTerminal(stack->items[i - 1])) {
            *result = stack->items[i - 1];
            return EXIT_SUCCESS;
        }
    }

    return EXIT_FAILURE;
}

bool precedenceStackIsFinished(const PrecedenceStack *stack) {
    return stack->size == 2 && stack->items[0] == PREC_END && stack->items[1] == PREC_NONTERMINAL;
}

int precedenceStackPushAfterTopTerminal(PrecedenceStack *stack, PrecedenceSymbol symbol) {
    if (stack == NULL) {
        return EXIT_FAILURE;
    }

    size_t index;

    for (index = stack->size; index > 0; index--) {
        if (isTerminal(stack->items[index - 1])) {
            break;
        }
    }

    if (index == 0) {
        return EXIT_FAILURE;
    }

    if (stack->size >= stack->capacity) {
        size_t new_capacity = stack->capacity * 2;
        PrecedenceSymbol *new_items = realloc(stack->items, new_capacity * sizeof *new_items);

        if (new_items == NULL) {
            return EXIT_FAILURE;
        }

        stack->items = new_items;
        stack->capacity = new_capacity;
    }

    memmove(&stack->items[index + 1], &stack->items[index],
            (stack->size - index) * sizeof *stack->items);

    stack->items[index] = symbol;
    stack->size++;

    return EXIT_SUCCESS;
}
