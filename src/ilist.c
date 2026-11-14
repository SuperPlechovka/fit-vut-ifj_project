#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ilist.h"

static void *xmalloc(size_t n)
{
  void *p = malloc(n);
  if (p == NULL) {
    fprintf(stderr, "Interní chyba: nedostatek paměti\n");
    exit(99);
  }
  return p;
}

static char *xstrdup(const char *s)
{
  size_t n = strlen(s) + 1;
  char *p = xmalloc(n);
  memcpy(p, s, n);
  return p;
}

void listInit(ListOfInstr *L)
{
  L->first = NULL;
  L->last = NULL;
}

void listFree(ListOfInstr *L)
{
  ListItem *it = L->first;
  while (it != NULL) {
    ListItem *next = it->next;
    free(it->instr.a1.name);
    free(it->instr.a2.name);
    free(it->instr.a3.name);
    free(it);
    it = next;
  }
  L->first = NULL;
  L->last = NULL;
}

void listInsertLast(ListOfInstr *L, Instr I)
{
  ListItem *item = xmalloc(sizeof(ListItem));
  item->instr = I;
  item->next = NULL;
  if (L->last == NULL)
    L->first = item;
  else
    L->last->next = item;
  L->last = item;
}

Operand opNone(void)
{
  Operand o = { OPD_NONE, FR_GF, NULL, { 0 } };
  return o;
}

Operand opVar(Frame f, const char *name)
{
  Operand o = opNone();
  o.kind = OPD_VAR;
  o.frame = f;
  o.name = xstrdup(name);
  return o;
}

Operand opLabel(const char *name)
{
  Operand o = opNone();
  o.kind = OPD_LABEL;
  o.name = xstrdup(name);
  return o;
}

Operand opType(const char *name)
{
  Operand o = opNone();
  o.kind = OPD_TYPE;
  o.name = xstrdup(name);
  return o;
}

Operand opInt(long long v)
{
  Operand o = opNone();
  o.kind = OPD_INT;
  o.val.i = v;
  return o;
}

Operand opFloat(double v)
{
  Operand o = opNone();
  o.kind = OPD_FLOAT;
  o.val.d = v;
  return o;
}

Operand opString(const char *s)
{
  Operand o = opNone();
  o.kind = OPD_STRING;
  o.name = xstrdup(s);
  return o;
}

Operand opBool(int v)
{
  Operand o = opNone();
  o.kind = OPD_BOOL;
  o.val.b = v ? 1 : 0;
  return o;
}

Operand opNil(void)
{
  Operand o = opNone();
  o.kind = OPD_NIL;
  return o;
}

void emit(ListOfInstr *L, InstrType t, Operand a1, Operand a2, Operand a3)
{
  Instr I;
  I.type = t;
  I.a1 = a1;
  I.a2 = a2;
  I.a3 = a3;
  listInsertLast(L, I);
}

static const char *instrNames[I_COUNT] = {
  [I_MOVE] = "MOVE", [I_CREATEFRAME] = "CREATEFRAME", [I_PUSHFRAME] = "PUSHFRAME",
  [I_POPFRAME] = "POPFRAME", [I_DEFVAR] = "DEFVAR", [I_CALL] = "CALL",
  [I_RETURN] = "RETURN", [I_PUSHS] = "PUSHS", [I_POPS] = "POPS",
  [I_ADD] = "ADD", [I_SUB] = "SUB", [I_MUL] = "MUL", [I_DIV] = "DIV",
  [I_IDIV] = "IDIV", [I_LT] = "LT", [I_GT] = "GT", [I_EQ] = "EQ",
  [I_AND] = "AND", [I_OR] = "OR", [I_NOT] = "NOT",
  [I_INT2FLOAT] = "INT2FLOAT", [I_FLOAT2INT] = "FLOAT2INT",
  [I_INT2CHAR] = "INT2CHAR", [I_STRI2INT] = "STRI2INT",
  [I_INT2STR] = "INT2STR", [I_FLOAT2STR] = "FLOAT2STR",
  [I_READ] = "READ", [I_WRITE] = "WRITE", [I_CONCAT] = "CONCAT",
  [I_STRLEN] = "STRLEN", [I_GETCHAR] = "GETCHAR", [I_SETCHAR] = "SETCHAR",
  [I_TYPE] = "TYPE", [I_LABEL] = "LABEL", [I_JUMP] = "JUMP",
  [I_JUMPIFEQ] = "JUMPIFEQ", [I_JUMPIFNEQ] = "JUMPIFNEQ", [I_EXIT] = "EXIT",
  [I_BREAK] = "BREAK", [I_DPRINT] = "DPRINT",
};

static void printEscaped(FILE *out, const char *s)
{
  for (; *s != '\0'; s++) {
    unsigned char c = (unsigned char)*s;
    if (c <= 32 || c == 35 || c == 92)
      fprintf(out, "\\%03d", c);
    else
      fputc(c, out);
  }
}

static void printOperand(FILE *out, const Operand *o)
{
  static const char *frames[] = { "GF", "LF", "TF" };
  switch (o->kind) {
    case OPD_VAR:    fprintf(out, "%s@%s", frames[o->frame], o->name); break;
    case OPD_LABEL:  fputs(o->name, out); break;
    case OPD_TYPE:   fputs(o->name, out); break;
    case OPD_INT:    fprintf(out, "int@%lld", o->val.i); break;
    case OPD_FLOAT:  fprintf(out, "float@%a", o->val.d); break;
    case OPD_STRING: fputs("string@", out); printEscaped(out, o->name); break;
    case OPD_BOOL:   fprintf(out, "bool@%s", o->val.b ? "true" : "false"); break;
    case OPD_NIL:    fputs("nil@nil", out); break;
    case OPD_NONE:   break;
  }
}

void listPrint(const ListOfInstr *L, FILE *out)
{
  const ListItem *it;
  for (it = L->first; it != NULL; it = it->next) {
    const Operand *ops[3] = { &it->instr.a1, &it->instr.a2, &it->instr.a3 };
    fputs(instrNames[it->instr.type], out);
    for (int i = 0; i < 3; i++) {
      if (ops[i]->kind == OPD_NONE)
        break;
      fputc(' ', out);
      printOperand(out, ops[i]);
    }
    fputc('\n', out);
  }
}
Operand opDup(Operand o)
{
  if (o.name != NULL) {
    o.name = xstrdup(o.name);
  }
  return o;
}
void joinLists(ListOfInstr *dest, ListOfInstr *L1, ListOfInstr *L2)
{
  listInit(dest);
  if (L1->first == NULL) {
    *dest = *L2;
  } else if (L2->first == NULL) {
    *dest = *L1;
  } else {
    dest->first = L1->first;
    L1->last->next = L2->first;
    dest->last = L2->last;
  }
  /* Vynulování původních seznamů pro zamezení double-free */
  L1->first = L1->last = NULL;
  L2->first = L2->last = NULL;
}
