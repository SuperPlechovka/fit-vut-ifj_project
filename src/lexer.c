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
static int check_digit_sequence(const char *text, size_t *position);
static LexerResult finish_number(Lexer *lexer, Token *token, size_t line, size_t column);
static LexerResult read_multiline_string(Lexer *lexer, Token *token, size_t line, size_t column);
static LexerResult handle_line_start(Lexer *lexer, Token *token, int first_character);
static LexerResult read_line_comment(Lexer *lexer, Token *token);
static LexerResult read_block_comment(Lexer *lexer, Token *token);
static LexerResult finish_input(Lexer *lexer, Token *token);

/** @brief Stavy automatu. */
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
    lexer->previous_column = 1;
    lexer->pending_slash = 0;
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
    if (lexer->pending_dedents > 0) {
        const int set_result = set_token(token, TOK_DEDENT, (TokenData){0}, NULL, 0, lexer->line,
                                         lexer->column, lexer->line, lexer->column);
        if (!set_result) {
            return LEXER_INTERNAL_ERROR;
        }
        lexer->pending_dedents--;
        return LEXER_OK;
    }
    if (lexer->pending_slash) {
        const int set_result = set_token(token, TOK_SLASH, (TokenData){0}, NULL, 0, lexer->line,
                                         lexer->column - 1, lexer->line, lexer->column);
        if (!set_result) {
            return LEXER_INTERNAL_ERROR;
        }
        lexer->pending_slash = 0;
        return LEXER_OK;
    }
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
    int string_start = 0;

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
                const LexerResult result = finish_input(lexer, token);
                if (result != LEXER_OK) {
                    return result;
                }
                state = SCAN_DONE;
            } else if (character == ' ' || character == '\t' || character == '\r') {
                continue;
            } else if (character == '\n') {
                lexer->at_line_start = 1;
                if (lexer->parenthesis_depth > 0) {
                    continue;
                }
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
                const int append_result = append_lexem(lexer, (char)character);
                if (!append_result) {
                    return LEXER_INTERNAL_ERROR;
                }
                state = SCAN_NUMBER;
            } else if (character == '"') {
                token_line = lexer->line;
                token_column = lexer->column - 1;
                string_start = 1;
                state = SCAN_STRING;
            } else if (character == '/') {
                int next_character;
                const int next_read_result = read_char(lexer, &next_character);
                if (!next_read_result) {
                    return LEXER_INTERNAL_ERROR;
                }
                if (next_character == '/') {
                    const LexerResult result = read_line_comment(lexer, token);
                    if (result != LEXER_OK || token->type == TOK_NEWLINE ||
                        token->type == TOK_DEDENT || token->type == TOK_EOF) {
                        return result;
                    }
                    continue;
                }
                if (next_character == '*') {
                    const LexerResult result = read_block_comment(lexer, token);
                    if (result != LEXER_OK || token->type == TOK_NEWLINE) {
                        return result;
                    }
                    continue;
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
                    lexer->parenthesis_depth++;
                    break;
                case ')':
                    type = TOK_RPAREN;
                    if (lexer->parenthesis_depth > 0) {
                        lexer->parenthesis_depth--;
                    }
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
                case '.': {
                    token_line = lexer->line;
                    token_column = lexer->column - 1;
                    int next_character;
                    const int next_read_result = read_char(lexer, &next_character);
                    if (!next_read_result) {
                        return LEXER_INTERNAL_ERROR;
                    }
                    if (next_character != EOF && !unread_char(lexer, next_character)) {
                        return LEXER_INTERNAL_ERROR;
                    }
                    if (is_digit(next_character)) {
                        const int append_result = append_lexem(lexer, '.');
                        if (!append_result) {
                            return LEXER_INTERNAL_ERROR;
                        }
                        state = SCAN_NUMBER;
                        continue;
                    }
                    type = TOK_DOT;
                    break;
                }
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
            if (token->type == TOK_INDENT || token->type == TOK_DEDENT ||
                token->type == TOK_SLASH || token->type == TOK_EOF) {
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
                    return LEXER_INTERNAL_ERROR;
            } else {
                unread_char(lexer, character);
                if (lexer->lexem_length == 1 && lexer->lexem[0] == '_') {
                    state = SCAN_ERROR;
                    break;
                }
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
                    return LEXER_INTERNAL_ERROR;
                } else {
                    state = SCAN_DONE;
                }
            }
            break;

        case SCAN_NUMBER: {
            const char previous = lexer->lexem[lexer->lexem_length - 1];
            const int exponent_sign =
                (character == '+' || character == '-') && (previous == 'e' || previous == 'E');
            if (is_digit(character) || character == '.' || character == '_' || character == 'e' ||
                character == 'E' || character == 'd' || character == 'D' || exponent_sign) {
                const int append_result = append_lexem(lexer, (char)character);
                if (!append_result) {
                    return LEXER_INTERNAL_ERROR;
                }
            } else {
                unread_char(lexer, character);
                if (is_letter(character)) {
                    state = SCAN_ERROR;
                    break;
                }
                const LexerResult result = finish_number(lexer, token, token_line, token_column);
                if (result == LEXER_ERROR) {
                    state = SCAN_ERROR;
                    break;
                }
                if (result != LEXER_OK) {
                    return result;
                }
                state = SCAN_DONE;
            }
        } break;

        case SCAN_STRING:
            if (string_start) {
                string_start = 0;
                if (character == '"') {
                    int next_character;
                    const int next_read_result = read_char(lexer, &next_character);
                    if (!next_read_result) {
                        return LEXER_INTERNAL_ERROR;
                    }
                    if (next_character == '"') {
                        const LexerResult result =
                            read_multiline_string(lexer, token, token_line, token_column);
                        if (result == LEXER_ERROR) {
                            state = SCAN_ERROR;
                            break;
                        }
                        if (result != LEXER_OK) {
                            return result;
                        }
                        state = SCAN_DONE;
                        break;
                    }
                    if (next_character != EOF && !unread_char(lexer, next_character)) {
                        return LEXER_INTERNAL_ERROR;
                    }
                }
            }
            if (character == '"') {
                set_token_ok = set_token(token, TOK_STRING_LITERAL, (TokenData){0}, lexer->lexem,
                                         lexer->lexem_length, token_line, token_column, lexer->line,
                                         lexer->column);
                if (!set_token_ok) {
                    return LEXER_INTERNAL_ERROR;
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
                        return LEXER_INTERNAL_ERROR;
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
                    return LEXER_INTERNAL_ERROR;
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

/** @brief Rozpozná jestli je znak písmeno.
 * @param[in] character Testovaný znak.
 * @return Nenulová hodnota pro písmeno, jinak 0.
 */
static int is_letter(const int character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
}

/** @brief Rozpozná jestli je znak číslice.
 * @param[in] character Testovaný znak.
 * @return Nenulová hodnota pro číslici, jinak 0.
 */
static int is_digit(const int character) { return character >= '0' && character <= '9'; }

/** @brief Rozpozná jestli znak je přípustný uvnitř identifikátoru.
 * @param[in] character Testovaný znak.
 * @return Nenulová hodnota pro písmeno, číslici nebo podtržítko, jinak 0.
 */
static int is_identifier_character(const int character) {
    return is_letter(character) || is_digit(character) || character == '_';
}

/** @brief Přeskočí neprázdnou posloupnost číslic s případnými podtržítky.
 * @param[in] text Číselný zápis ukončený nulou.
 * @param[in,out] position Začátek posloupnosti, při úspěchu její konec.
 * @return 1 pro platnou posloupnost, jinak 0.
 */
static int check_digit_sequence(const char *text, size_t *position) {
    if (!is_digit(text[*position])) {
        return 0;
    }
    while (is_digit(text[*position]) || text[*position] == '_') {
        if (text[*position] == '_' &&
            (!is_digit(text[*position - 1]) || !is_digit(text[*position + 1]))) {
            return 0;
        }
        (*position)++;
    }
    return 1;
}

/** @brief Ověří celý číselný zápis, odstraní oddělovače, nastaví hodnotu tokenu.
 * @param[in,out] lexer Lexer s načteným číselným lexémem.
 * @param[out] token Celočíselný nebo desetinný token
 * @param[in] line Počáteční řádek čísla
 * @param[in] column Počáteční sloupec čísla.
 * @retval LEXER_OK Token nastaven.
 * @retval LEXER_ERROR Neplatný zápis.
 * @retval LEXER_INTERNAL_ERROR Selhání vytváření tokenu.
 */
static LexerResult finish_number(Lexer *lexer, Token *token, const size_t line,
                                 const size_t column) {
    size_t position = 0;
    int is_float = 0;
    if (lexer->lexem[position] != '.' && !check_digit_sequence(lexer->lexem, &position)) {
        return LEXER_ERROR;
    }
    if (lexer->lexem[position] == '.') {
        is_float = 1;
        position++;
        if (!check_digit_sequence(lexer->lexem, &position)) {
            return LEXER_ERROR;
        }
    }
    if (lexer->lexem[position] == 'e' || lexer->lexem[position] == 'E') {
        is_float = 1;
        position++;
        if (lexer->lexem[position] == '+' || lexer->lexem[position] == '-') {
            position++;
        }
        if (!check_digit_sequence(lexer->lexem, &position)) {
            return LEXER_ERROR;
        }
    }
    if (is_float && (lexer->lexem[position] == 'd' || lexer->lexem[position] == 'D')) {
        position++;
    }
    if (lexer->lexem[position] != '\0') {
        return LEXER_ERROR;
    }

    const size_t source_length = lexer->lexem_length;
    size_t length = 0;
    for (size_t i = 0; i < source_length; ++i) {
        if (lexer->lexem[i] != '_') {
            lexer->lexem[length] = lexer->lexem[i];
            length++;
        }
    }
    if (is_float && (lexer->lexem[length - 1] == 'd' || lexer->lexem[length - 1] == 'D')) {
        length--;
    }
    lexer->lexem[length] = '\0';
    lexer->lexem_length = length;

    TokenData data = {0};
    TokenType type;
    char *end;
    if (is_float) {
        type = TOK_DOUBLE_LITERAL;
        data.num_float_val = strtod(lexer->lexem, &end);
    } else {
        type = TOK_INT_LITERAL;
        data.num_int_val = strtoll(lexer->lexem, &end, 10);
        if (lexer->lexem[0] == '0' && data.num_int_val != 0) {
            return LEXER_ERROR;
        }
    }
    if (*end != '\0') {
        return LEXER_ERROR;
    }
    const int set_result =
        set_token(token, type, data, NULL, 0, line, column, line, column + source_length);
    if (!set_result) {
        return LEXER_INTERNAL_ERROR;
    }
    return LEXER_OK;
}

/** @brief Načte text mezi trojitými uvozovkami a uloží ho do tokenu.
 * @param[in,out] lexer Stav lexeru a buffer pro načítaný text.
 * @param[out] token Uložený řetězec bez trojitých uvozovek.
 * @param[in] line Řádek první uvozovky.
 * @param[in] column Sloupec první uvozovky.
 * @retval LEXER_OK Řetězec se načetl celý.
 * @retval LEXER_ERROR Vstup skončil před uzavíracími uvozovkami.
 * @retval LEXER_INTERNAL_ERROR Nepodařilo se číst nebo alokovat paměť
 */
static LexerResult read_multiline_string(Lexer *lexer, Token *token, const size_t line,
                                         const size_t column) {
    size_t quotes = 0;
    while (1) {
        int character;
        const int read_result = read_char(lexer, &character);
        if (!read_result) {
            return LEXER_INTERNAL_ERROR;
        }
        if (character == EOF) {
            return LEXER_ERROR;
        }
        if (character == '"') {
            quotes++;
            if (quotes == 3) {
                const int set_result =
                    set_token(token, TOK_STRING_LITERAL, (TokenData){0}, lexer->lexem,
                              lexer->lexem_length, line, column, lexer->line, lexer->column);
                if (!set_result) {
                    return LEXER_INTERNAL_ERROR;
                }
                return LEXER_OK;
            }
            continue;
        }
        for (size_t i = 0; i < quotes; ++i) {
            const int append_result = append_lexem(lexer, '"');
            if (!append_result) {
                return LEXER_INTERNAL_ERROR;
            }
        }
        quotes = 0;
        const int append_result = append_lexem(lexer, (char)character);
        if (!append_result) {
            return LEXER_INTERNAL_ERROR;
        }
    }
}

/** @brief Načte znak a posune pozici lexeru.
 * @param[in,out] lexer Inicializovaný lexer.
 * @param[out] character Načtený znak nebo EOF.
 * @return 1 při platných ukazatelích, jinak 0.
 */
static int read_char(Lexer *lexer, int *character) {
    if (lexer == NULL || character == NULL) {
        return 0;
    }
    lexer->previous_column = lexer->column;
    *character = fgetc(lexer->input);
    if (*character == EOF) {
        return !ferror(lexer->input);
    }
    if (*character == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return 1;
}

/** @brief Zpracuje odsazení a případně vydá INDENT nebo DEDENT
 * @param[in,out] lexer Inicializovaný lexer.
 * @param[out] token Token změny odsazení, pokud nastala.
 * @param[in] first_character První načtený znak řádku.
 * @retval LEXER_OK Odsazení zpracováno.
 * @retval LEXER_ERROR Tabulátor na začátku řádku nebo neplatná úroveň odsazení.
 * @retval LEXER_INTERNAL_ERROR Selhání alokace nebo pomocné operace.
 */
static LexerResult handle_line_start(Lexer *lexer, Token *token, const int first_character) {
    size_t indent = 0;
    int character = first_character;
    int has_tab = 0;
    int after_comment = 0;
    while (1) {
        while (character == ' ' || character == '\t' || character == '\r') {
            if (character == ' ' && !after_comment) {
                indent++;
            } else if (character == '\t') {
                has_tab = 1;
            }
            const int read_result = read_char(lexer, &character);
            if (!read_result) {
                return LEXER_INTERNAL_ERROR;
            }
        }
        if (character == EOF) {
            lexer->at_line_start = 0;
            return finish_input(lexer, token);
        }
        if (character == '\n') {
            const int read_result = read_char(lexer, &character);
            if (!read_result) {
                return LEXER_INTERNAL_ERROR;
            }
            indent = 0;
            has_tab = 0;
            after_comment = 0;
            continue;
        }
        if (character == '/') {
            int next_character;
            const int read_result = read_char(lexer, &next_character);
            if (!read_result) {
                return LEXER_INTERNAL_ERROR;
            }
            if (next_character == '/' || next_character == '*') {
                const size_t comment_line = lexer->line;
                LexerResult result;
                if (next_character == '/') {
                    result = read_line_comment(lexer, token);
                } else {
                    result = read_block_comment(lexer, token);
                }
                if (result != LEXER_OK || token->type == TOK_DEDENT || token->type == TOK_EOF) {
                    return result;
                }
                token_init(token);
                if (lexer->line != comment_line) {
                    indent = 0;
                    has_tab = 0;
                    after_comment = 0;
                } else {
                    after_comment = 1;
                }
                const int next_read_result = read_char(lexer, &character);
                if (!next_read_result) {
                    return LEXER_INTERNAL_ERROR;
                }
                continue;
            }
            if (next_character != EOF && !unread_char(lexer, next_character)) {
                return LEXER_INTERNAL_ERROR;
            }
            lexer->pending_slash = 1;
        } else if (!unread_char(lexer, character)) {
            return LEXER_INTERNAL_ERROR;
        }
        if (has_tab) {
            return LEXER_ERROR;
        }
        break;
    }

    lexer->at_line_start = 0;
    const size_t previous_indent = lexer->indents[lexer->indent_depth - 1];
    if (lexer->parenthesis_depth > 0) {
        if (indent <= previous_indent) {
            return LEXER_ERROR;
        }
        indent = previous_indent;
    }
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
        const int set_result =
            set_token(token, TOK_DEDENT, (TokenData){0}, NULL, 0, lexer->line, 1, lexer->line, 1);
        if (!set_result) {
            return LEXER_INTERNAL_ERROR;
        }
        return LEXER_OK;
    }
    if (lexer->pending_slash) {
        const int set_result = set_token(token, TOK_SLASH, (TokenData){0}, NULL, 0, lexer->line,
                                         lexer->column - 1, lexer->line, lexer->column);
        if (!set_result) {
            return LEXER_INTERNAL_ERROR;
        }
        lexer->pending_slash = 0;
    }
    return LEXER_OK;
}

/** @brief Přeskočí řádkový komentář po úvodních lomítkách.
 * @param[in,out] lexer Inicializovaný lexer.
 * @param[out] token Token NEWLINE mimo závorky nebo token EOF na konci vstupu.
 * @retval LEXER_OK Úspěšně přeskočil komentář.
 * @retval LEXER_INTERNAL_ERROR Selhání vytvoření tokenu.
 */
static LexerResult read_line_comment(Lexer *lexer, Token *token) {
    int character;
    int read_result = read_char(lexer, &character);
    while (read_result && character != '\n' && character != EOF) {
        read_result = read_char(lexer, &character);
    }
    if (!read_result) {
        return LEXER_INTERNAL_ERROR;
    }
    if (character == '\n') {
        lexer->at_line_start = 1;
        if (lexer->parenthesis_depth > 0) {
            return LEXER_OK;
        }
        const int set_result = set_token(token, TOK_NEWLINE, (TokenData){0}, NULL, 0,
                                         lexer->line - 1, 1, lexer->line, 1);
        if (!set_result) {
            return LEXER_INTERNAL_ERROR;
        }
        return LEXER_OK;
    }
    return finish_input(lexer, token);
}

/** @brief Přeskočí blokový komentář i komentáře vnořené v něj
 * @param[in,out] lexer Lexer, ze kterého se čte.
 * @param[out] token TOK_NEWLINE pokud komentář obsahuje nový řádek a je mimo závorky.
 * @retval LEXER_OK Komentář přeskočen.
 * @retval LEXER_ERROR Chybí uzavření nebo za víceřádkovým komentářem následuje kód.
 * @retval LEXER_INTERNAL_ERROR Chyba při čtení nebo vytvoření tokenu.
 */
static LexerResult read_block_comment(Lexer *lexer, Token *token) {
    size_t depth = 1;
    size_t newline_line = 0;
    int previous = 0;
    while (depth > 0) {
        int character;
        const int read_result = read_char(lexer, &character);
        if (!read_result) {
            return LEXER_INTERNAL_ERROR;
        }
        if (character == EOF) {
            return LEXER_ERROR;
        }
        if (character == '\n' && newline_line == 0) {
            newline_line = lexer->line - 1;
        }
        if (previous == '/' && character == '*') {
            depth++;
            previous = 0;
        } else if (previous == '*' && character == '/') {
            depth--;
            previous = 0;
        } else {
            previous = character;
        }
    }
    if (newline_line == 0) {
        return LEXER_OK;
    }

    int character;
    int read_result = read_char(lexer, &character);
    while (read_result && (character == ' ' || character == '\t' || character == '\r')) {
        read_result = read_char(lexer, &character);
    }
    if (!read_result) {
        return LEXER_INTERNAL_ERROR;
    }
    if (character != '\n' && character != EOF) {
        return LEXER_ERROR;
    }
    lexer->at_line_start = 1;
    if (lexer->parenthesis_depth > 0) {
        return LEXER_OK;
    }
    const int set_result = set_token(token, TOK_NEWLINE, (TokenData){0}, NULL, 0, newline_line, 1,
                                     lexer->line, lexer->column);
    if (!set_result) {
        return LEXER_INTERNAL_ERROR;
    }
    return LEXER_OK;
}

/** @brief Na konci vstupu postupně uzavře bloky a pak vydá EOF.
 * @param[in,out] lexer Inicializovaný lexer.
 * @param[out] token Token DEDENT nebo EOF.
 * @retval LEXER_OK Token nastaven.
 * @retval LEXER_INTERNAL_ERROR Selhání vytvoření tokenu.
 */
static LexerResult finish_input(Lexer *lexer, Token *token) {
    TokenType type = TOK_EOF;
    if (lexer->indent_depth > 1) {
        lexer->indent_depth--;
        type = TOK_DEDENT;
    }
    const int set_result = set_token(token, type, (TokenData){0}, NULL, 0, lexer->line,
                                     lexer->column, lexer->line, lexer->column);
    if (!set_result) {
        return LEXER_INTERNAL_ERROR;
    }
    if (type == TOK_EOF) {
        lexer->eof_emitted = 1;
    }
    return LEXER_OK;
}

/** @brief Vrátí znak do vstupu a upraví pozici.
 * @param[in,out] lexer Inicializovaný lexer.
 * @param[in] character Vrácený znak, EOF se nevrací
 * @return 1 při úspěchu, jinak 0
 */
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
        lexer->column = lexer->previous_column;
    } else if (lexer->column > 1) {
        lexer->column--;
    }
    return 1;
}

/** @brief Připojí znak k lexému a podle potřeby zvětší buffer.
 * @param[in,out] lexer Inicializovaný lexer.
 * @param[in] character Připojený znak.
 * @return 1 při úspěchu, 0 při neplatném lexeru nebo selhání alokace.
 */
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

/** @brief Vyprázdní lexém.
 * @param[in,out] lexer Inicializovaný lexer, případně NULL.
 */
static void reset_lexem(Lexer *lexer) {
    if (lexer != NULL) {
        lexer->lexem_length = 0;
        if (lexer->lexem != NULL) {
            lexer->lexem[0] = '\0';
        }
    }
}

/** @brief Určí, zda je text klíčové slovo.
 * @param[in] text Text ukončený nulou
 * @param[in] length Délka textu bez ukončovací nuly.
 * @return Druh klíčového slova, jinak TOK_IDENTIFIER.
 */
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

/** @brief Nastaví token a případný text zkopíruje do paměti.
 * @param[out] token Token bez vlastněného textu.
 * @param[in] type Druh tokenu.
 * @param[in] data Hodnota tokenu.
 * @param[in] text Kopírovaný text, NULL pro token bez textu.
 * @param[in] text_length Počet kopírovaných bajtů.
 * @param[in] line Počáteční řádek.
 * @param[in] column Počáteční sloupec.
 * @param[in] end_line Koncový řádek.
 * @param[in] end_column Koncový sloupec.
 * @return 1 při úspěchu,0 při selhání alokace.
 */
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
