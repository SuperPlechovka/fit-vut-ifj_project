#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "expr.h"
#include "expr_table.h"

#define STACK_MAX 256

extern PrecSym lexer_to_sym(void);
extern char *lexer_lexeme(void);

typedef struct {
  PrecSym sym;      /* terminál */
  int     is_E;     /* 1 = hotový podvýraz */
  int     handle;   /* 1 = značka začátku pivotu */
  char   *lex;      /* text tokenu operandu, jinak NULL */
  Node   *node;     /* strom podvýrazu */
} Item;

static Item st[STACK_MAX];
static int  sp;

static char   *in_lex;
static PrecSym prev;

static int is_operand(PrecSym s)
{
  return s == PS_ID || s == PS_INT || s == PS_DOUBLE || s == PS_STRING;
}

static int is_binop(PrecSym s)
{
  return s >= PS_PLUS && s <= PS_NE && s != PS_UMINUS;
}

static void push(Item it)
{
  if (sp >= STACK_MAX) {
    fprintf(stderr, "Interní chyba: přetečení zásobníku výrazů\n");
    exit(99);
  }
  st[sp++] = it;
}

static void clear_stack(void)
{
  for (int i = 0; i < sp; i++) {
    if (st[i].is_E)
      nodeFree(st[i].node);
    else
      free(st[i].lex);
  }
  sp = 0;
}

/* Index nejvyššího terminálu na zásobníku */
static int top_terminal(void)
{
  for (int i = sp - 1; i >= 0; i--)
    if (!st[i].is_E && !st[i].handle)
      return i;
  return 0;
}

/* Vloží značku bezprostředně nad terminál na indexu t. */
static void insert_handle(int t)
{
  if (sp >= STACK_MAX) {
    fprintf(stderr, "Interní chyba: přetečení zásobníku výrazů\n");
    exit(99);
  }
  memmove(&st[t + 2], &st[t + 1], (size_t)(sp - t - 1) * sizeof(Item));
  st[t + 1] = (Item){ .handle = 1 };
  sp++;
}

static OpKind toOp(PrecSym s)
{
  switch (s) {
    case PS_PLUS:
      return OP_ADD;
    case PS_MINUS:
      return OP_SUB;
    case PS_MUL:
      return OP_MUL;
    case PS_DIV:
      return OP_DIV;
    case PS_LT:
      return OP_LT;
    case PS_LE:
      return OP_LE;
    case PS_GT:
      return OP_GT;
    case PS_GE:
      return OP_GE;
    case PS_EQ:
      return OP_EQ;
    case PS_NE:
      return OP_NE;
    default:
      return OP_NONE;
  }
}

/* Načte další symbol. Tady se rozhoduje mezi binárním a unárním minus. */
static PrecSym next_sym(void)
{
  PrecSym s = lexer_to_sym();

  if (s == PS_MINUS && !is_operand(prev) && prev != PS_RPAR) {
    s = PS_UMINUS;
  }

  prev = s;
  in_lex = is_operand(s) ? lexer_lexeme() : NULL;
  return s;
}

/**Uvolní všechno rozpracované a vrátí kód syntaktické chyby. */
static int fail(Node **out)
{
  clear_stack();
  free(in_lex);
  in_lex = NULL;
  *out = NULL;
  return 2;
}

/** Zredukuje pivot nad nejvyšší značkou. Vrací 1 (úspěch) nebo 0 (chyba). */
static int reduce(void)
{
  int h = sp - 1;
  while (h >= 0 && !st[h].handle)
    h--;
  if (h < 0)
    return 0;

  int n = sp - h - 1;
  Item *a = &st[h + 1];
  Node *res = NULL;

  if (n == 1 && !a[0].is_E && is_operand(a[0].sym)) {
    /* E -> operand */
    switch (a[0].sym) {
      case PS_INT:
        res = nodeLitInt(strtoll(a[0].lex, NULL, 10));
        break;
      case PS_DOUBLE:
        res = nodeLitDouble(strtod(a[0].lex, NULL));
        break;
      case PS_STRING:
        res = nodeLitString(a[0].lex);
        break;
      case PS_ID:
        res = nodeVar(a[0].lex);
        break;
      default:
        break;
    }
    free(a[0].lex);
    a[0].lex = NULL;
  } else if (n == 2 && !a[0].is_E && a[0].sym == PS_UMINUS && a[1].is_E) {
    /* E -> - E */
    res = nodeUnary(OP_NEG, a[1].node);
    a[1].node = NULL;
  } else if (n == 3 && a[0].is_E && !a[1].is_E && is_binop(a[1].sym) && a[2].is_E) {
    /* E -> E op E */
    res = nodeBinary(toOp(a[1].sym), a[0].node, a[2].node);
    a[0].node = NULL;
    a[2].node = NULL;
  } else if (n == 3 && !a[0].is_E && a[0].sym == PS_LPAR && a[1].is_E && !a[2].is_E && a[2].sym == PS_RPAR) {
    /* E -> ( E ) */
    res = a[1].node;
    a[1].node = NULL;
  } else {
    return 0;
  }

  sp = h;
  push((Item){ .is_E = 1, .node = res });
  return 1;
}

int parse_expression(Node **out)
{
  *out = NULL;
  sp = 0;
  prev = PS_END;
  push((Item){ .sym = PS_END });
  PrecSym in = next_sym();

  for (;;) {
    int t = top_terminal();
    Prec p = precLookup(st[t].sym, in);

    switch (p) {
      case P_LT:
        insert_handle(t);
        push((Item){ .sym = in, .lex = in_lex });
        in_lex = NULL;
        in = next_sym();
        break;

      case P_EQ:
        push((Item){ .sym = in, .lex = in_lex });
        in_lex = NULL;
        in = next_sym();
        break;

      case P_GT:
        if (!reduce()) {
          return fail(out);
        }
        break;

      default:
        /* Konec výrazu / Chyba */
        if (in == PS_END && sp == 2 && st[1].is_E) {
          *out = st[1].node;
          st[1].node = NULL;
          clear_stack();
          return 0;
        }
        return fail(out);
    }
  }
}