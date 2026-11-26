/**
 * @file
 * @brief Tokeny a jejich vlastnictví pro lexer IFJ26.
 * @details Implementace překladače IFJ26 (IFJ/IAL 2026/27).
 * @author Jan Křepinský (xkrepij00@stud.fit.vutbr.cz)
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/** @brief Druhy tokenů. */
typedef enum token_type {
    TOK_IDENTIFIER,
    TOK_INT_LITERAL,
    TOK_DOUBLE_LITERAL,
    TOK_STRING_LITERAL,

    TOK_KW_IMPORT,
    TOK_KW_DEF,
    TOK_KW_VAL,
    TOK_KW_VAR,
    TOK_KW_IF,
    TOK_KW_THEN,
    TOK_KW_ELSE,
    TOK_KW_WHILE,
    TOK_KW_DO,
    TOK_KW_RETURN,
    TOK_KW_INT,
    TOK_KW_DOUBLE,
    TOK_KW_STRING,
    TOK_KW_UNIT,

    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,

    TOK_ASSIGN,
    TOK_EQ,
    TOK_NEQ,
    TOK_LT,
    TOK_LE,
    TOK_GT,
    TOK_GE,

    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_COLON,
    TOK_SEMICOLON,
    TOK_DOT,
    TOK_AT,

    TOK_NEWLINE,
    TOK_INDENT,
    TOK_DEDENT,
    TOK_EOF
} TokenType;

/** @brief Unie hodnot tokenu podle jeho druhu. */
typedef union token_data {
    int64_t num_int_val;  /**< Hodnota celočíselného literálu. */
    double num_float_val; /**< Hodnota desetinného literálu. */
    char *text;           /**< Vlastněný text identifikátoru nebo řetězce. */
} TokenData;

/** @brief Struktura tokenu s hodnotou a pozicí ve vstupu. */
typedef struct token {
    TokenType type;     /**< Druh tokenu. */
    TokenData data;     /**< Hodnota tokenu. */
    size_t text_length; /**< Délka textu bez ukončovací nuly. */
    size_t line;        /**< Počáteční řádek. */
    size_t column;      /**< Počáteční sloupec. */
    size_t end_line;    /**< Koncový řádek. */
    size_t end_column;  /**< Koncový sloupec. */
} Token;

/** @brief Inicializuje nový token.
 * @param[out] token Nový token, případně NULL.
 */
void token_init(Token *token);

/** @brief Uvolní vlastněný text a resetuje token.
 * @param[in,out] token Inicializovaný token, případně NULL.
 */
void token_free(Token *token);

/** @brief Vrátí statický název druhu tokenu.
 * @param[in] type Typ tokenu.
 * @return Název tokenu, pro neznámou hodnotu TOK_UNKNOWN.
 */
const char *token_type_name(TokenType type);
