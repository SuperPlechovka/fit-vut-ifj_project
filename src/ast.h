#ifndef AST_H
#define AST_H

#include <stddef.h>
/*AI:make nice coments in czech for this head file*/
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
  /* celý program */
  N_PROGRAM,
  N_FUNC
} NodeKind;

typedef enum {
  OP_NONE,
  OP_ADD, OP_SUB, OP_MUL, OP_DIV,
  OP_LT, OP_LE, OP_GT, OP_GE, OP_EQ, OP_NE,
  OP_NEG
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
 *   N_FUNC                    name, is_main, type (návratový typ),
 *                             body (seznam příkazů přes next)
 *   N_PROGRAM                 body (seznam funkcí přes next)
 *
 * Pole next slouží jen k řetězení sourozenců v jednom seznamu
 * (příkazy, argumenty, funkce). Nikdy neukazuje na jiný podstrom.
 *
 * Uzel vlastní všechny ukazatele, které drží (left, right, args,
 * body, next). Uvolní je nodeFree.
 */
typedef struct Node {
  NodeKind  kind;

  /* operátor: u N_BINARY a N_UNARY, jinak OP_NONE */
  OpKind    op;

  /* typ: u výrazů výsledný typ, u N_DECL deklarovaný typ,
   * u N_FUNC návratový typ. Hodnotu nastavuje sémantická analýza
   * (TY_UNKNOWN, dokud nedoběhne). */
  TypeKind  type;

  /* hodnoty literálů a jména */
  char     *name;       /* identifikátor, text literálu, jméno funkce */
  long long ival;       /* hodnota N_LIT_INT */
  double    dval;       /* hodnota N_LIT_DOUBLE */

  /* příznaky */
  int       is_var;     /* N_DECL: 1 = var (modifikovatelná), 0 = val */
  int       is_main;    /* N_FUNC: 1 = funkce s anotací @main */
  /* poloha ve zdrojovém souboru, z tokenu, ze kterého uzel vznikl */
  int       line;       /* řádek, od 1 */
  int       col;        /* sloupec, od 1 */
  /* potomci */
  struct Node *left;    /* první operand, výraz, inicializace nebo NULL */
  struct Node *right;   /* druhý operand (jen N_BINARY) */
  struct Node *args;    /* argumenty volání (N_CALL) */
  struct Node *body;    /* příkazy (N_FUNC) nebo funkce (N_PROGRAM) */

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

Node *nodeFunc(const char *name, int is_main, TypeKind ret, Node *body);
Node *nodeProgram(Node *funcs);
void nodeSetPos(Node *n, int line, int col);

#endif