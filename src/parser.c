#include "parser.h"
#include "expression_parser.h"
#include <stdbool.h>
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

static bool isTokenTypeEqual(const Parser *parser, TokenType type) {
    return parser->current.type == type;
}

static ErrorCode expect(Parser *parser, TokenType expected_type, ParseError *error) {
    if (!isTokenTypeEqual(parser, expected_type)) {
        return setParserError(error, ERR_SYNTAX, &parser->current, "unexpected token");
    }

    return parserAdvance(parser, error);
}

static TypeKind typeKindFromToken(TokenType type) {
    switch (type) {
    case TOK_KW_INT:
        return TY_INT;
    case TOK_KW_DOUBLE:
        return TY_DOUBLE;
    case TOK_KW_STRING:
        return TY_STRING;
    case TOK_KW_UNIT:
        return TY_UNIT;
    default:
        return TY_UNKNOWN;
    }
}

static ErrorCode parseType(Parser *parser, ParseError *error, TypeKind *out) {
    TypeKind kind = typeKindFromToken(parser->current.type);

    if (kind == TY_UNKNOWN) {
        return setParserError(error, ERR_SYNTAX, &parser->current, "expected type");
    }

    *out = kind;
    return parserAdvance(parser, error);
}

static ErrorCode parseTypeOpt(Parser *parser, ParseError *error, TypeKind *out) {
    if (!isTokenTypeEqual(parser, TOK_COLON)) {
        *out = TY_UNKNOWN;
        return ERR_OK;
    }

    ErrorCode code = parserAdvance(parser, error);

    if (code != ERR_OK) {
        return code;
    }

    return parseType(parser, error, out);
}

// Creates AST leaf node from token operand. Returns NULL if alocation fails.
Node *nodeFromToken(const Token *token) {
    Node *n;
    switch (token->type) {
    case TOK_IDENTIFIER:
        n = nodeVar(token->data.text);
        break;
    case TOK_INT_LITERAL:
        n = nodeLitInt(token->data.num_int_val);
        break;
    case TOK_DOUBLE_LITERAL:
        n = nodeLitDouble(token->data.num_float_val);
        break;
    case TOK_STRING_LITERAL:
        n = nodeLitString(token->data.text);
        break;
    default:
        return NULL;
    }
    nodeSetPos(n, token->line, token->column);
    return n;
}

static bool isTerm(TokenType type) {
    return type == TOK_INT_LITERAL || type == TOK_STRING_LITERAL || type == TOK_DOUBLE_LITERAL ||
           type == TOK_IDENTIFIER;
}

static ErrorCode parseTerm(Parser *parser, ParseError *error, Node **out) {
    if (!isTerm(parser->current.type)) {
        return setParserError(error, ERR_SYNTAX, &parser->current, "expected term");
    }

    *out = nodeFromToken(&parser->current);
    if (*out == NULL) {
        return setParserError(error, ERR_INTERNAL, &parser->current, "out of memory");
    }

    return parserAdvance(parser, error);
}

static ErrorCode parseArgs(Parser *parser, ParseError *error, Node **args) {
    if (isTokenTypeEqual(parser, TOK_RPAREN)) {
        return ERR_OK;
    }

    while (true) {
        Node *arg = NULL;
        ErrorCode code = parseTerm(parser, error, &arg);
        if (code != ERR_OK) {
            return code;
        }
        nodeAppend(args, arg);

        if (!isTokenTypeEqual(parser, TOK_COMMA)) {
            return ERR_OK;
        }

        code = parserAdvance(parser, error);

        if (code != ERR_OK) {
            return code;
        }
    }
}

static ErrorCode parseCall(Parser *parser, ParseError *error, Node **result) {
    Node *call = nodeCall(parser->current.data.text, NULL);

    if (call == NULL) {
        return setParserError(error, ERR_INTERNAL, &parser->current, "failed to create call node");
    }

    nodeSetPos(call, (int)parser->current.line, (int)parser->current.column);

    ErrorCode code = parserAdvance(parser, error);
    if (code == ERR_OK) {
        code = expect(parser, TOK_LPAREN, error);
    }

    if (code == ERR_OK) {
        code = parseArgs(parser, error, &call->args);
    }

    if (code == ERR_OK) {
        code = expect(parser, TOK_RPAREN, error);
    }

    if (code != ERR_OK) {
        nodeFree(call);
        return code;
    }

    *result = call;
    return ERR_OK;
}

