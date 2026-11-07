/**
 * @file
 * @brief Správa stavu lexeru a místo pro implementaci tokenizace.
 * @details Implementace překladače IFJ26 (IFJ/IAL 2026/27).
 * @author Jan Křepinský (xkrepij00@stud.fit.vutbr.cz)
 */
#include "lexer.h"

#include <stdlib.h>
#include <string.h>

static int read_char(Lexer *lexer, int *character);
static int unread_char(Lexer *lexer, int character);
static int append_lexem(Lexer *lexer, char character);
static void reset_lexem(Lexer *lexer);
static TokenType keyword_type(const char *text, size_t length);
static int set_token(Token *token, TokenType type, TokenData data, const char *text,
                     size_t text_length, size_t line, size_t column, size_t end_line,
                     size_t end_column);
static int is_letter(int character);
static int is_digit(int character);
static int is_identifier_character(int character);
static LexerResult handle_line_start(Lexer *lexer, Token *token, int first_character);
static LexerResult read_line_comment(Lexer *lexer, Token *token);

typedef enum lexer_scan_state {
    SCAN_START,
    SCAN_LINE_START,
    SCAN_OPERATOR,
    SCAN_IDENTIFIER,
    SCAN_NUMBER,
    SCAN_STRING,
    SCAN_ESCAPE,
    SCAN_DONE,
    SCAN_ERROR
} LexerScanState;

LexerResult lexer_init(Lexer *lexer, FILE *input) {
    if (lexer == NULL || input == NULL || lexer->input != NULL) {
        return LEXER_INTERNAL_ERROR;
    }

    lexer->lexem_capacity = 32;
    lexer->indent_capacity = 8;
    lexer->lexem = malloc(lexer->lexem_capacity);
    lexer->indents = malloc(lexer->indent_capacity * sizeof(*lexer->indents));
    if (lexer->lexem == NULL || lexer->indents == NULL) {
        free(lexer->lexem);
        free(lexer->indents);
        *lexer = (Lexer){0};
        return LEXER_INTERNAL_ERROR;
    }

    lexer->input = input;
    lexer->line = 1;
    lexer->column = 1;
    lexer->lexem[0] = '\0';
    lexer->lexem_length = 0;
    lexer->indents[0] = 0;
    lexer->indent_depth = 1;
    lexer->pending_dedents = 0;
    lexer->at_line_start = 1;
    lexer->eof_emitted = 0;
    lexer->parenthesis_depth = 0;
    return LEXER_OK;
}

void lexer_destroy(Lexer *lexer) {
    if (lexer == NULL) {
        return;
    }
    free(lexer->lexem);
    free(lexer->indents);
    *lexer = (Lexer){0};
}

