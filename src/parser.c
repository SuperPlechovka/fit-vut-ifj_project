#include "error.h"
#include "parser.h"
#include <stdlib.h>
#include <string.h>

int parserInit(Parser *parser, Lexer *lexer) {
    if (parser == NULL || lexer == NULL) {
        return EXIT_FAILURE;
    }

    memset(parser, 0, sizeof(*parser));
    parser->lexer = lexer;

    if (next_token(lexer, &parser->current) != 0) {
        return EXIT_FAILURE;
    }

    if (next_token(lexer, &parser->lookahead) != 0) {
        token_free(&parser->current);
        memset(&parser->current, 0, sizeof(parser->current));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
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

ErrorCode setParserError(ParseError *error, ErrorCode code, const Token *at,
                                const char *message) {
    return setParserErrorAt(error, code, at ? at->line : 0, at ? at->column : 0, message);
}

ErrorCode parserAdvance(Parser *parser, ParseError *error) {
    if (parser == NULL || parser->lexer == NULL) {
        return setParserError(error, ERR_INTERNAL, NULL, "invalid parser arguments");
    }

    Token next;
    int lex_rc = next_token(parser->lexer, &next);
    if (lex_rc != LEXER_OK) {
        ErrorCode code = (lex_rc == LEXER_ERROR) ? ERR_LEXICAL : ERR_INTERNAL;
        return setParserErrorAt(error, code, parser->lexer->line, parser->lexer->column,
                                code == ERR_LEXICAL ? "invalid token" : "lexer internal error");
    }

    token_free(&parser->current);
    parser->current = parser->lookahead;
    parser->lookahead = next;

    return ERR_OK;
}
