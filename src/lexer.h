/**
 * @file
 * @brief Rozhraní rozpracovaného lexeru IFJ26.
 * @details Implementace překladače IFJ26 (IFJ/IAL 2026/27).
 * @author Jan Křepinský (xkrepij00@stud.fit.vutbr.cz)
 */
#pragma once

#include <stdio.h>

#include "token.h"

typedef enum lexer_result {
    LEXER_OK = 0,
    LEXER_ERROR = 1,
    LEXER_INTERNAL_ERROR = 99
} LexerResult;

typedef struct lexer {
    FILE *input;
    size_t line;
    size_t column;
    char *lexem;
    size_t lexem_length;
    size_t lexem_capacity;
    size_t *indents;
    size_t indent_depth;
    size_t indent_capacity;
    size_t pending_dedents;
    int at_line_start;
    int eof_emitted;
    size_t parenthesis_depth;
} Lexer;

LexerResult lexer_init(Lexer *lexer, FILE *input);

void lexer_destroy(Lexer *lexer);

LexerResult next_token(Lexer *lexer, Token *token);
