/**
 * @file
 * @brief Tokeny a jejich vlastnictví pro lexer IFJ26.
 * @details Implementace překladače IFJ26 (IFJ/IAL 2026/27).
 * @author Jan Křepinský (xkrepij00@stud.fit.vutbr.cz)
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

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

typedef union token_data {
    int64_t num_int_val;
    double num_float_val;
    char *text;
} TokenData;

typedef struct token {
    TokenType type;
    TokenData data;
    size_t text_length;
    size_t line;
    size_t column;
    size_t end_line;
    size_t end_column;
} Token;

/** @brief Inicializuje nový token.
 * @param[out] token Nový token.
 */
void token_init(Token *token);

/** @brief Uvolní vlastněný text a resetuje token.
 * @param[in,out] token Inicializovaný token.
 */
void token_free(Token *token);

/** @brief Vrátí statický název druhu tokenu.
 * @param[in] type Typ tokenu.
 * @return Název tokenu, pro neznámou hodnotu TOK_UNKNOWN.
 */
const char *token_type_name(TokenType type);