LexerResult next_token(Lexer *lexer, Token *token) {
    if (token == NULL || lexer == NULL || lexer->input == NULL) {
        return LEXER_INTERNAL_ERROR;
    }
    token_init(token);
    if (lexer->eof_emitted) {
        const int eof_result = set_token(token, TOK_EOF, (TokenData){0}, NULL, 0, lexer->line,
                                         lexer->column, lexer->line, lexer->column);
        if (!eof_result) {
            return LEXER_INTERNAL_ERROR;
        }
        return LEXER_OK;
    }
    reset_lexem(lexer);

    LexerScanState state = SCAN_START;
    int character = 0;
    int set_token_ok = 1;
    size_t token_line = lexer->line;
    size_t token_column = lexer->column;
    int operator_first = 0;

    while (state != SCAN_DONE && state != SCAN_ERROR) {
        const int read_result = read_char(lexer, &character);
        if (!read_result) {
            return LEXER_INTERNAL_ERROR;
        }

        switch (state) {
        case SCAN_START:
            if (lexer->at_line_start) {
                state = SCAN_LINE_START;
                unread_char(lexer, character);
                continue;
            }
            if (character == EOF) {
                if (lexer->indent_depth > 1) {
                    lexer->indent_depth--;
                    set_token_ok =
                        set_token(token, TOK_DEDENT, (TokenData){0}, NULL, 0, lexer->line,
                                  lexer->column, lexer->line, lexer->column);
                    if (!set_token_ok) {
                        return LEXER_INTERNAL_ERROR;
                    }
                    lexer->at_line_start = 1;
                    return LEXER_OK;
                }
                set_token(token, TOK_EOF, (TokenData){0}, NULL, 0, lexer->line, lexer->column,
                          lexer->line, lexer->column);
                lexer->eof_emitted = 1;
                state = SCAN_DONE;
            } else if (character == ' ' || character == '\t' || character == '\r') {
                continue;
            } else if (character == '\n') {
                lexer->at_line_start = 1;
                set_token(token, TOK_NEWLINE, (TokenData){0}, NULL, 0, lexer->line - 1, 1,
                          lexer->line, 1);
                state = SCAN_DONE;
            } else if (is_letter(character) || character == '_') {
                token_line = lexer->line;
                token_column = lexer->column - 1;
                append_lexem(lexer, (char)character);
                state = SCAN_IDENTIFIER;
            } else if (is_digit(character)) {
                token_line = lexer->line;
                token_column = lexer->column - 1;
                append_lexem(lexer, (char)character);
                state = SCAN_NUMBER;
            } else if (character == '"') {
                token_line = lexer->line;
                token_column = lexer->column - 1;
                state = SCAN_STRING;
            } else if (character == '/') {
                int next_character;
                read_char(lexer, &next_character);
                if (next_character == '/') {
                    return read_line_comment(lexer, token);
                }
                unread_char(lexer, next_character);
                set_token_ok = set_token(token, TOK_SLASH, (TokenData){0}, NULL, 0, lexer->line,
                                         lexer->column - 1, lexer->line, lexer->column);
                if (!set_token_ok) {
                    return LEXER_INTERNAL_ERROR;
                }
                return LEXER_OK;
            } else if (character == '=' || character == '!' || character == '<' ||
                       character == '>') {
                operator_first = character;
                token_line = lexer->line;
                token_column = lexer->column - 1;
                state = SCAN_OPERATOR;
            } else {
                TokenType type;
                switch (character) {
                case '+':
                    type = TOK_PLUS;
                    break;
                case '-':
                    type = TOK_MINUS;
                    break;
                case '*':
                    type = TOK_STAR;
                    break;
                case '/':
                    type = TOK_SLASH;
                    break;
                case '(':
                    type = TOK_LPAREN;
                    break;
                case ')':
                    type = TOK_RPAREN;
                    break;
                case ',':
                    type = TOK_COMMA;
                    break;
                case ':':
                    type = TOK_COLON;
                    break;
                case ';':
                    type = TOK_SEMICOLON;
                    break;
                case '.':
                    type = TOK_DOT;
                    break;
                case '@':
                    type = TOK_AT;
                    break;
                default:
                    state = SCAN_ERROR;
                    continue;
                }
                set_token_ok = set_token(token, type, (TokenData){0}, NULL, 0, lexer->line,
                                         lexer->column - 1, lexer->line, lexer->column);
                if (!set_token_ok)
                    return LEXER_INTERNAL_ERROR;
                state = SCAN_DONE;
            }
            break;

        case SCAN_LINE_START: {
            const LexerResult indentation_result = handle_line_start(lexer, token, character);
            if (indentation_result != LEXER_OK) {
                return indentation_result;
            }
            if (token->type == TOK_INDENT || token->type == TOK_DEDENT) {
                state = SCAN_DONE;
            } else {
                state = SCAN_START;
            }
        } break;

        case SCAN_OPERATOR:
            if (operator_first == '=' && character == '=') {
                set_token(token, TOK_EQ, (TokenData){0}, NULL, 0, token_line, token_column,
                          lexer->line, lexer->column);
            } else if (operator_first == '=') {
                unread_char(lexer, character);
                set_token(token, TOK_ASSIGN, (TokenData){0}, NULL, 0, token_line, token_column,
                          token_line, token_column + 1);
            } else if (operator_first == '!' && character == '=') {
                set_token(token, TOK_NEQ, (TokenData){0}, NULL, 0, token_line, token_column,
                          lexer->line, lexer->column);
            } else if (operator_first == '<' && character == '=') {
                set_token(token, TOK_LE, (TokenData){0}, NULL, 0, token_line, token_column,
                          lexer->line, lexer->column);
            } else if (operator_first == '>' && character == '=') {
                set_token(token, TOK_GE, (TokenData){0}, NULL, 0, token_line, token_column,
                          lexer->line, lexer->column);
            } else {
                unread_char(lexer, character);
                if (operator_first == '<') {
                    set_token(token, TOK_LT, (TokenData){0}, NULL, 0, token_line, token_column,
                              token_line, token_column + 1);
                } else if (operator_first == '>') {
                    set_token(token, TOK_GT, (TokenData){0}, NULL, 0, token_line, token_column,
                              token_line, token_column + 1);
                } else {
                    state = SCAN_ERROR;
                    break;
                }
            }
            state = SCAN_DONE;
            break;

        case SCAN_IDENTIFIER:
            if (is_identifier_character(character)) {
                const int append_result = append_lexem(lexer, (char)character);
                if (!append_result)
                    state = SCAN_ERROR;
            } else {
                unread_char(lexer, character);
                const TokenType token_type = keyword_type(lexer->lexem, lexer->lexem_length);
                if (token_type == TOK_IDENTIFIER) {
                    set_token_ok = set_token(token, token_type, (TokenData){0}, lexer->lexem,
                                             lexer->lexem_length, token_line, token_column,
                                             token_line, token_column + lexer->lexem_length);
                } else {
                    set_token_ok =
                        set_token(token, token_type, (TokenData){0}, NULL, 0, token_line,
                                  token_column, token_line, token_column + lexer->lexem_length);
                }
                if (!set_token_ok) {
                    state = SCAN_ERROR;
                } else {
                    state = SCAN_DONE;
                }
            }
            break;

        case SCAN_NUMBER:
            if (is_digit(character) || character == '.') {
                const int append_result = append_lexem(lexer, (char)character);
                if (!append_result)
                    state = SCAN_ERROR;
            } else {
                unread_char(lexer, character);
                if (is_letter(character) || character == '_') {
                    state = SCAN_ERROR;
                    break;
                }
                size_t dot_count = 0;
                for (size_t i = 0; i < lexer->lexem_length; ++i) {
                    if (lexer->lexem[i] == '.')
                        dot_count++;
                }
                if (dot_count > 1) {
                    state = SCAN_ERROR;
                    break;
                }
                if (strchr(lexer->lexem, '.') != NULL) {
                    TokenData data = {0};
                    data.num_float_val = strtod(lexer->lexem, NULL);
                    set_token_ok =
                        set_token(token, TOK_DOUBLE_LITERAL, data, NULL, 0, token_line,
                                  token_column, token_line, token_column + lexer->lexem_length);
                } else {
                    TokenData data = {0};
                    data.num_int_val = strtoll(lexer->lexem, NULL, 10);
                    set_token_ok =
                        set_token(token, TOK_INT_LITERAL, data, NULL, 0, token_line, token_column,
                                  token_line, token_column + lexer->lexem_length);
                }
                if (!set_token_ok) {
                    state = SCAN_ERROR;
                    break;
                }
                state = SCAN_DONE;
            }
            break;

        case SCAN_STRING:
            if (character == '"') {
                set_token_ok = set_token(token, TOK_STRING_LITERAL, (TokenData){0}, lexer->lexem,
                                         lexer->lexem_length, token_line, token_column, token_line,
                                         token_column + lexer->lexem_length);
                if (!set_token_ok) {
                    state = SCAN_ERROR;
                    break;
                }
                state = SCAN_DONE;
            } else if (character == '\\') {
                state = SCAN_ESCAPE;
            } else {
                if (character == '\n' || character == EOF) {
                    state = SCAN_ERROR;
                } else {
                    const int append_result = append_lexem(lexer, (char)character);
                    if (!append_result) {
                        state = SCAN_ERROR;
                    }
                }
            }
            break;

        case SCAN_ESCAPE:
            if (character == 'n' || character == 'r' || character == 't' || character == '"' ||
                character == '\\') {
                char escaped_character = (char)character;
                if (character == 'n')
                    escaped_character = '\n';
                if (character == 'r')
                    escaped_character = '\r';
                if (character == 't')
                    escaped_character = '\t';
                const int append_result = append_lexem(lexer, escaped_character);
                if (!append_result)
                    state = SCAN_ERROR;
                else {
                    state = SCAN_STRING;
                }
            } else {
                state = SCAN_ERROR;
            }
            break;

        case SCAN_DONE:
        case SCAN_ERROR:
            break;
        }
    }

    if (state == SCAN_ERROR) {
        token_free(token);
        return LEXER_ERROR;
    }
    return LEXER_OK;
}

