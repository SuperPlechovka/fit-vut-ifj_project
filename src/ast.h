#ifndef AST_H
#define AST_H

#include <stddef.h>

typedef enum {
  /* výrazy */
  N_LIT_INT,
  N_LIT_DOUBLE,
  N_LIT_STRING,
  N_VAR,
  N_BINARY,
  N_UNARY,
  N_CALL,
  /* příkazy */
  N_DECL,
  N_ASSIGN,
  N_WRITE,
  N_RETURN,
  N_IF,
  N_WHILE,
  /* celý program */
  N_PROGRAM,
  N_FUNC
} NodeKind;

typedef enum {
  OP_NONE,
  OP_ADD, OP_SUB, OP_MUL, OP_DIV,
  OP_LT, OP_LE, OP_GT, OP_GE, OP_EQ, OP_NE,
  OP_NEG,
  OP_AND, OP_OR
} OpKind;

typedef enum {
  TY_UNKNOWN,
  TY_INT,
  TY_DOUBLE,
  TY_STRING,
  TY_UNIT,
  TY_BOOL
} TypeKind;

/*
 * Uzel abstraktního syntaktického stromu.
 *
 * Význam polí podle druhu uzlu (kind):
 *   N_LIT_INT, N_LIT_DOUBLE   ival / dval
 *   N_LIT_STRING              name (text literálu bez uvozovek)
 *   N_VAR                     name
 *   N_BINARY                  op, left, right
 *   N_UNARY                   op (OP_NEG), left
 *   N_CALL                    name, args (seznam argumentů přes next)
 *   N_DECL                    name, is_var, type (deklarovaný typ),
 *                             left (inicializace, nebo NULL)
 *   N_ASSIGN                  name, left
 *   N_WRITE                   left
 *   N_RETURN                  left (výraz, nebo NULL)
 *   N_IF                      left (podmínka), body (THEN větev), right (ELSE větev nebo NULL)
 *   N_WHILE                   left (podmínka), body (tělo cyklu)
 *   N_FUNC                    name, is_main, type (návratový typ),
 *                             args (seznam parametrů přes next),
 *                             body (seznam příkazů přes next)
 *   N_PROGRAM                 body (seznam funkcí přes next)
 *
 * Pole next slouží k řetězení sourozenců v jednořetězcovém seznamu
 * (příkazy, argumenty, parametry, funkce).
 */
typedef struct Node {
  NodeKind  kind;

  /* operátor: u N_BINARY a N_UNARY, jinak OP_NONE */
  OpKind    op;

  /* typ: u výrazů výsledný typ, u N_DECL deklarovaný typ, u N_FUNC návratový typ */
  TypeKind  type;

  /* hodnoty literálů a jména */
  char     *name;       /* identifikátor, text literálu, jméno funkce */
  long long ival;       /* hodnota N_LIT_INT */
  double    dval;       /* hodnota N_LIT_DOUBLE */

  /* příznaky */
  int       is_var;     /* N_DECL: 1 = var, 0 = val */
  int       is_main;    /* N_FUNC: 1 = funkce s anotací @main */

  /* poloha ve zdrojovém souboru */
  int       line;       /* řádek, od 1 */
  int       col;        /* sloupec, od 1 */

  /* potomci */
  struct Node *left;    /* první operand, výraz, inicializace nebo podmínka */
  struct Node *right;   /* druhý operand (N_BINARY) nebo ELSE větev (N_IF) */
  struct Node *args;    /* argumenty volání (N_CALL) nebo parametry (N_FUNC) */
  struct Node *body;    /* příkazy (N_FUNC, THEN větev u N_IF, tělo N_WHILE) nebo funkce (N_PROGRAM) */

  /* sourozenec v seznamu příkazů, argumentů nebo funkcí */
  struct Node *next;
} Node;

Node *nodeNew(NodeKind kind);
void  nodeFree(Node *n);

Node *nodeLitInt(long long v);
Node *nodeLitDouble(double v);
Node *nodeLitString(const char *s);
Node *nodeVar(const char *name);
Node *nodeBinary(OpKind op, Node *l, Node *r);
Node *nodeUnary(OpKind op, Node *operand);
Node *nodeCall(const char *name, Node *args);

Node *nodeDecl(const char *name, int is_var, TypeKind declared, Node *init);
Node *nodeAssign(const char *name, Node *expr);
Node *nodeWrite(Node *expr);
Node *nodeReturn(Node *expr);

Node *nodeIf(Node *cond, Node *then_body, Node *else_body);
Node *nodeWhile(Node *cond, Node *body);

Node *nodeFunc(const char *name, int is_main, TypeKind ret, Node *body);
Node *nodeProgram(Node *funcs);

void nodeSetPos(Node *n, int line, int col);
void nodeAppend(Node **list, Node *node);

#endif