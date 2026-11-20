/**
 * @file
 * @brief Rozhraní rozpracovaného lexeru IFJ26.
 * @details Implementace překladače IFJ26 (IFJ/IAL 2026/27).
 * @author Jan Křepinský (xkrepij00@stud.fit.vutbr.cz)
 */
#pragma once

#include <stdio.h>

#include "token.h"

/** @brief Výsledek operace lexeru. */
typedef enum lexer_result { LEXER_OK = 0, LEXER_ERROR = 1, LEXER_INTERNAL_ERROR = 99 } LexerResult;

/** @brief Struktura lexeru s vlastními buffery. */
typedef struct lexer {
    FILE *input;              /**< Vstupní soubor, lexer jej nezavírá. */
    size_t line;              /**< Aktuální řádek od 1. */
    size_t column;            /**< Aktuální sloupec od 1. */
    char *lexem;              /**< Vlastněný buffer lexému. */
    size_t lexem_length;      /**< Délka lexému bez ukončovací nuly. */
    size_t lexem_capacity;    /**< Kapacita bufferu lexému v bajtech. */
    size_t *indents;          /**< Vlastněný zásobník úrovní odsazení. */
    size_t indent_depth;      /**< Počet úrovní v zásobníku. */
    size_t indent_capacity;   /**< Kapacita zásobníku odsazení. */
    size_t pending_dedents;   /**< Počet čekajících tokenů DEDENT. */
    int at_line_start;        /**< Příznak začátku řádku. */
    int eof_emitted;          /**< Příznak vydaného tokenu EOF. */
    size_t parenthesis_depth; /**< Hloubka závorek pro potlačení nových řádků a odsazení. */
} Lexer;

/** @brief Inicializuje lexer a jeho buffery.
 * @param[in,out] lexer Vynulovaný lexer.
 * @param[in] input Vstupní soubor.
 * @retval LEXER_OK Lexer byl inicializován.
 * @retval LEXER_INTERNAL_ERROR Neplatný vstup, již inicializovaný lexer nebo selhání alokace.
 */
LexerResult lexer_init(Lexer *lexer, FILE *input);

/** @brief Uvolní buffery a vynuluje lexer.
 * @param[in,out] lexer Inicializovaný nebo vynulovaný lexer, případně NULL.
 */
void lexer_destroy(Lexer *lexer);

/** @brief Načte další token ze vstupu.
 * @param[in,out] lexer Inicializovaný lexer.
 * @param[out] token Načtený token; vlastněný text uvolňuje volající přes token_free.
 * @retval LEXER_OK Token byl načten, včetně TOK_EOF na konci vstupu.
 * @retval LEXER_ERROR Lexikální chyba.
 * @retval LEXER_INTERNAL_ERROR Neplatný vstup nebo interní chyba.
 */
LexerResult next_token(Lexer *lexer, Token *token);
