#include "parser.h"
#include <stdlib.h>
#include <string.h>

static ErrorCode readToken(Parser *parser, Token *out, ParseError *error) {
    LexerResult rc = next_token(parser->lexer, out);
    if (rc == LEXER_OK) {
        return ERR_OK;
    }

    ErrorCode code = (rc == LEXER_ERROR) ? ERR_LEXICAL : ERR_INTERNAL;
    return setParserErrorAt(error, code, parser->lexer->line, parser->lexer->column,
                            code == ERR_LEXICAL ? "invalid token" : "lexer internal error");
}

ErrorCode parserInit(Parser *parser, Lexer *lexer, ParseError *error) {
    if (parser == NULL || lexer == NULL) {
        return setParserError(error, ERR_INTERNAL, NULL, "invalid parser arguments");
    }

    memset(parser, 0, sizeof(*parser));
    parser->lexer = lexer;

    int rc = readToken(parser, &parser->current, error);
    if (rc != ERR_OK) {
        return rc;
    }

    rc = readToken(parser, &parser->lookahead, error);
    if (rc != ERR_OK) {
        token_free(&parser->current);
        memset(&parser->current, 0, sizeof(parser->current));
        return rc;
    }

    return ERR_OK;
}

void parserDestroy(Parser *parser) {
    if (parser == NULL) {
        return;
    }

    token_free(&parser->current);
    token_free(&parser->lookahead);
}

ErrorCode setParserErrorAt(ParseError *error, ErrorCode code, size_t line, size_t column,
                           const char *message) {
    if (error != NULL && error->code == ERR_OK) {
        error->code = code;
        error->line = line;
        error->column = column;
        error->message = message;
    }
    return code;
}

ErrorCode setParserError(ParseError *error, ErrorCode code, const Token *at, const char *message) {
    return setParserErrorAt(error, code, at ? at->line : 0, at ? at->column : 0, message);
}

ErrorCode parserAdvance(Parser *parser, ParseError *error) {
    if (parser == NULL || parser->lexer == NULL) {
        return setParserError(error, ERR_INTERNAL, NULL, "invalid parser arguments");
    }

    Token next;
    int rc = readToken(parser, &next, error);
    if (rc != ERR_OK) {
        return rc;
    }

    token_free(&parser->current);
    parser->current = parser->lookahead;
    parser->lookahead = next;

    return ERR_OK;
}
