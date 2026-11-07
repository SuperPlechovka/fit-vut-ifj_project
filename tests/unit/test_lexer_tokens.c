/**
 * @file
 * @brief Unit testy tokenizace aktuálně implementovaného lexeru.
 */
/* AI prompt: vytvoř unit testy pro lexer podle src/lexer.h a src/token.h.
 * Ověř:
 * - vstup "abc def Int\n" - TOK_IDENTIFIER, TOK_KW_DEF, TOK_KW_INT, TOK_NEWLINE, TOK_EOF;
 * - vstup "12 3.5 \"a\\n\" == != <= >= = < > + - * / ( ) , : ; . @\n" - TOK_INT_LITERAL,
 * TOK_DOUBLE_LITERAL, TOK_STRING_LITERAL, operátory a interpunkci;
 * - vstup "a\n  b\n    c\nd" - správné pořadí TOK_INDENT a TOK_DEDENT;
 * - vstupy "2abc", "1.2.3", "a\n  b\n c" a "a\n\tb" - LEXER_ERROR;
 * - prázdný vstup - TOK_EOF;
 * - prázdný řetězec "" - TOK_STRING_LITERAL s délkou 0;
 * - řádkový komentář "a // komentář\nb" - TOK_IDENTIFIER, TOK_NEWLINE, TOK_IDENTIFIER, TOK_EOF;
 * - neplatný escape "\\q" - LEXER_ERROR;
 * - neuzavřený řetězec "\"abc" - LEXER_ERROR;
 * - samotný znak "!" - LEXER_ERROR;
 * - číslo "1_2" - LEXER_ERROR;
 * - prázdný řádek "a\n\nb\n" - odsazení se nezmění;
 * - opakované volání po TOK_EOF - vždy TOK_EOF.
 * Model: GPT-6, Codex.
 */
#include "../../src/lexer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static FILE *input_from_text(const char *text) {
    FILE *input = tmpfile();
    assert(input != NULL);
    assert(fputs(text, input) >= 0);
    rewind(input);
    return input;
}

static LexerResult read_token(Lexer *lexer, Token *token, TokenType expected) {
    LexerResult result = next_token(lexer, token);
    assert(result == LEXER_OK);
    if (token->type != expected) {
        fprintf(stderr, "expected %s, got %s\n", token_type_name(expected),
                token_type_name(token->type));
    }
    assert(token->type == expected);
    return result;
}

