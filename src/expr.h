
/**
 * @file expr.h
 * @brief Rozhraní precedenčního parseru pro syntaktickou analýzu výrazů.
 * @details Projekt: Implementace překladače IFJ26 (IFJ/IAL 2026/2027).
 *          Tento soubor poskytuje rozhraní pro zpracování výrazů metodou zdola-nahoru
 *          a jejich převod do abstraktního syntaktického stromu (AST).
 * @author Vojtěch Matěj (xmatejv00)
 */

#ifndef EXPR_H
#define EXPR_H

#include "ast.h"

/**
 * @brief Zpracuje jeden výraz z posloupnosti tokenů na vstupu.
 * @details Používá precedenční syntaktickou analýzu (zdola-nahoru) řízenou precedenční tabulkou.
 *          Při analýze zároveň staví odpovídající podstrom AST. V případě chyby garantuje
 *          kompletní uvolnění rozpracovaných struktur bez úniků paměti.
 *
 * @param[out] out Ukazatel na proměnnou, do které bude uložen kořenový uzel vytvořeného podstromu AST.
 * @return Hodnota 0 při úspěšném zpracování (*out obsahuje platný uzel AST).
 *         Hodnota 2 při syntaktické chybě (*out je nastaven na NULL).
 */
int parse_expression(Node **out);

#endif /* EXPR_H */