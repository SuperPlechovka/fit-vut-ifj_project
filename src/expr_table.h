#ifndef EXPR_TABLE_H
#define EXPR_TABLE_H

typedef enum {
  PS_PLUS, PS_MINUS, PS_MUL, PS_DIV, PS_UMINUS,
  PS_LT, PS_LE, PS_GT, PS_GE, PS_EQ, PS_NE,
  PS_LPAR, PS_RPAR, PS_ID, PS_END,
  PS_INT, PS_DOUBLE, PS_STRING
} PrecSym;

typedef enum { P_ERR, P_LT, P_EQ, P_GT } Prec;

Prec precLookup(PrecSym stackTop, PrecSym input);

#endif