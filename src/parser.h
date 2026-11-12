#ifndef PARSER_H
#define PARSER_H

#include "error.h"
#include "lexer.h"
#include "token.h"
#include <stddef.h>

typedef struct {
    int code;
    size_t line;
    size_t column;
    const char *message;
} ParseError;

typedef struct {
    Lexer *lexer;
    Token current;
    Token lookahead;
} Parser;

int parserInit(Parser *parser, Lexer *lexer);
void parserDestroy(Parser *parser);
ErrorCode parserAdvance(Parser *parser, ParseError *error);
ErrorCode setParserError(ParseError *error, ErrorCode code, const Token *at, const char *message);
ErrorCode parserAdvance(Parser *parser, ParseError *error);

#endif