ErrorCode parseExprOrCall(Parser *parser, ParseError *error, Node **result) {
    if (parser == NULL || error == NULL || result == NULL) {
        return setParserError(error, ERR_INTERNAL, NULL, "invalid parseExprOrCall arguments");
    }

    // ensure caller is not freeing random address
    *result = NULL;

    if (isTokenTypeEqual(parser, TOK_IDENTIFIER) && parser->lookahead.type == TOK_LPAREN) {
        return parseCall(parser, error, result);
    }
    return parseExpression(parser, error, result);
}

static ErrorCode parseReturn(Parser *parser, ParseError *error, Node **result) {
    size_t line = parser->current.line, column = parser->current.column;

    ErrorCode code = expect(parser, TOK_KW_RETURN, error);

    if (code != ERR_OK) {
        return code;
    }

    Node *ret;

    if (isTokenTypeEqual(parser, TOK_NEWLINE) || isTokenTypeEqual(parser, TOK_SEMICOLON) ||
        isTokenTypeEqual(parser, TOK_DEDENT) || isTokenTypeEqual(parser, TOK_EOF)) {

        ret = nodeReturn(NULL);

        if (ret == NULL) {
            return setParserError(error, ERR_INTERNAL, &parser->current, "out of memory");
        }

        nodeSetPos(ret, (int)line, (int)column);
        *result = ret;

        return ERR_OK;
    }

    Node *expr;
    code = parseExprOrCall(parser, error, &expr);
    if (code != ERR_OK) {
        return code;
    }

    ret = nodeReturn(expr);

    if (ret == NULL) {
        return setParserError(error, ERR_INTERNAL, &parser->current, "out of memory");
    }

    nodeSetPos(ret, (int)line, (int)column);
    *result = ret;
    return ERR_OK;
}

static ErrorCode parseVarDef(Parser *parser, ParseError *error, Node **result) {
    int is_var = isTokenTypeEqual(parser, TOK_KW_VAR);
    size_t line = parser->current.line, column = parser->current.column;

    ErrorCode code = parserAdvance(parser, error);
    if (code != ERR_OK) {
        return code;
    }

    if (!isTokenTypeEqual(parser, TOK_IDENTIFIER)) {
        return setParserError(error, ERR_SYNTAX, &parser->current, "expected identifier");
    }

    Node *decl = nodeDecl(parser->current.data.text, is_var, TY_UNKNOWN, NULL);
    if (decl == NULL) {
        return setParserError(error, ERR_INTERNAL, &parser->current, "out of memory");
    }

    nodeSetPos(decl, (int)line, (int)column);

    code = parserAdvance(parser, error);
    if (code == ERR_OK) {
        code = parseTypeOpt(parser, error, &decl->type);
    }

    if (code == ERR_OK) {
        code = expect(parser, TOK_ASSIGN, error);
    }

    if (code == ERR_OK) {
        code = parseExprOrCall(parser, error, &decl->left);
    }

    if (code != ERR_OK) {
        nodeFree(decl);
        return code;
    }

    *result = decl;
    return ERR_OK;
}

ErrorCode parseSimpleStatement(Parser *parser, ParseError *error, Node **result) {
    if (parser == NULL || error == NULL || result == NULL) {
        return setParserError(error, ERR_INTERNAL, NULL, "invalid parseSimpleStatement arguments");
    }

    *result = NULL;

    if (isTokenTypeEqual(parser, TOK_KW_VAL) || isTokenTypeEqual(parser, TOK_KW_VAR)) {
        // 32: <simple_stmt> -> <var_def>
        return parseVarDef(parser, error, result);
    }

    if (isTokenTypeEqual(parser, TOK_KW_RETURN)) {
        // return
        return parseReturn(parser, error, result);
    }

    if (isTokenTypeEqual(parser, TOK_IDENTIFIER) && parser->lookahead.type == TOK_ASSIGN) {
        // <simple_statement> -> id = <expr_or_call>

        Node *assign = nodeAssign(parser->current.data.text, NULL);

        if (assign == NULL) {
            return setParserError(error, ERR_INTERNAL, &parser->current, "out of memory");
        }
        nodeSetPos(assign, (int)parser->current.line, (int)parser->current.column);

        ErrorCode code = parserAdvance(parser, error);
        if (code == ERR_OK) {
            code = parserAdvance(parser, error);
        }

        if (code == ERR_OK) {
            code = parseExprOrCall(parser, error, &assign->left);
        }

        if (code != ERR_OK) {
            nodeFree(assign);
            return code;
        }

        *result = assign;
        return code;
    }

    return parseExprOrCall(parser, error, result);
}
