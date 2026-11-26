/**
 * @file
 * @brief Rozhraní tabulky symbolů IFJ26.
 * @details Implementace překladače IFJ26 (IFJ/IAL 2026/27).
 * @author Jan Křepinský (xkrepij00@stud.fit.vutbr.cz)
 */
#pragma once

#include <stddef.h>

/** @brief Tabulka symbolů. */
typedef struct symtable Symtable;

/** @brief Typ symbolu nebo interní značka typu parametru. */
typedef enum symbol_type {
    SYMBOL_TYPE_UNKNOWN,
    SYMBOL_TYPE_INT,
    SYMBOL_TYPE_DOUBLE,
    SYMBOL_TYPE_STRING,
    SYMBOL_TYPE_UNIT,
    SYMBOL_TYPE_ANY
} SymbolType;

/** @brief Druh symbolu */
typedef enum symbol_kind { SYMBOL_VAR, SYMBOL_VAL, SYMBOL_PARAMETER, SYMBOL_FUNCTION } SymbolKind;

/** @brief Informace o symbolu. */
typedef struct symbol_data {
    SymbolKind kind;                   /**< Druh symbolu. */
    SymbolType type;                   /**< Datový nebo návratový typ. */
    size_t parameter_count;            /**< Počet parametrů funkce. */
    const SymbolType *parameter_types; /**< Typy parametrů v pořadí */
    int defined;                       /**< Funkce má definici. */
    int builtin;                       /**< Jde o vestavěnou funkci. */
    int entry_point;                   /**< Funkce je označená @main. */
} SymbolData;

/** @brief Výsledek operace tabulky symbolů. */
typedef enum symtable_result {
    SYMTABLE_OK = 0,
    SYMTABLE_DUPLICATE,
    SYMTABLE_NOT_FOUND,
    SYMTABLE_INTERNAL_ERROR = 99
} SymtableResult;

/** @brief Vytvoří prázdnou tabulku symbolů.
 * @return Nová tabulka, a nebo NULL při selhání alokace.
 */
Symtable *symtable_create(void);

/** @brief Uvolní všechny symboly
 * @param[in,out] table Tabulka symbolů, případně NULL.
 */
void symtable_clear(Symtable *table);

/** @brief Uvolní tabulku včetně uložených jmen a typů parametrů.
 * @param[in,out] table Tabulka symbolů, případně NULL.
 */
void symtable_destroy(Symtable *table);

/** @brief Vloží nový symbol.
 * @param[in,out] table Tabulka.
 * @param[in] name Neprázdné jméno symbolu
 * @param[in] data Informace o symbolu
 * @retval SYMTABLE_OK Symbol byl vložen.
 * @retval SYMTABLE_DUPLICATE Stejné jméno a počet parametrů už existují.
 * @retval SYMTABLE_INTERNAL_ERROR Neplatný vstup nebo selhání alokace.
 */
SymtableResult symtable_insert(Symtable *table, const char *name, const SymbolData *data);

/** @brief Vyhledá symbol tabulce.
 * @param[in] table Tabulka symbolů.
 * @param[in] name Hledané jméno.
 * @param[in] parameter_count Počet parametrů funkce.
 * @return Data symbolu, nebo NULL při nenalezení nebo neplatném vstupu.
 */
const SymbolData *symtable_find(const Symtable *table, const char *name, size_t parameter_count);

/** @brief Aktualizuje informace o existujícím symbolu.
 * @param[in,out] table Tabulka obsahující symbol.
 * @param[in] name Jméno upravovaného symbolu.
 * @param[in] data Nová data stejného druhu a počtu parametrů.
 * @retval SYMTABLE_OK Data  nahrazena.
 * @retval SYMTABLE_NOT_FOUND Jméno s data->parameter_count v tabulce neexistuje.
 * @retval SYMTABLE_INTERNAL_ERROR Neplatný vstup, změna druhu nebo selhání alokace.
 */
SymtableResult symtable_update(Symtable *table, const char *name, const SymbolData *data);

/** @brief Odstraní symbol podle jména a počtu parametrů a vyváží strom.
 * @param[in,out] table Tabulka obsahující symbol.
 * @param[in] name Jméno symbolu.
 * @param[in] parameter_count Počet parametrů funkce.
 * @retval SYMTABLE_OK Symbol i jeho data byly uvolněny.
 * @retval SYMTABLE_NOT_FOUND Symbol neexistuje.
 * @retval SYMTABLE_INTERNAL_ERROR Neplatná tabulka nebo jméno
 */
SymtableResult symtable_remove(Symtable *table, const char *name, size_t parameter_count);
