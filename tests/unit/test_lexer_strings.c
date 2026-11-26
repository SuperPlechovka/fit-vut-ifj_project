/**
 * @file
 * @brief Testy běžných a víceřádkových řetězců, zdrojových pozic a návaznosti tokenů.
 */
/* AI prompt: Ověř trojité uvozovky, doslovný obsah včetně LF a zpětných
 * lomítek, vnitřní jednu/dvě uvozovky, prázdný obsah a chybu při EOF.
 * Ověř běžné escape sekvence, koncové pozice zdrojového zápisu a zachování
 * odsazení i hloubky závorek po víceřádkovém řetězci. Model: GPT-6, Codex.
 */
#include "../../src/lexer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static FILE *input_from_text(const char *text) {
    FILE *input = tmpfile();
    assert(input != NULL);
    const int written = fputs(text, input);
    assert(written >= 0);
    rewind(input);
    return input;
}

static void read_token(Lexer *lexer, Token *token, TokenType expected) {
    const LexerResult result = next_token(lexer, token);
    if (result != LEXER_OK || token->type != expected) {
        fprintf(stderr, "Očekáván %s, výsledek %d, token %s\n", token_type_name(expected), result,
                token_type_name(token->type));
    }
    assert(result == LEXER_OK);
    assert(token->type == expected);
}

static void expect_string(const char *text, const char *value, size_t end_line, size_t end_column) {
    FILE *input = input_from_text(text);
    Lexer lexer = {0};
    const LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);
    read_token(&lexer, &token, TOK_STRING_LITERAL);
    if (strcmp(token.data.text, value) != 0) {
        fprintf(stderr, "Vstup <%s>, očekáván obsah <%s>, skutečný obsah <%s>\n", text, value,
                token.data.text);
    }
    assert(token.text_length == strlen(value));
    assert(strcmp(token.data.text, value) == 0);
    assert(token.line == 1 && token.column == 1);
    assert(token.end_line == end_line && token.end_column == end_column);
    assert(lexer.indent_depth == 1 && lexer.parenthesis_depth == 0);
    token_free(&token);
    read_token(&lexer, &token, TOK_EOF);
    assert(token.line == end_line && token.column == end_column);
    token_free(&token);
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_normal_strings(void) {
    const struct {
        const char *text;
        const char *value;
    } cases[] = {
        {"\"\"", ""},          {"\"abc\"", "abc"}, {"\"a\\nb\"", "a\nb"}, {"\"a\\rb\"", "a\rb"},
        {"\"a\\tb\"", "a\tb"}, {"\"\\\\\"", "\\"}, {"\"\\\"\"", "\""},
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        expect_string(cases[i].text, cases[i].value, 1, strlen(cases[i].text) + 1);
    }
}

static void test_multiline_strings(void) {
    const struct {
        const char *text;
        const char *value;
    } single_line[] = {
        {"\"\"\"\"\"\"", ""},
        {"\"\"\"abc\"\"\"", "abc"},
        {"\"\"\"a\\nb\"\"\"", "a\\nb"},
        {"\"\"\""
         "a\"b\"\"c"
         "\"\"\"",
         "a\"b\"\"c"},
        {"\"\"\""
         "\\q\\t\\r\\\"\\\\"
         "\"\"\"",
         "\\q\\t\\r\\\"\\\\"},
        {"\"\"\"// a /* b */ () #\"\"\"", "// a /* b */ () #"},
    };
    for (size_t i = 0; i < sizeof single_line / sizeof *single_line; ++i) {
        expect_string(single_line[i].text, single_line[i].value, 1,
                      strlen(single_line[i].text) + 1);
    }
    expect_string("\"\"\"a\nb\"\"\"", "a\nb", 2, 5);
    expect_string("\"\"\"\n\"\"\"", "\n", 2, 4);
    expect_string("\"\"\"a\r\nb\"\"\"", "a\r\nb", 2, 5);
    expect_string("\"\"\"\t \n\t\"\"\"", "\t \n\t", 2, 5);
    expect_string("\"\"\"A\n       hoj s\\nvete\n          \"\"\"",
                  "A\n       hoj s\\nvete\n          ", 3, 14);
}

static void test_invalid_strings(void) {
    const char *inputs[] = {
        "\"",          "\"abc",         "\"a\nb\"",       "\"\\q\"",  "\"\\u0041\"",
        "\"\\0\"",     "\"abc\\",       "\"\"\"",         "\"\"\"\"", "\"\"\"abc",
        "\"\"\"abc\"", "\"\"\"abc\"\"", "\"\"\"abc\n  x",
    };
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        FILE *input = input_from_text(inputs[i]);
        Lexer lexer = {0};
        LexerResult result = lexer_init(&lexer, input);
        assert(result == LEXER_OK);
        Token token;
        token_init(&token);
        result = next_token(&lexer, &token);
        if (result != LEXER_ERROR) {
            fprintf(stderr, "Vstup <%s>: očekávána lexikální chyba, výsledek %d\n", inputs[i],
                    result);
        }
        assert(result == LEXER_ERROR);
        token_free(&token);
        lexer_destroy(&lexer);
        fclose(input);
    }
}

