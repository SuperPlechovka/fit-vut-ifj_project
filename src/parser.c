/**
 * @file
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
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

static SourcePos currentPos(const Parser *parser) {
    return (SourcePos){parser->current.line, parser->current.column};
}

static void setNodePos(Node *node, SourcePos pos) {
    nodeSetPos(node, (int)pos.line, (int)pos.column);
}

// Creates AST leaf node from token operand. Returns NULL if alocation fails.
Node *nodeFromToken(const Parser *parser) {
    Node *n;

    switch (parser->current.type) {
    case TOK_IDENTIFIER:
        n = nodeVar(parser->current.data.text);
        break;
    case TOK_INT_LITERAL:
        n = nodeLitInt(parser->current.data.num_int_val);
        break;
    case TOK_DOUBLE_LITERAL:
        n = nodeLitDouble(parser->current.data.num_float_val);
        break;
    case TOK_STRING_LITERAL:
        n = nodeLitString(parser->current.data.text);
        break;
    default:
        return NULL;
    }

    setNodePos(n, currentPos(parser));
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

    *out = nodeFromToken(parser);
    if (*out == NULL) {
        return setParserError(error, ERR_INTERNAL, &parser->current, "out of memory");
    }

    return parserAdvance(parser, error);
}

typedef ErrorCode (*ItemParser)(Parser *parser, ParseError *error, Node **out);

static ErrorCode parseCommaList(Parser *parser, ParseError *error, ItemParser parseItem,
                                Node **list) {
    *list = NULL;

    if (isTokenTypeEqual(parser, TOK_RPAREN)) {
        return ERR_OK;
    }

    while (true) {
        Node *item = NULL;
        ErrorCode code = parseItem(parser, error, &item);
        if (code != ERR_OK) {
            return code;
        }

        nodeAppend(list, item);

        if (!isTokenTypeEqual(parser, TOK_COMMA)) {
            return ERR_OK;
        }

        code = parserAdvance(parser, error);

        if (code != ERR_OK) {
            return code;
        }
    }
}

static ErrorCode parseArgs(Parser *parser, ParseError *error, Node **args) {
    // 47. <args>            -> <term> <args_tail>
    // 48. <args>            -> eps
    // 49. <args_tail>       -> , <term> <args_tail>
    // 50. <args_tail>       -> eps
    return parseCommaList(parser, error, parseTerm, args);
}

static ErrorCode parseCall(Parser *parser, ParseError *error, Node **result) {
    Node *call = nodeCall(parser->current.data.text, NULL);

    if (call == NULL) {
        return setParserError(error, ERR_INTERNAL, &parser->current, "failed to create call node");
    }

    setNodePos(call, currentPos(parser));

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
    SourcePos pos = currentPos(parser);

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

        setNodePos(ret, pos);
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

    setNodePos(ret, pos);
    *result = ret;
    return ERR_OK;
}

static ErrorCode parseVarDef(Parser *parser, ParseError *error, Node **result) {
    int is_var = isTokenTypeEqual(parser, TOK_KW_VAR);
    SourcePos pos = currentPos(parser);

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

    setNodePos(decl, pos);

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

        setNodePos(assign, currentPos(parser));

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
    SourcePos pos = currentPos(parser);
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

    setNodePos(whileNode, pos);

    *result = whileNode;
    return code;
}

static ErrorCode parseIf(Parser *parser, ParseError *error, Node **result) {
    // 42. <if> -> if expr then <block> else <block>
    *result = NULL;
    SourcePos pos = currentPos(parser);
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

    setNodePos(ifNode, pos);

    *result = ifNode;
    return code;
}

static ErrorCode parseStatementSeparator(Parser *parser, ParseError *error) {
    // 30. <stmt_sep>        -> NEWLINE
    // 31. <stmt_sep>        -> ; <nl_opt>

    if (isTokenTypeEqual(parser, TOK_NEWLINE)) {
        return parserAdvance(parser, error);
    }

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

static ErrorCode parseParam(Parser *parser, ParseError *error, Node **out) {
    // 16. <param>           -> id : <type>
    SourcePos pos = currentPos(parser);

    *out = NULL;

    if (!isTokenTypeEqual(parser, TOK_IDENTIFIER)) {
        return setParserError(error, ERR_SYNTAX, &parser->current, "expected parameter name");
    }

    Node *param = nodeParam(parser->current.data.text, TY_UNKNOWN);
    if (param == NULL) {
        return setParserError(error, ERR_INTERNAL, &parser->current, "out of memory");
    }

    ErrorCode code = parserAdvance(parser, error);
    if (code == ERR_OK) {
        code = expect(parser, TOK_COLON, error);
    }

    if (code == ERR_OK) {
        code = parseType(parser, error, &param->type);
    }

    if (code != ERR_OK) {
        nodeFree(param);
        return code;
    }

    setNodePos(param, pos);

    *out = param;
    return code;
}

static ErrorCode parseParams(Parser *parser, ParseError *error, Node **params) {
    // 12. <params>          -> <param> <params_tail>
    // 13. <params>          -> eps
    // 14. <params_tail>     -> , <param> <params_tail>
    // 15. <params_tail>     -> eps
    return parseCommaList(parser, error, parseParam, params);
}

static ErrorCode parseBody(Parser *parser, ParseError *error, Node **result) {
    // 10. <func_body>       -> <expr_or_call> NEWLINE         // jednořádková funkce
    // 11. <func_body>       -> <block>                        // víceřádková funkce
    *result = NULL;

    // 11
    if (isTokenTypeEqual(parser, TOK_NEWLINE)) {
        return parseBlock(parser, error, result);
    }

    // 10
    ErrorCode code = parseExprOrCall(parser, error, result);

    if (code == ERR_OK) {
        if (isTokenTypeEqual(parser, TOK_EOF)) {
            return ERR_OK;
        }
        code = expect(parser, TOK_NEWLINE, error);
    }

    if (code != ERR_OK) {
        nodeFree(*result);
        *result = NULL;
    }

    return code;
}

static ErrorCode parseNewlineOptional(Parser *parser, ParseError *error) {
    //  8. <nl_opt>          -> NEWLINE
    //  9. <nl_opt>          -> eps
    if (isTokenTypeEqual(parser, TOK_NEWLINE)) {
        return parserAdvance(parser, error);
    }

    return ERR_OK;
}

static ErrorCode parseMain(Parser *parser, ParseError *error, bool *isMain) {
    //  6. <main_opt>        -> @ id <nl_opt>                  // id musí být "main", jinak chyba 2
    //  7. <main_opt>        -> eps
    *isMain = false;

    if (!isTokenTypeEqual(parser, TOK_AT)) {
        return ERR_OK;
    }

    ErrorCode code = parserAdvance(parser, error);

    if (code != ERR_OK) {
        return code;
    }

    if (!isTokenTypeEqual(parser, TOK_IDENTIFIER) ||
        (strcmp(parser->current.data.text, "main") != 0)) {
        return setParserError(error, ERR_SYNTAX, &parser->current, "expected id main");
    }

    *isMain = true;

    code = parserAdvance(parser, error);

    if (code == ERR_OK) {
        code = parseNewlineOptional(parser, error);
    }

    // //-------------------------------------------------
    // if (code == ERR_OK) {
    //     if (!isTokenTypeEqual(parser, TOK_IDENTIFIER) ||
    //         (strcmp(parser->current.data.text, "main") != 0)) {
    //         return setParserError(error, ERR_SYNTAX, &parser->current, "expected id main");
    //     }
    //
    //     *isMain = true;
    //
    //     code = parserAdvance(parser, error);
    //
    //     if (code == ERR_OK) {
    //         code = parseNewlineOptional(parser, error);
    //     }
    // }

    return code;
}

static ErrorCode parseFunctionDefinition(Parser *parser, ParseError *error, Node **result) {
    //  5. <func_def>        -> <main_opt> def id ( <params> ) <type_opt> = <func_body>
    *result = NULL;
    bool isMain;

    ErrorCode code = parseMain(parser, error, &isMain);

    // pos of keyword def
    SourcePos pos = currentPos(parser);

    if (code == ERR_OK) {
        code = expect(parser, TOK_KW_DEF, error);
    }

    Node *func = NULL;

    if (code == ERR_OK) {
        if (!isTokenTypeEqual(parser, TOK_IDENTIFIER)) {
            return setParserError(error, ERR_SYNTAX, &parser->current, "expected function name");
        }

        func = nodeFunc(parser->current.data.text, isMain, TY_UNKNOWN, NULL);
        if (func == NULL) {
            return setParserError(error, ERR_INTERNAL, &parser->current, "out of memory");
        }

        code = parserAdvance(parser, error);
    }

    if (code == ERR_OK) {
        code = expect(parser, TOK_LPAREN, error);
    }

    if (code == ERR_OK) {
        code = parseParams(parser, error, &func->args);
    }

    if (code == ERR_OK) {
        code = expect(parser, TOK_RPAREN, error);
    }

    if (code == ERR_OK) {
        code = parseTypeOpt(parser, error, &func->type);
    }

    if (code == ERR_OK) {
        code = expect(parser, TOK_ASSIGN, error);
    }

    if (code == ERR_OK) {
        code = parseBody(parser, error, &func->body);
    }

    if (code != ERR_OK) {
        nodeFree(func);
        *result = NULL;
        return code;
    }

    setNodePos(func, pos);
    *result = func;
    return code;
}

static ErrorCode parseFunctions(Parser *parser, ParseError *error, Node **result) {
    //  3. <funcs>           -> <func_def> <funcs>
    //  4. <funcs>           -> eps
    *result = NULL;

    while (isTokenTypeEqual(parser, TOK_AT) || isTokenTypeEqual(parser, TOK_KW_DEF)) {
        Node *funcDef = NULL;
        ErrorCode code = parseFunctionDefinition(parser, error, &funcDef);

        if (code != ERR_OK) {
            return code;
        }
        nodeAppend(result, funcDef);
    }

    return ERR_OK;
}

static ErrorCode parseProlog(Parser *parser, ParseError *error) {
    //  2. <prolog>          -> import id . * NEWLINE          // id musí být "ifj26", jinak chyba 2
    ErrorCode code = expect(parser, TOK_KW_IMPORT, error);

    if (code == ERR_OK) {
        if (!isTokenTypeEqual(parser, TOK_IDENTIFIER) ||
            (strcmp(parser->current.data.text, "ifj26") != 0)) {
            return setParserError(error, ERR_SYNTAX, &parser->current, "expected identifier ifj26");
        }

        code = parserAdvance(parser, error);
    }

    if (code == ERR_OK) {
        code = expect(parser, TOK_DOT, error);
    }

    if (code == ERR_OK) {
        code = expect(parser, TOK_STAR, error);
    }

    if (code == ERR_OK) {
        code = expect(parser, TOK_NEWLINE, error);
    }

    return code;
}

ErrorCode parseProgram(Parser *parser, ParseError *error, Node **result) {
    //  1. <program>         -> <prolog> <funcs> EOF
    if (parser == NULL || error == NULL || result == NULL) {
        return setParserError(error, ERR_INTERNAL, NULL, "invalid parseProgram arguments");
    }

    SourcePos pos = currentPos(parser);

    *result = NULL;

    Node *program = nodeProgram(NULL);

    if (program == NULL) {
        return setParserError(error, ERR_INTERNAL, &parser->current, "out of memory");
    }

    ErrorCode code = parseProlog(parser, error);

    if (code == ERR_OK) {
        code = parseFunctions(parser, error, &program->body);
    }

    if (code == ERR_OK && !isTokenTypeEqual(parser, TOK_EOF)) {
        code = setParserError(error, ERR_SYNTAX, &parser->current, "expected function definition");
    }

    if (code != ERR_OK) {
        nodeFree(program);
        *result = NULL;
        return code;
    }

    setNodePos(program, pos);
    *result = program;
    return ERR_OK;
}