static int is_letter(const int character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
}

static int is_digit(const int character) { return character >= '0' && character <= '9'; }

static int is_identifier_character(const int character) {
    return is_letter(character) || is_digit(character) || character == '_';
}

static int read_char(Lexer *lexer, int *character) {
    if (lexer == NULL || character == NULL) {
        return 0;
    }
    *character = fgetc(lexer->input);
    if (*character == EOF) {
        return 1;
    }
    if (*character == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return 1;
}

static LexerResult handle_line_start(Lexer *lexer, Token *token, const int first_character) {
    size_t indent = 0;
    int character;

    if (first_character == '\t') {
        return LEXER_ERROR;
    }
    character = first_character;
    while (character == ' ') {
        indent++;
        const int read_result = read_char(lexer, &character);
        if (!read_result) {
            return LEXER_INTERNAL_ERROR;
        }
    }
    unread_char(lexer, character);

    lexer->at_line_start = 0;
    if (character == '\n' || character == EOF) {
        return LEXER_OK;
    }

    const size_t previous_indent = lexer->indents[lexer->indent_depth - 1];
    if (indent > previous_indent) {
        if (lexer->indent_depth == lexer->indent_capacity) {
            const size_t new_capacity = lexer->indent_capacity * 2;
            size_t *new_indents = realloc(lexer->indents, new_capacity * sizeof(*new_indents));
            if (new_indents == NULL) {
                return LEXER_INTERNAL_ERROR;
            }
            lexer->indents = new_indents;
            lexer->indent_capacity = new_capacity;
        }
        lexer->indents[lexer->indent_depth] = indent;
        lexer->indent_depth++;
        const int set_result = set_token(token, TOK_INDENT, (TokenData){0}, NULL, 0, lexer->line, 1,
                                         lexer->line, indent + 1);
        if (!set_result) {
            return LEXER_INTERNAL_ERROR;
        }
        return LEXER_OK;
    }

    while (lexer->indent_depth > 1 && indent < lexer->indents[lexer->indent_depth - 1]) {
        lexer->indent_depth--;
        lexer->pending_dedents++;
    }
    if (indent != lexer->indents[lexer->indent_depth - 1]) {
        return LEXER_ERROR;
    }
    if (lexer->pending_dedents > 0) {
        lexer->pending_dedents--;
        lexer->at_line_start = 1;
        const int set_result =
            set_token(token, TOK_DEDENT, (TokenData){0}, NULL, 0, lexer->line, 1, lexer->line, 1);
        if (!set_result) {
            return LEXER_INTERNAL_ERROR;
        }
    }
    return LEXER_OK;
}

static LexerResult read_line_comment(Lexer *lexer, Token *token) {
    int character;
    int read_result = read_char(lexer, &character);
    while (read_result && character != '\n' && character != EOF) {
        read_result = read_char(lexer, &character);
    }
    if (character == '\n') {
        lexer->at_line_start = 1;
        const int set_result = set_token(token, TOK_NEWLINE, (TokenData){0}, NULL, 0,
                                         lexer->line - 1, 1, lexer->line, 1);
        if (!set_result) {
            return LEXER_INTERNAL_ERROR;
        }
        return LEXER_OK;
    }
    const int set_result = set_token(token, TOK_EOF, (TokenData){0}, NULL, 0, lexer->line,
                                     lexer->column, lexer->line, lexer->column);
    if (!set_result) {
        return LEXER_INTERNAL_ERROR;
    }
    lexer->eof_emitted = 1;
    return LEXER_OK;
}

static int unread_char(Lexer *lexer, const int character) {
    if (lexer == NULL || character == EOF) {
        return 0;
    }
    if (ungetc(character, lexer->input) == EOF)
        return 0;
    if (character == '\n') {
        if (lexer->line > 1) {
            lexer->line--;
        }
    } else if (lexer->column > 1) {
        lexer->column--;
    }
    return 1;
}

static int append_lexem(Lexer *lexer, const char character) {
    if (lexer == NULL) {
        return 0;
    }
    if (lexer->lexem_length + 1 >= lexer->lexem_capacity) {
        const size_t new_capacity = lexer->lexem_capacity * 2;
        char *new_lexem = realloc(lexer->lexem, new_capacity);
        if (new_lexem == NULL) {
            return 0;
        }
        lexer->lexem = new_lexem;
        lexer->lexem_capacity = new_capacity;
    }
    lexer->lexem[lexer->lexem_length] = character;
    lexer->lexem_length++;
    lexer->lexem[lexer->lexem_length] = '\0';
    return 1;
}

static void reset_lexem(Lexer *lexer) {
    if (lexer != NULL) {
        lexer->lexem_length = 0;
        if (lexer->lexem != NULL) {
            lexer->lexem[0] = '\0';
        }
    }
}

static TokenType keyword_type(const char *text, const size_t length) {
    static const struct keyword {
        const char *text;
        TokenType type;
    } keywords[] = {
        {.text = "import", .type = TOK_KW_IMPORT}, {.text = "def", .type = TOK_KW_DEF},
        {.text = "val", .type = TOK_KW_VAL},       {.text = "var", .type = TOK_KW_VAR},
        {.text = "if", .type = TOK_KW_IF},         {.text = "then", .type = TOK_KW_THEN},
        {.text = "else", .type = TOK_KW_ELSE},     {.text = "while", .type = TOK_KW_WHILE},
        {.text = "do", .type = TOK_KW_DO},         {.text = "return", .type = TOK_KW_RETURN},
        {.text = "Int", .type = TOK_KW_INT},       {.text = "Double", .type = TOK_KW_DOUBLE},
        {.text = "String", .type = TOK_KW_STRING}, {.text = "Unit", .type = TOK_KW_UNIT},
    };
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
        if (strlen(keywords[i].text) == length && strncmp(text, keywords[i].text, length) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_IDENTIFIER;
}

static int set_token(Token *token, const TokenType type, const TokenData data, const char *text,
                     const size_t text_length, const size_t line, const size_t column,
                     const size_t end_line, const size_t end_column) {
    token->type = type;
    token->data = data;
    token->text_length = 0;
    token->line = line;
    token->column = column;
    token->end_line = end_line;
    token->end_column = end_column;

    if (text == NULL) {
        return 1;
    }
    token->data.text = malloc(text_length + 1);
    if (token->data.text == NULL) {
        return 0;
    }
    memcpy(token->data.text, text, text_length);
    token->data.text[text_length] = '\0';
    token->text_length = text_length;
    return 1;
}