static void test_identifiers_keywords_and_positions(void) {
    FILE *input = input_from_text("abc def Int\n");
    Lexer lexer = {0};
    Token token;
    token_init(&token);
    assert(lexer_init(&lexer, input) == LEXER_OK);

    read_token(&lexer, &token, TOK_IDENTIFIER);
    assert(strcmp(token.data.text, "abc") == 0);
    assert(token.line == 1 && token.column == 1 && token.end_column == 4);
    token_free(&token);
    read_token(&lexer, &token, TOK_KW_DEF);
    assert(token.line == 1 && token.column == 5 && token.end_column == 8);
    assert(token.data.text == NULL);
    token_free(&token);
    read_token(&lexer, &token, TOK_KW_INT);
    assert(token.line == 1 && token.column == 9 && token.end_column == 12);
    token_free(&token);
    read_token(&lexer, &token, TOK_NEWLINE);
    token_free(&token);
    read_token(&lexer, &token, TOK_EOF);

    token_free(&token);
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_literals_and_operators(void) {
    FILE *input = input_from_text("12 3.5 \"a\\n\" == != <= >= = < > + - * / ( ) , : ; . @\n");
    const TokenType expected[] = {TOK_INT_LITERAL,
                                  TOK_DOUBLE_LITERAL,
                                  TOK_STRING_LITERAL,
                                  TOK_EQ,
                                  TOK_NEQ,
                                  TOK_LE,
                                  TOK_GE,
                                  TOK_ASSIGN,
                                  TOK_LT,
                                  TOK_GT,
                                  TOK_PLUS,
                                  TOK_MINUS,
                                  TOK_STAR,
                                  TOK_SLASH,
                                  TOK_LPAREN,
                                  TOK_RPAREN,
                                  TOK_COMMA,
                                  TOK_COLON,
                                  TOK_SEMICOLON,
                                  TOK_DOT,
                                  TOK_AT,
                                  TOK_NEWLINE,
                                  TOK_EOF};
    Lexer lexer = {0};
    Token token;
    token_init(&token);
    assert(lexer_init(&lexer, input) == LEXER_OK);
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        read_token(&lexer, &token, expected[i]);
        if (expected[i] == TOK_INT_LITERAL)
            assert(token.data.num_int_val == 12);
        if (expected[i] == TOK_DOUBLE_LITERAL)
            assert(token.data.num_float_val == 3.5);
        if (expected[i] == TOK_STRING_LITERAL)
            assert(strcmp(token.data.text, "a\n") == 0);
        token_free(&token);
    }
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_indent_dedent_and_repeated_eof(void) {
    FILE *input = input_from_text("a\n  b\n    c\nd");
    const TokenType expected[] = {TOK_IDENTIFIER, TOK_NEWLINE, TOK_INDENT,     TOK_IDENTIFIER,
                                  TOK_NEWLINE,    TOK_INDENT,  TOK_IDENTIFIER, TOK_NEWLINE,
                                  TOK_DEDENT,     TOK_DEDENT,  TOK_IDENTIFIER, TOK_EOF};
    Lexer lexer = {0};
    Token token;
    token_init(&token);
    assert(lexer_init(&lexer, input) == LEXER_OK);
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        read_token(&lexer, &token, expected[i]);
        token_free(&token);
    }
    read_token(&lexer, &token, TOK_EOF);
    token_free(&token);
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_lexical_errors(void) {
    const char *invalid_inputs[] = {"2abc", "1.2.3", "a\n  b\n c", "a\n\tb"};
    for (size_t i = 0; i < sizeof(invalid_inputs) / sizeof(invalid_inputs[0]); ++i) {
        FILE *input = input_from_text(invalid_inputs[i]);
        Lexer lexer = {0};
        Token token;
        token_init(&token);
        assert(lexer_init(&lexer, input) == LEXER_OK);
        LexerResult result = LEXER_OK;
        while ((result = next_token(&lexer, &token)) == LEXER_OK && token.type != TOK_EOF) {
            token_free(&token);
        }
        assert(result == LEXER_ERROR);
        token_free(&token);
        lexer_destroy(&lexer);
        fclose(input);
    }
}

static void test_edge_cases(void) {
    const char *invalid_inputs[] = {"\"\\q\"", "\"abc", "!", "1_2"};
    const char *comment_input = "a // komentář\nb";
    FILE *input = input_from_text("");
    Lexer lexer = {0};
    Token token;
    token_init(&token);
    assert(lexer_init(&lexer, input) == LEXER_OK);
    read_token(&lexer, &token, TOK_EOF);
    token_free(&token);
    lexer_destroy(&lexer);
    fclose(input);

    input = input_from_text("\"\"");
    assert(lexer_init(&lexer, input) == LEXER_OK);
    read_token(&lexer, &token, TOK_STRING_LITERAL);
    assert(token.text_length == 0);
    token_free(&token);
    lexer_destroy(&lexer);
    fclose(input);

    input = input_from_text(comment_input);
    assert(lexer_init(&lexer, input) == LEXER_OK);
    read_token(&lexer, &token, TOK_IDENTIFIER);
    token_free(&token);
    read_token(&lexer, &token, TOK_NEWLINE);
    token_free(&token);
    read_token(&lexer, &token, TOK_IDENTIFIER);
    token_free(&token);
    read_token(&lexer, &token, TOK_EOF);
    token_free(&token);
    lexer_destroy(&lexer);
    fclose(input);

    for (size_t i = 0; i < sizeof(invalid_inputs) / sizeof(invalid_inputs[0]); ++i) {
        input = input_from_text(invalid_inputs[i]);
        assert(lexer_init(&lexer, input) == LEXER_OK);
        assert(next_token(&lexer, &token) == LEXER_ERROR);
        token_free(&token);
        lexer_destroy(&lexer);
        fclose(input);
    }

    input = input_from_text("a\n\nb\n");
    assert(lexer_init(&lexer, input) == LEXER_OK);
    read_token(&lexer, &token, TOK_IDENTIFIER);
    token_free(&token);
    read_token(&lexer, &token, TOK_NEWLINE);
    token_free(&token);
    read_token(&lexer, &token, TOK_NEWLINE);
    token_free(&token);
    read_token(&lexer, &token, TOK_IDENTIFIER);
    token_free(&token);
    read_token(&lexer, &token, TOK_NEWLINE);
    token_free(&token);
    read_token(&lexer, &token, TOK_EOF);
    token_free(&token);
    lexer_destroy(&lexer);
    fclose(input);
}

int main(void) {
    test_identifiers_keywords_and_positions();
    test_literals_and_operators();
    test_indent_dedent_and_repeated_eof();
    test_lexical_errors();
    test_edge_cases();
    return 0;
}
