#ifndef ILIST_H
#define ILIST_H

#include <stdio.h>

typedef enum {
    I_MOVE,
    I_CREATEFRAME,
    I_PUSHFRAME,
    I_POPFRAME,
    I_DEFVAR,
    I_CALL,
    I_RETURN,
    I_PUSHS,
    I_POPS,
    I_ADD,
    I_SUB,
    I_MUL,
    I_DIV,
    I_IDIV,
    I_LT,
    I_GT,
    I_EQ,
    I_AND,
    I_OR,
    I_NOT,
    I_INT2FLOAT,
    I_FLOAT2INT,
    I_INT2CHAR,
    I_STRI2INT,
    I_INT2STR,
    I_FLOAT2STR,
    I_READ,
    I_WRITE,
    I_CONCAT,
    I_STRLEN,
    I_GETCHAR,
    I_SETCHAR,
    I_TYPE,
    I_LABEL,
    I_JUMP,
    I_JUMPIFEQ,
    I_JUMPIFNEQ,
    I_EXIT,
    I_BREAK,
    I_DPRINT,
    I_COUNT
} InstrType;

typedef enum { FR_GF, FR_LF, FR_TF } Frame;
typedef enum {
    OPD_NONE,
    OPD_VAR,
    OPD_LABEL,
    OPD_TYPE,
    OPD_INT,
    OPD_FLOAT,
    OPD_STRING,
    OPD_BOOL,
    OPD_NIL
} OperandKind;

typedef struct {
    OperandKind kind;
    Frame frame;
    char *name;
    union {
        long long i;
        double d;
        int b;
    } val;
} Operand;

typedef struct {
    InstrType type;
    Operand a1, a2, a3;
} Instr;

typedef struct listItem {
    Instr instr;
    struct listItem *next;
} ListItem;

typedef struct {
    ListItem *first;
    ListItem *last;
} ListOfInstr;

void listInit(ListOfInstr *L);
void listFree(ListOfInstr *L);
void listInsertLast(ListOfInstr *L, Instr I);

Operand opNone(void);
Operand opVar(Frame f, const char *name);
Operand opLabel(const char *name);
Operand opType(const char *name);
Operand opInt(long long v);
Operand opFloat(double v);
Operand opString(const char *s);
Operand opBool(int v);
Operand opNil(void);

void emit(ListOfInstr *L, InstrType t, Operand a1, Operand a2, Operand a3);
void listPrint(const ListOfInstr *L, FILE *out);

Operand opDup(Operand o);
void joinLists(ListOfInstr *dest, ListOfInstr *L1, ListOfInstr *L2);
#endif