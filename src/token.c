/**
 * @file
 * @brief Inicializace, uvolnění a diagnostické názvy tokenů IFJ26.
 * @details Implementace překladače IFJ26 (IFJ/IAL 2026/27).
 * @author Jan Křepinský (xkrepij00@stud.fit.vutbr.cz)
 */
#include "token.h"

#include <stdlib.h>

void token_init(Token *token) {
    if (token == NULL) {
        return;
    }

    token->type = TOK_IDENTIFIER;
    token->data.num_int_val = 0;
    token->data.num_float_val = 0.0;
    token->text_length = 0;
    token->line = 0;
    token->column = 0;
    token->end_line = 0;
    token->end_column = 0;
}

void token_free(Token *token) {
    if (token == NULL) {
        return;
    }
    if (token->type == TOK_IDENTIFIER || token->type == TOK_STRING_LITERAL) {
        free(token->data.text);
    }
    token_init(token);
}

/* AI prompt: podle enum token_type v src/token.h vytvoř funkci,
 * která podle předaného TokenType vrátí název příslušné hodnoty. Model: GPT-6, Codex.
 */
const char *token_type_name(const TokenType type) {
    switch (type) {
    case TOK_IDENTIFIER:
        return "TOK_IDENTIFIER";
    case TOK_INT_LITERAL:
        return "TOK_INT_LITERAL";
    case TOK_DOUBLE_LITERAL:
        return "TOK_DOUBLE_LITERAL";
    case TOK_STRING_LITERAL:
        return "TOK_STRING_LITERAL";
    case TOK_KW_IMPORT:
        return "TOK_KW_IMPORT";
    case TOK_KW_DEF:
        return "TOK_KW_DEF";
    case TOK_KW_VAL:
        return "TOK_KW_VAL";
    case TOK_KW_VAR:
        return "TOK_KW_VAR";
    case TOK_KW_IF:
        return "TOK_KW_IF";
    case TOK_KW_THEN:
        return "TOK_KW_THEN";
    case TOK_KW_ELSE:
        return "TOK_KW_ELSE";
    case TOK_KW_WHILE:
        return "TOK_KW_WHILE";
    case TOK_KW_DO:
        return "TOK_KW_DO";
    case TOK_KW_RETURN:
        return "TOK_KW_RETURN";
    case TOK_KW_INT:
        return "TOK_KW_INT";
    case TOK_KW_DOUBLE:
        return "TOK_KW_DOUBLE";
    case TOK_KW_STRING:
        return "TOK_KW_STRING";
    case TOK_KW_UNIT:
        return "TOK_KW_UNIT";
    case TOK_PLUS:
        return "TOK_PLUS";
    case TOK_MINUS:
        return "TOK_MINUS";
    case TOK_STAR:
        return "TOK_STAR";
    case TOK_SLASH:
        return "TOK_SLASH";
    case TOK_ASSIGN:
        return "TOK_ASSIGN";
    case TOK_EQ:
        return "TOK_EQ";
    case TOK_NEQ:
        return "TOK_NEQ";
    case TOK_LT:
        return "TOK_LT";
    case TOK_LE:
        return "TOK_LE";
    case TOK_GT:
        return "TOK_GT";
    case TOK_GE:
        return "TOK_GE";
    case TOK_LPAREN:
        return "TOK_LPAREN";
    case TOK_RPAREN:
        return "TOK_RPAREN";
    case TOK_COMMA:
        return "TOK_COMMA";
    case TOK_COLON:
        return "TOK_COLON";
    case TOK_SEMICOLON:
        return "TOK_SEMICOLON";
    case TOK_DOT:
        return "TOK_DOT";
    case TOK_AT:
        return "TOK_AT";
    case TOK_NEWLINE:
        return "TOK_NEWLINE";
    case TOK_INDENT:
        return "TOK_INDENT";
    case TOK_DEDENT:
        return "TOK_DEDENT";
    case TOK_EOF:
        return "TOK_EOF";
    default:
        return "TOK_UNKNOWN";
    }
}
