/**
 * @file
 * @brief Testy číselných literálů, jejich hodnot, hranic a zdrojových pozic.
 */
/* AI prompt: Ověř Int a Double, podtržítka mezi číslicemi,
 * exponenty, přípony d/D, zápis .5, úvodní nuly, chyby zápisu a sousední
 * operátory. Zkontroluj taky pozice původních lexémů a samostatné _.
 * Model: GPT-6, Codex.
 */
#include "../../src/lexer.h"

#include <assert.h>
#include <limits.h>
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

static void expect_number(const char *text, TokenType type, long long integer, double decimal) {
    FILE *input = input_from_text(text);
    Lexer lexer = {0};
    const LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);

    read_token(&lexer, &token, type);
    assert(token.line == 1 && token.column == 1);
    assert(token.end_line == 1 && token.end_column == strlen(text) + 1);
    if (type == TOK_INT_LITERAL) {
        assert(token.data.num_int_val == integer);
    } else {
        assert(token.data.num_float_val == decimal);
    }
    token_free(&token);
    read_token(&lexer, &token, TOK_EOF);
    assert(token.line == 1 && token.column == strlen(text) + 1);
    token_free(&token);
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_integers(void) {
    const struct {
        const char *text;
        long long value;
    } cases[] = {
        {"0", 0},
        {"7", 7},
        {"123", 123},
        {"1_2", 12},
        {"1_000_000", 1000000},
        {"9_8_7", 987},
        {"00", 0},
        {"0_0", 0},
        {"9223372036854775807", LLONG_MAX},
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        expect_number(cases[i].text, TOK_INT_LITERAL, cases[i].value, 0.0);
    }
}

static void test_decimals(void) {
    const struct {
        const char *text;
        double value;
    } cases[] = {
        {"0.0", 0.0},       {"1.5", 1.5},         {".5", 0.5},          {".0", 0.0},
        {"1e3", 1000.0},    {"1E+3", 1000.0},     {"1e-3", 0.001},      {".5e2", 50.0},
        {"1_2.5_0", 12.5},  {"1_0e+0_2", 1000.0}, {"1.5d", 1.5},        {"1.5D", 1.5},
        {".5D", 0.5},       {"1e3D", 1000.0},     {"1_2.5e+1D", 125.0}, {"01.5", 1.5},
        {"0.0_0e-0D", 0.0},
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        expect_number(cases[i].text, TOK_DOUBLE_LITERAL, 0, cases[i].value);
    }
}

static void test_invalid_numbers(void) {
    const char *inputs[] = {
        "01",   "01_0",  "1.",    "1.e2",  "1e",    "1e+",    "1e-",   "1e--2", "1e+-2",
        "1_",   "1__2",  "1_.5",  "1._5",  "1_e2",  "1e_2",   "1e+_2", "1e2_",  "1.2.3",
        ".5.6", "1e2.3", "1e2e3", "1.0dd", "1.0d2", "1.0De2", "1d",    "1D",    "1f",
        "1.0F", "1L",    "0x10",  "0b10",  "2abc",  "1e2abc", "_",
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

static void test_operators_and_exponent_signs(void) {
    FILE *input = input_from_text("-1e+2-.5+3_0/2.5D*4");
    const TokenType expected[] = {
        TOK_MINUS, TOK_DOUBLE_LITERAL, TOK_MINUS, TOK_DOUBLE_LITERAL, TOK_PLUS, TOK_INT_LITERAL,
        TOK_SLASH, TOK_DOUBLE_LITERAL, TOK_STAR,  TOK_INT_LITERAL,    TOK_EOF,
    };
    const double values[] = {100.0, 0.5, 30.0, 2.5, 4.0};
    size_t value_index = 0;
    Lexer lexer = {0};
    const LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);
    for (size_t i = 0; i < sizeof expected / sizeof *expected; ++i) {
        read_token(&lexer, &token, expected[i]);
        if (token.type == TOK_INT_LITERAL) {
            assert(token.data.num_int_val == (long long)values[value_index]);
            value_index++;
        } else if (token.type == TOK_DOUBLE_LITERAL) {
            assert(token.data.num_float_val == values[value_index]);
            value_index++;
        }
        token_free(&token);
    }
    assert(value_index == sizeof values / sizeof *values);
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_dots_and_identifiers(void) {
    FILE *input = input_from_text("ifj26.* . .. .5 _x _1 __");
    const TokenType expected[] = {
        TOK_IDENTIFIER,     TOK_DOT,        TOK_STAR,       TOK_DOT,        TOK_DOT, TOK_DOT,
        TOK_DOUBLE_LITERAL, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_EOF,
    };
    Lexer lexer = {0};
    const LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);
    for (size_t i = 0; i < sizeof expected / sizeof *expected; ++i) {
        read_token(&lexer, &token, expected[i]);
        if (token.type == TOK_DOUBLE_LITERAL) {
            assert(token.data.num_float_val == 0.5);
        } else if (i == 7) {
            assert(strcmp(token.data.text, "_x") == 0);
        } else if (i == 8) {
            assert(strcmp(token.data.text, "_1") == 0);
        } else if (i == 9) {
            assert(strcmp(token.data.text, "__") == 0);
        }
        token_free(&token);
    }
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_source_positions(void) {
    FILE *input = input_from_text("a = 1_000.5D\nb=.5\n.\nx");
    const TokenType expected[] = {
        TOK_IDENTIFIER, TOK_ASSIGN,  TOK_DOUBLE_LITERAL, TOK_NEWLINE,
        TOK_IDENTIFIER, TOK_ASSIGN,  TOK_DOUBLE_LITERAL, TOK_NEWLINE,
        TOK_DOT,        TOK_NEWLINE, TOK_IDENTIFIER,     TOK_EOF,
    };
    const size_t lines[] = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4};
    const size_t columns[] = {1, 3, 5, 1, 1, 2, 3, 1, 1, 1, 1, 2};
    Lexer lexer = {0};
    const LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);
    for (size_t i = 0; i < sizeof expected / sizeof *expected; ++i) {
        read_token(&lexer, &token, expected[i]);
        assert(token.line == lines[i] && token.column == columns[i]);
        if (i == 2) {
            assert(token.end_line == 1 && token.end_column == 13);
            assert(token.data.num_float_val == 1000.5);
        } else if (i == 6) {
            assert(token.end_line == 2 && token.end_column == 5);
        } else if (i == 8) {
            assert(token.end_line == 3 && token.end_column == 2);
        }
        token_free(&token);
    }
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_long_number(void) {
    char text[160] = "1";
    memset(text + 1, '0', 100);
    memcpy(text + 101, "e-100", 6);
    expect_number(text, TOK_DOUBLE_LITERAL, 0, 1.0);
}

int main(void) {
    test_integers();
    test_decimals();
    test_invalid_numbers();
    test_operators_and_exponent_signs();
    test_dots_and_identifiers();
    test_source_positions();
    test_long_number();
    return 0;
}
