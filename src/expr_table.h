/**
 * @file expr_table.h
 * @brief Rozhraní precedenční tabulky pro syntaktickou analýzu výrazů.
 * @details Projekt: Implementace překladače IFJ26 (IFJ/IAL 2026/2027).
 *          Tento soubor definuje symboly precedenční analýzy, relace priority
 *          operátorů a funkci pro vyhledávání v precedenční tabulce.
 * @author Vojtěch Matěj (xmatejv00)
 */

#ifndef EXPR_TABLE_H
#define EXPR_TABLE_H

/**
 * @brief Symboly používané v precedenční syntaktické analýze výrazů.
 */
typedef enum {
  /* Aritmetické operátory */
  PS_PLUS,    /**< Sčítání (+) */
  PS_MINUS,   /**< Odčítání (-) */
  PS_MUL,     /**< Násobení (*) */
  PS_DIV,     /**< Dělení (/) */
  PS_UMINUS,  /**< Unární mínus (u-) */

  /* Relační operátory */
  PS_LT,      /**< Menší než (<) */
  PS_LE,      /**< Menší nebo rovno (<=) */
  PS_GT,      /**< Větší než (>) */
  PS_GE,      /**< Větší nebo rovno (>=) */
  PS_EQ,      /**< Rovnost (==) */
  PS_NE,      /**< Nerovnost (!=) */

  /* Závorky a ukonečení */
  PS_LPAR,    /**< Levá závorka (() */
  PS_RPAR,    /**< Pravá závorka ()) */
  PS_ID,      /**< Identifikátor proměnné / obecný operand */
  PS_END,     /**< Koncový symbol / Dolar ($) */

  /* Literály */
  PS_INT,     /**< Celočíselný literál */
  PS_DOUBLE,  /**< Desetinný literál */
  PS_STRING   /**< Řetězcový literál */
} PrecSym;

/**
 * @brief Precedenční relace určující akci parseru.
 */
typedef enum {
  P_ERR,  /**< Chyba (prázdné místo v tabulce) – neplatná kombinace symbolů */
  P_LT,   /**< Posun (<) – vložení značky pivotu a načtení nového tokenu */
  P_EQ,   /**< Sloučení (=) – zpracování závorek */
  P_GT    /**< Redukce (>) – nahrazení výrazu podle gramatického pravidla */
} Prec;

/**
 * @brief Vyhledá precedenční relaci pro danou dvojici symbolů.
 * @details Na základě symbolu na vrcholu zásobníku a příchozího symbolu ze vstupu
 *          vrátí odpovídající akci (posun, redukce, sloučení nebo chyba).
 *
 * @param stackTop Symbol nacházející se na vrcholu zásobníku (nejvyšší terminál).
 * @param input Příchozí symbol ze vstupu (lookahead).
 * @return Precedenční relace typu Prec (P_LT, P_GT, P_EQ nebo P_ERR).
 */
Prec precLookup(PrecSym stackTop, PrecSym input);

#endif /* EXPR_TABLE_H */