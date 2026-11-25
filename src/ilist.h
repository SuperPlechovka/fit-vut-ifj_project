#ifndef ILIST_H
#define ILIST_H
/**
 * @file ilist.h
 * @brief Generátor cílového kódu IFJcode26 z abstraktního syntaktického stromu (AST).
 * @details Projekt: Implementace překladače IFJ26 (IFJ/IAL 2026/2027).
 *
 *
 * @author Vojtěch Matěj (xmatejv00)
 */

#include <stdio.h>

typedef enum {
    /* práce s rámci, volání funkcí */
    I_MOVE,
    I_CREATEFRAME,
    I_PUSHFRAME,
    I_POPFRAME,
    I_DEFVAR,
    I_CALL,
    I_RETURN,

    /* práce s datovým zásobníkem */
    I_PUSHS,
    I_POPS,

    /* aritmetické, relační, booleovské a konverzní instrukce */
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

    /* vstupně-výstupní instrukce */
    I_READ,
    I_WRITE,

    /* práce s řetězci */
    I_CONCAT,
    I_STRLEN,
    I_GETCHAR,
    I_SETCHAR,

    /* práce s typy */
    I_TYPE,

    /* řízení toku programu */
    I_LABEL,
    I_JUMP,
    I_JUMPIFEQ,
    I_JUMPIFNEQ,
    I_EXIT,

    /* ladicí instrukce */
    I_BREAK,
    I_DPRINT,

    /* počet typů instrukcí */
    I_COUNT
} InstrType;

typedef enum {
    FR_GF, /* globální rámec */
    FR_LF, /* lokální rámec */
    FR_TF  /* dočasný rámec */
} Frame;

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

/*
 * Operand instrukce cílového kódu IFJcode26.
 *
 * Význam polí podle druhu operandu (kind):
 *   OPD_NONE     nic (nepoužitý operand)
 *   OPD_VAR      frame, name (název proměnné bez prefixu rámce)
 *   OPD_LABEL    name (název návěští)
 *   OPD_TYPE     name (název typu: "int", "float", "string", "bool")
 *   OPD_INT      val.i
 *   OPD_FLOAT    val.d
 *   OPD_STRING   name (text řetězcové konstanty)
 *   OPD_BOOL     val.b (0 = false, nenulová hodnota = true)
 *   OPD_NIL      nic (konstanta nil@nil)
 */
typedef struct {
    OperandKind kind;

    /* paměťový rámec: pouze u OPD_VAR */
    Frame frame;

    /* řetězcová data: název proměnné, návěští, typu nebo text literálu */
    char *name;

    /* číselné a logické hodnoty literálů */
    union {
        long long i; /* hodnota OPD_INT */
        double d;    /* hodnota OPD_FLOAT */
        int b;       /* hodnota OPD_BOOL */
    } val;
} Operand;

/*
 * Jedna instrukce cílového kódu.
 * Nepoužité operandy mají druh OPD_NONE.
 */
typedef struct {
    InstrType type;
    Operand a1, a2, a3;
} Instr;

/* Položka jednosměrně vázaného seznamu instrukcí. */
typedef struct listItem {
    Instr instr;
    struct listItem *next;
} ListItem;

/* Lineární seznam instrukcí s ukazatelem na začátek a konec. */
typedef struct {
    ListItem *first;
    ListItem *last;
} ListOfInstr;

/* správa seznamu instrukcí */
void listInit(ListOfInstr *L);
void listFree(ListOfInstr *L);
void listInsertLast(ListOfInstr *L, Instr I);
void joinLists(ListOfInstr *dest, ListOfInstr *L1, ListOfInstr *L2);

/* konstruktory a kopírování operandů */
Operand opNone(void);
Operand opVar(Frame f, const char *name);
Operand opLabel(const char *name);
Operand opType(const char *name);
Operand opInt(long long v);
Operand opFloat(double v);
Operand opString(const char *s);
Operand opBool(int v);
Operand opNil(void);
Operand opDup(Operand o);

/* generování a výpis instrukcí */
void emit(ListOfInstr *L, InstrType t, Operand a1, Operand a2, Operand a3);
void listPrint(const ListOfInstr *L, FILE *out);

#endif
