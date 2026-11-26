/**
 * @file
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "error.h"
#include "lexer.h"
#include "token.h"
#include <stddef.h>

typedef struct {
    ErrorCode code;
    size_t line;
    size_t column;
    const char *message;
} ParseError;

typedef struct {
    Lexer *lexer;
    Token current;
    Token lookahead;
} Parser;

typedef struct {
    size_t line, column;
} SourcePos;

ErrorCode parserInit(Parser *parser, Lexer *lexer, ParseError *error);
void parserDestroy(Parser *parser);
ErrorCode setParserError(ParseError *error, ErrorCode code, const Token *at, const char *message);
ErrorCode setParserErrorAt(ParseError *error, ErrorCode code, size_t line, size_t column,
                           const char *message);
ErrorCode parserAdvance(Parser *parser, ParseError *error);
// Creates AST leaf node from token operand. Returns NULL if alocation fails.
Node *tokenToNode(const Parser *parser);

// not a part of the actual API, but unit tests need to see them
ErrorCode parseCall(Parser *parser, ParseError *error, Node **result);
ErrorCode parseExprOrCall(Parser *parser, ParseError *error, Node **result);
ErrorCode parseSimpleStatement(Parser *parser, ParseError *error, Node **result);
ErrorCode parseBlock(Parser *parser, ParseError *error, Node **result);
ErrorCode parseProgram(Parser *parser, ParseError *error, Node **result);

#endif