static void test_following_tokens_and_positions(void) {
    FILE *input = input_from_text("\"a\\n\" x\n\"\" y\n\"\"\"a\n  b\"\"\" z\nw");
    const TokenType expected[] = {
        TOK_STRING_LITERAL, TOK_IDENTIFIER, TOK_NEWLINE,
        TOK_STRING_LITERAL, TOK_IDENTIFIER, TOK_NEWLINE,
        TOK_STRING_LITERAL, TOK_IDENTIFIER, TOK_NEWLINE,
        TOK_IDENTIFIER,     TOK_EOF,
    };
    const size_t lines[] = {1, 1, 1, 2, 2, 2, 3, 4, 4, 5, 5};
    const size_t columns[] = {1, 7, 1, 1, 4, 1, 1, 8, 1, 1, 2};
    Lexer lexer = {0};
    const LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);
    for (size_t i = 0; i < sizeof expected / sizeof *expected; ++i) {
        read_token(&lexer, &token, expected[i]);
        assert(token.line == lines[i] && token.column == columns[i]);
        if (i == 0) {
            assert(strcmp(token.data.text, "a\n") == 0);
            assert(token.end_line == 1 && token.end_column == 6);
        } else if (i == 3) {
            assert(token.text_length == 0);
            assert(token.end_line == 2 && token.end_column == 3);
        } else if (i == 6) {
            assert(strcmp(token.data.text, "a\n  b") == 0);
            assert(token.end_line == 4 && token.end_column == 7);
        }
        token_free(&token);
    }
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_empty_string_lookahead(void) {
    const char *inputs[] = {"\"\"x", "\"\" x"};
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        FILE *input = input_from_text(inputs[i]);
        Lexer lexer = {0};
        const LexerResult result = lexer_init(&lexer, input);
        assert(result == LEXER_OK);
        Token token;
        token_init(&token);
        read_token(&lexer, &token, TOK_STRING_LITERAL);
        assert(token.text_length == 0 && token.end_column == 3);
        token_free(&token);
        read_token(&lexer, &token, TOK_IDENTIFIER);
        assert(strcmp(token.data.text, "x") == 0);
        token_free(&token);
        read_token(&lexer, &token, TOK_EOF);
        token_free(&token);
        lexer_destroy(&lexer);
        fclose(input);
    }
}

static void test_multiline_layout(void) {
    FILE *input = input_from_text("a\n  \"\"\"first\nno_indent\n      last\"\"\"\n  b\nc\n");
    const TokenType expected[] = {
        TOK_IDENTIFIER, TOK_NEWLINE,    TOK_INDENT,  TOK_STRING_LITERAL,
        TOK_NEWLINE,    TOK_IDENTIFIER, TOK_NEWLINE, TOK_DEDENT,
        TOK_IDENTIFIER, TOK_NEWLINE,    TOK_EOF,
    };
    Lexer lexer = {0};
    const LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);
    for (size_t i = 0; i < sizeof expected / sizeof *expected; ++i) {
        read_token(&lexer, &token, expected[i]);
        if (i == 3) {
            assert(strcmp(token.data.text, "first\nno_indent\n      last") == 0);
            assert(token.line == 2 && token.column == 3);
            assert(token.end_line == 4 && token.end_column == 14);
            assert(lexer.indent_depth == 2);
        }
        token_free(&token);
    }
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_parentheses_in_string(void) {
    FILE *input = input_from_text("f(\"\"\"a\n)\n\"\"\",1)\n");
    const TokenType expected[] = {
        TOK_IDENTIFIER,  TOK_LPAREN, TOK_STRING_LITERAL, TOK_COMMA,
        TOK_INT_LITERAL, TOK_RPAREN, TOK_NEWLINE,        TOK_EOF,
    };
    Lexer lexer = {0};
    const LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);
    for (size_t i = 0; i < sizeof expected / sizeof *expected; ++i) {
        read_token(&lexer, &token, expected[i]);
        if (i == 2) {
            assert(strcmp(token.data.text, "a\n)\n") == 0);
            assert(lexer.parenthesis_depth == 1);
        }
        token_free(&token);
    }
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_long_strings(void) {
    char value[203];
    memset(value, 'a', 100);
    value[100] = '"';
    value[101] = '"';
    memset(value + 102, 'b', 100);
    value[202] = '\0';
    char text[209];
    const int length = snprintf(text, sizeof text, "\"\"\"%s\"\"\"", value);
    assert(length == 208);
    expect_string(text, value, 1, 209);

    memset(value, 'a', 200);
    value[200] = '\0';
    const int normal_length = snprintf(text, sizeof text, "\"%s\"", value);
    assert(normal_length == 202);
    expect_string(text, value, 1, 203);
}

int main(void) {
    test_normal_strings();
    test_multiline_strings();
    test_invalid_strings();
    test_following_tokens_and_positions();
    test_empty_string_lookahead();
    test_multiline_layout();
    test_parentheses_in_string();
    test_long_strings();
    return 0;
}
