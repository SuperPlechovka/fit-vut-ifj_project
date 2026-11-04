/**
 * @file ast.h
 * @brief Definice datových struktur a rozhraní abstraktního syntaktického stromu (AST).
 * @details Projekt: Implementace překladače IFJ26 (IFJ/IAL 2026/2027).
 *          Tento soubor obsahuje definice uzlů, typů, operátorů a konstruktorů
 *          pro stavbu a správu syntaktického stromu.
 * @author Vojtěch Matěj (xmatejv00)
 */

#ifndef AST_H
#define AST_H

#include <stddef.h>

/**
 * @brief Druhy uzlů abstraktního syntaktického stromu.
 */
typedef enum {
  /* Výrazy a literály */
  N_LIT_INT,    /**< Celočíselný literál (long long) */
  N_LIT_DOUBLE, /**< Desetinný literál (double) */
  N_LIT_STRING, /**< Řetězcový literál (string) */
  N_VAR,        /**< Identifikátor / odkaz na proměnnou */
  N_BINARY,     /**< Binární operace (např. +, -, *, /) */
  N_UNARY,      /**< Unární operace (např. -x) */
  N_CALL,       /**< Volání funkce */

  /* Příkazy */
  N_DECL,       /**< Deklarace proměnné (var / val) */
  N_ASSIGN,     /**< Přiřazení do proměnné */
  N_WRITE,      /**< Vestavěný příkaz pro výpis */
  N_RETURN,     /**< Návrat z funkce (return) */

  /* Struktura programu */
  N_PROGRAM,    /**< Kořenový uzel celého programu */
  N_FUNC        /**< Definice funkce */
} NodeKind;

/**
 * @brief Druhy operátorů v binárních a unárních výrazech.
 */
typedef enum {
  OP_NONE,      /**< Žádný operátor */
  OP_ADD,       /**< Sčítání (+) */
  OP_SUB,       /**< Odčítání (-) */
  OP_MUL,       /**< Násobení (*) */
  OP_DIV,       /**< Dělení (/) */
  OP_LT,        /**< Menší než (<) */
  OP_LE,        /**< Menší nebo rovno (<=) */
  OP_GT,        /**< Větší než (>) */
  OP_GE,        /**< Větší nebo rovno (>=) */
  OP_EQ,        /**< Rovnost (==) */
  OP_NE,        /**< Nerovnost (!=) */
  OP_NEG        /**< Unární mínus (-x) */
} OpKind;

/**
 * @brief Datové typy v jazyce IFJ26.
 */
typedef enum {
  TY_UNKNOWN,   /**< Neznámý / dosud nezjištěný typ */
  TY_INT,       /**< Celé číslo (Int) */
  TY_DOUBLE,    /**< Desetinné číslo (Float) */
  TY_STRING,    /**< Řetězec (String) */
  TY_UNIT,      /**< Prázdný typ / bez hodnoty (Void/Unit) */
  TY_BOOL       /**< Pravdivostní hodnota (Bool) */
} TypeKind;

/**
 * @brief Struktura reprezentující uzel abstraktního syntaktického stromu.
 *
 * Význam jednotlivých polí podle druhu uzlu (kind):
 *   - N_LIT_INT, N_LIT_DOUBLE : Hodnota uložena v ival / dval.
 *   - N_LIT_STRING            : Text literálu bez uvozovek v name.
 *   - N_VAR                   : Název proměnné v name.
 *   - N_BINARY                : Operátor v op, levý a pravý operand v left, right.
 *   - N_UNARY                 : Operátor op (OP_NEG), operand v left.
 *   - N_CALL                  : Název funkce v name, seznam argumentů navázaný v args přes next.
 *   - N_DECL                  : Název proměnné v name, příznak is_var, deklarovaný typ v type,
 *                               případná inicializace v left (nebo NULL).
 *   - N_ASSIGN                : Název cílové proměnné v name, hodnota v left.
 *   - N_WRITE                 : Vytiskávaný výraz v left.
 *   - N_RETURN                : Vracený výraz v left (nebo NULL).
 *   - N_FUNC                  : Název v name, příznak is_main, návratový typ v type,
 *                               seznam příkazů navázaný v body přes next.
 *   - N_PROGRAM               : Seznam funkcí navázaný v body přes next.
 *
 * @note Pole next slouží výhradně k zřetězení sourozenců v jednom seznamu 
 *       (např. příkazy v bloku, argumenty volání nebo funkce v programu). 
 *       Nikdy neukazuje na samostatný podstrom.
 *
 * @warning Uzel plně vlastní všechny své alokované ukazatele (left, right, args, body, next, name).
 *          Při volání nodeFree dochází k jejich rekurzivnímu uvolnění.
 */
