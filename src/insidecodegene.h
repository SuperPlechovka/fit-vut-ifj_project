/**
 * @file codegen.h
 * @brief Generátor cílového kódu IFJcode26 z abstraktního syntaktického stromu (AST).
 * @details Projekt: Implementace překladače IFJ26 (IFJ/IAL 2026/2027).
 *          Tento soubor poskytuje rozhraní pro překlad celého programu reprezentovaného
 *          uzly AST do posloupnosti instrukcí tříadresného kódu IFJcode26.
 * @author Vojtěch Matěj (xmatejv00)
 */

#ifndef INSIDECODEGENE_H
#define INSIDECODEGENE_H

#include "ast.h"
#include "ilist.h"

/**
 * @brief Vygeneruje posloupnost instrukcí IFJcode26 pro celý program.
 * @details Prochází kořenový uzel AST (N_PROGRAM) a všechny jeho podstromy (funkce,
 *          deklarace, příkazy, výrazy). Generované instrukce postupně zapisuje
 *          do předaného instrukčního seznamu.
 *
 * @param prog Ukazatel na kořenový uzel AST reprezentující celý program.
 * @param[out] out Ukazatel na seznam instrukcí, do kterého bude vygenerovaný kód uložen.
 * @return Nula (0) při úspěšném vygenerování kódu, jinak nenulový chybový kód.
 */
int codegenProgram(const Node *prog, ListOfInstr *out);

#endif /* INSIDECODEGENE_H */
