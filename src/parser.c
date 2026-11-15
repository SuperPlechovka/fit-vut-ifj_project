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
    // 32. <simple_stmt>     -> <var_def>
    // 33. <simple_stmt>     -> <return>
    // 34. <simple_stmt>     -> id = <expr_or_call>            // LL(2): id a za ním =
    // 35. <simple_stmt>     -> <expr_or_call>
    if (parser == NULL || error == NULL || result == NULL) {
        return setParserError(error, ERR_INTERNAL, NULL, "invalid parseSimpleStatement arguments");
    }

    *result = NULL;

    if (isTokenTypeEqual(parser, TOK_KW_VAL) || isTokenTypeEqual(parser, TOK_KW_VAR)) {
        // 32: <simple_stmt> -> <var_def>
        return parseVarDef(parser, error, result);
    }

    if (isTokenTypeEqual(parser, TOK_KW_RETURN)) {
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

static ErrorCode parseWhile(Parser *parser, ParseError *error, Node **result) {
    // 43. <while>           -> while expr do <block>
    *result = NULL;
    size_t line = parser->current.line, column = parser->current.column;
    Node *whileNode = nodeWhile(NULL, NULL);

    if (whileNode == NULL) {
        return setParserError(error, ERR_INTERNAL, &parser->current, "out of memory");
    }

    ErrorCode code = expect(parser, TOK_KW_WHILE, error);

    if (code == ERR_OK) {
        code = parseExpression(parser, error, &whileNode->left);
    }

    if (code == ERR_OK) {
        code = expect(parser, TOK_KW_DO, error);
    }

    if (code == ERR_OK) {
        code = parseBlock(parser, error, &whileNode->body);
    }

    if (code != ERR_OK) {
        *result = NULL;
        nodeFree(whileNode);
        return code;
    }

    nodeSetPos(whileNode, (int)line, (int)column);

    *result = whileNode;
    return code;
}

static ErrorCode parseIf(Parser *parser, ParseError *error, Node **result) {
    // 42. <if> -> if expr then <block> else <block>
    *result = NULL;
    size_t line = parser->current.line, column = parser->current.column;
    Node *ifNode = nodeIf(NULL, NULL, NULL);

    if (ifNode == NULL) {
        return setParserError(error, ERR_INTERNAL, &parser->current, "out of memory");
    }

    ErrorCode code = expect(parser, TOK_KW_IF, error);

    if (code == ERR_OK) {
        code = parseExpression(parser, error, &ifNode->left);
    }

    if (code == ERR_OK) {
        code = expect(parser, TOK_KW_THEN, error);
    }

    if (code == ERR_OK) {
        code = parseBlock(parser, error, &ifNode->body);
    }

    if (code == ERR_OK) {
        code = expect(parser, TOK_KW_ELSE, error);
    }

    if (code == ERR_OK) {
        code = parseBlock(parser, error, &ifNode->right);
    }

    if (code != ERR_OK) {
        nodeFree(ifNode);
        return code;
    }

    nodeSetPos(ifNode, (int)line, (int)column);

    *result = ifNode;
    return code;
}

static ErrorCode parseStatementSeparator(Parser *parser, ParseError *error) {
    // rule 30
    if (isTokenTypeEqual(parser, TOK_NEWLINE)) {
        return parserAdvance(parser, error);
    }

    // rule 31
    if (isTokenTypeEqual(parser, TOK_SEMICOLON)) {
        ErrorCode code = parserAdvance(parser, error);

        if (code == ERR_OK && isTokenTypeEqual(parser, TOK_NEWLINE)) {
            code = parserAdvance(parser, error);
        }
        return code;
    }

    if (isTokenTypeEqual(parser, TOK_DEDENT)) {
        return ERR_OK;
    }

    return setParserError(error, ERR_SYNTAX, &parser->current, "expected newline or ;");
}

static ErrorCode parseStatement(Parser *parser, ParseError *error, Node **result) {
    // 27. <statement>       -> <simple_stmt> <stmt_sep>
    // 28. <statement>       -> <if>
    // 29. <statement>       -> <while>
    *result = NULL;

    if (isTokenTypeEqual(parser, TOK_KW_IF)) {
        // 28
        return parseIf(parser, error, result);
    }

    if (isTokenTypeEqual(parser, TOK_KW_WHILE)) {
        // 29
        return parseWhile(parser, error, result);
    }

    ErrorCode code = parseSimpleStatement(parser, error, result);

    if (code == ERR_OK) {
        code = parseStatementSeparator(parser, error);
    }

    if (code != ERR_OK) {
        nodeFree(*result);
        *result = NULL;
    }

    return code;
}

static ErrorCode parseStatements(Parser *parser, ParseError *error, Node **result) {
    // 24. <statements>      -> <statement> <statements_tail>
    // 25. <statements_tail> -> <statement> <statements_tail>
    // 26. <statements_tail> -> eps
    *result = NULL;

    do {
        Node *statement = NULL;

        ErrorCode code = parseStatement(parser, error, &statement);

        if (code != ERR_OK) {
            nodeFree(*result);
            *result = NULL;
            return code;
        }

        nodeAppend(result, statement);

    } while (!isTokenTypeEqual(parser, TOK_DEDENT));

    return ERR_OK;
}

ErrorCode parseBlock(Parser *parser, ParseError *error, Node **result) {
    if (parser == NULL || error == NULL || result == NULL) {
        return setParserError(error, ERR_INTERNAL, NULL, "invalid parseBlock arguments");
    }

    *result = NULL;

    ErrorCode code = expect(parser, TOK_NEWLINE, error);
    if (code == ERR_OK) {
        code = expect(parser, TOK_INDENT, error);
    }

    if (code == ERR_OK) {
        code = parseStatements(parser, error, result);
    }

    if (code == ERR_OK) {
        code = expect(parser, TOK_DEDENT, error);
    }

    if (code != ERR_OK) {
        nodeFree(*result);
        *result = NULL;
    }

    return code;
}
