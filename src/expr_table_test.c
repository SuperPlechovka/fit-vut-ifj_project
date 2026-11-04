#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "expr_table.c"

int main(void)
{
  for (int i = 0; i < N_PREC; i++)
    assert(strlen(rows[i]) == N_PREC);

  assert(precLookup(PS_PLUS, PS_PLUS) == P_GT);
  assert(precLookup(PS_PLUS, PS_MUL) == P_LT);
  assert(precLookup(PS_MUL, PS_PLUS) == P_GT);
  assert(precLookup(PS_UMINUS, PS_UMINUS) == P_LT);
  assert(precLookup(PS_END, PS_LPAR) == P_LT);
  assert(precLookup(PS_LPAR, PS_RPAR) == P_EQ);
  assert(precLookup(PS_LPAR, PS_END) == P_ERR);
  assert(precLookup(PS_ID, PS_RPAR) == P_GT);
  assert(precLookup(PS_ID, PS_LPAR) == P_ERR);
  assert(precLookup(PS_ID, PS_NE) == P_GT);
  assert(precLookup(PS_LT, PS_LT) == P_ERR);
  assert(precLookup(PS_LT, PS_EQ) == P_ERR);
  assert(precLookup(PS_END, PS_END) == P_ERR);
  assert(precLookup(PS_PLUS, PS_INT) == P_LT);

  puts("tabulka ok");
  return 0;
}