typedef struct Node {
  NodeKind  kind;       /**< Druh syntaktického uzlu */

  OpKind    op;         /**< Operátor (pro N_BINARY a N_UNARY, jinak OP_NONE) */

  TypeKind  type;       /**< Výsledný typ výrazu / deklarovaný typ / návratový typ funkce */

  char     *name;       /**< Identifikátor proměnné/funkce nebo text literálu */
  long long ival;       /**< Celočíselná hodnota pro N_LIT_INT */
  double    dval;       /**< Desetinná hodnota pro N_LIT_DOUBLE */

  int       is_var;     /**< Příznak modifikovatelnosti u N_DECL (1 = var, 0 = val) */
  int       is_main;    /**< Příznak hlavní funkce u N_FUNC (1 = anotace @main) */

  int       line;       /**< Číslo řádku ve zdrojovém souboru (od 1) */
  int       col;        /**< Číslo sloupce ve zdrojovém souboru (od 1) */

  struct Node *left;    /**< První operand, výraz, inicializace nebo NULL */
  struct Node *right;   /**< Druhý operand (pouze pro N_BINARY) */
  struct Node *args;    /**< Seznam argumentů při volání funkce (N_CALL) */
  struct Node *body;    /**< Tělo funkce (N_FUNC) nebo seznam funkcí (N_PROGRAM) */

  struct Node *next;    /**< Ukazatel na následujícího sourozence v seznamu */
} Node;

/* --- Základní správci paměti uzlů --- */

/**
 * @brief Alokuje a inicializuje nový uzel zadaného druhu.
 * @param kind Druh vytvářeného uzlu.
 * @return Ukazatel na nový uzel nebo NULL při selhání alokace.
 */
Node *nodeNew(NodeKind kind);

/**
 * @brief Rekurzivně uvolní uzel, všechny jeho podstromy i navázané sourozence.
 * @param n Ukazatel na uzel k uvolnění (bezpečně zpracuje i NULL).
 */
void  nodeFree(Node *n);

/* --- Konstruktory pro výrazy a literály --- */

/**
 * @brief Vytvoří uzel pro celočíselný literál.
 * @param v Celá hodnota.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeLitInt(long long v);

/**
 * @brief Vytvoří uzel pro desetinný literál.
 * @param v Desetinná hodnota.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeLitDouble(double v);

/**
 * @brief Vytvoří uzel pro řetězcový literál.
 * @param s Řetězec literálu.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeLitString(const char *s);

/**
 * @brief Vytvoří uzel pro odkaz na proměnnou.
 * @param name Název proměnné.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeVar(const char *name);

/**
 * @brief Vytvoří uzel pro binární operaci.
 * @param op Druh binárního operátoru.
 * @param l Levý operand.
 * @param r Pravý operand.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeBinary(OpKind op, Node *l, Node *r);

/**
 * @brief Vytvoří uzel pro unární operaci.
 * @param op Druh unárního operátoru.
 * @param operand Vstupní operand.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeUnary(OpKind op, Node *operand);

/**
 * @brief Vytvoří uzel pro volání funkce.
 * @param name Název volané funkce.
 * @param args Seznam argumentů.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeCall(const char *name, Node *args);

/* --- Konstruktory pro příkazy --- */

/**
 * @brief Vytvoří uzel pro deklaraci proměnné.
 * @param name Název proměnné.
 * @param is_var Modifikovatelnost (1 = var, 0 = val).
 * @param declared Deklarovaný datový typ.
 * @param init Inicializační výraz nebo NULL.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeDecl(const char *name, int is_var, TypeKind declared, Node *init);

/**
 * @brief Vytvoří uzel pro přiřazení do proměnné.
 * @param name Název cílové proměnné.
 * @param expr Přiřazovaný výraz.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeAssign(const char *name, Node *expr);

/**
 * @brief Vytvoří uzel pro vestavěný příkaz výpisu (write).
 * @param expr Výraz k vytištění.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeWrite(Node *expr);

/**
 * @brief Vytvoří uzel pro návrat z funkce.
 * @param expr Návratový výraz nebo NULL.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeReturn(Node *expr);

/* --- Konstruktory pro strukturu programu --- */

/**
 * @brief Vytvoří uzel pro definici funkce.
 * @param name Název funkce.
 * @param is_main Příznak hlavního vstupu programu (1 = @main, 0 = běžná funkce).
 * @param ret Návratový typ funkce.
 * @param body Seznam příkazů tvořících tělo funkce.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeFunc(const char *name, int is_main, TypeKind ret, Node *body);

/**
 * @brief Vytvoří kořenový uzel programu.
 * @param funcs Seznam definic funkcí tvořících program.
 * @return Ukazatel na vytvořený uzel.
 */
Node *nodeProgram(Node *funcs);

/**
 * @brief Nastaví pozici uzlu ve zdrojovém kódu.
 * @param n Ukazatel na uzel.
 * @param line Číslo řádku.
 * @param col Číslo sloupce.
 */
void nodeSetPos(Node *n, int line, int col);

#endif /* AST_H */