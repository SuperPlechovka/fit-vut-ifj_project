#include "expr_table.h"

#define N_PREC 15

static const char *rows[N_PREC] = {
  /*         +  -  *  / u- < <= > >= == != (  )  i  $ */
  /* +  */ ">><<<>>>>>><><>",
  /* -  */ ">><<<>>>>>><><>",
  /* *  */ ">>>><>>>>>><><>",
  /* /  */ ">>>><>>>>>><><>",
  /* u- */ ">>>><>>>>>><><>",
  /* <  */ "<<<<<......<><>",
  /* <= */ "<<<<<......<><>",
  /* >  */ "<<<<<......<><>",
  /* >= */ "<<<<<......<><>",
  /* == */ "<<<<<......<><>",
  /* != */ "<<<<<......<><>",
  /* (  */ "<<<<<<<<<<<<=<.",
  /* )  */ ">>>>>>>>>>>.>.>",
  /* i  */ ">>>>.>>>>>>.>.>",
  /* $  */ "<<<<<<<<<<<<.<."
};

static int idx(PrecSym s)
{
  if (s == PS_INT || s == PS_DOUBLE || s == PS_STRING)
    return PS_ID;
  return (int)s;
}

Prec precLookup(PrecSym stackTop, PrecSym input)
{
  switch (rows[idx(stackTop)][idx(input)]) {
    case '<': return P_LT;
    case '>': return P_GT;
    case '=': return P_EQ;
    default:  return P_ERR;
  }
}