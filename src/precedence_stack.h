#ifndef TOK_STACK_H
#define TOK_STACK_H

#include <lexer.h>
#include <stdbool.h>

typedef enum {
    PREC_END,
    PREC_OPERAND,

    PREC_PLUS,
    PREC_MINUS,
    PREC_UMINUS,

    PREC_MUL,
    PREC_DIV,

    PREC_LT,
    PREC_LE,
    PREC_GT,
    PREC_GE,
    PREC_EQ,
    PREC_NEQ,

    PREC_LPAREN,
    PREC_RPAREN,

    PREC_NONTERMINAL,
    PREC_BOUNDARY,

    PREC_INVALID,
} PrecedenceSymbol;

typedef struct {
    PrecedenceSymbol *items;
    size_t size;
    size_t capacity;
} PrecedenceStack;

int precedenceStackInit(PrecedenceStack *stack);
int precedenceStackPush(PrecedenceStack *stack, PrecedenceSymbol symbol);
int precedenceStackPop(PrecedenceStack *stack, PrecedenceSymbol *result);
int precedenceStackPeek(const PrecedenceStack *stack, PrecedenceSymbol *result);
bool precedenceStackIsEmpty(PrecedenceStack *stack);
void precedenceStackDestroy(PrecedenceStack *stack);
PrecedenceSymbol tokenToPrecSymbol(TokenType kind);

typedef enum {
    REL_ERROR,
    REL_SHIFT,
    REL_REDUCE,
    REL_EQUAL,
} PrecedenceRelation;

PrecedenceRelation getPrecedenceRelation(PrecedenceSymbol stackSymbol,
                                         PrecedenceSymbol inputSymbol);

int stackTopTerminal(const PrecedenceStack *stack, PrecedenceSymbol *result);

bool precedenceStackIsFinished(const PrecedenceStack *stack);
int precedenceStackPushAfterTopTerminal(PrecedenceStack *stack, PrecedenceSymbol symbol);

#endif
