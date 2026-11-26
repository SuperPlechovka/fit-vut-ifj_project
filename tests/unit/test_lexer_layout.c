/**
 * @file
 * @brief Unit testy odsazení, logických řádků a konce vstupu.
 * @author Jan Křepinský (xkrepij00@stud.fit.vutbr.cz)
 */
/* AI prompt: Ověř přesné tokeny při návratu do vnějšího bloku, ignorování prázdných
 * a komentářových řádků, uzavření bloků na EOF, tabulátory a odsazení
 * pokračování v závorkách vůči prvnímu fyzickému řádku. Model: GPT-6, Codex.
 */
#include "../../src/lexer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static FILE *input_from_text(const char *text) {
    FILE *input = tmpfile();
    assert(input != NULL);
    int written = fputs(text, input);
    assert(written >= 0);
    rewind(input);
    return input;
}

static void expect_tokens(const char *text, const TokenType *expected, size_t count) {
    FILE *input = input_from_text(text);
    Lexer lexer = {0};
    LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);

    for (size_t i = 0; i < count; i++) {
        result = next_token(&lexer, &token);
        if (result != LEXER_OK || token.type != expected[i]) {
            fprintf(stderr, "Vstup <%s>, token %zu: očekáván %s, výsledek %d, token %s\n", text, i,
                    token_type_name(expected[i]), result, token_type_name(token.type));
        }
        assert(result == LEXER_OK);
        assert(token.type == expected[i]);
        token_free(&token);
    }

    assert(count > 0 && expected[count - 1] == TOK_EOF);
    result = next_token(&lexer, &token);
    assert(result == LEXER_OK && token.type == TOK_EOF);
    token_free(&token);
    lexer_destroy(&lexer);
    fclose(input);
}

static void expect_error(const char *text) {
    FILE *input = input_from_text(text);
    Lexer lexer = {0};
    LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);

    while ((result = next_token(&lexer, &token)) == LEXER_OK && token.type != TOK_EOF) {
        token_free(&token);
    }
    assert(result == LEXER_ERROR);
    token_free(&token);
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_dedent_to_outer_block(void) {
    const TokenType expected[] = {
        TOK_IDENTIFIER, TOK_NEWLINE,    TOK_INDENT,     TOK_IDENTIFIER, TOK_NEWLINE,
        TOK_INDENT,     TOK_IDENTIFIER, TOK_NEWLINE,    TOK_DEDENT,     TOK_IDENTIFIER,
        TOK_NEWLINE,    TOK_DEDENT,     TOK_IDENTIFIER, TOK_EOF,
    };
    expect_tokens("a\n    b\n        c\n    d\ne", expected, sizeof expected / sizeof *expected);

    const TokenType multiple[] = {
        TOK_IDENTIFIER, TOK_NEWLINE,    TOK_INDENT,  TOK_IDENTIFIER, TOK_NEWLINE,    TOK_INDENT,
        TOK_IDENTIFIER, TOK_NEWLINE,    TOK_INDENT,  TOK_IDENTIFIER, TOK_NEWLINE,    TOK_DEDENT,
        TOK_DEDENT,     TOK_IDENTIFIER, TOK_NEWLINE, TOK_DEDENT,     TOK_IDENTIFIER, TOK_EOF,
    };
    expect_tokens("a\n  b\n    c\n      d\n  e\nf", multiple, sizeof multiple / sizeof *multiple);
}

static void test_ignored_lines(void) {
    const TokenType expected[] = {
        TOK_IDENTIFIER, TOK_NEWLINE, TOK_INDENT,     TOK_IDENTIFIER, TOK_NEWLINE, TOK_IDENTIFIER,
        TOK_NEWLINE,    TOK_DEDENT,  TOK_IDENTIFIER, TOK_NEWLINE,    TOK_EOF,
    };
    expect_tokens("\n \n\t\n  // před programem\na\n  b\n\n \t\n// mimo blok\n"
                  "        // více odsazený komentář\n  c\n\nd\n  // za programem\n",
                  expected, sizeof expected / sizeof *expected);

    const TokenType empty[] = {TOK_EOF};
    expect_tokens("\n \t\r\n  // komentář\n\t// konec", empty, sizeof empty / sizeof *empty);
}

static void test_eof_closes_blocks(void) {
    const TokenType expected[] = {
        TOK_IDENTIFIER, TOK_NEWLINE,    TOK_INDENT, TOK_IDENTIFIER, TOK_NEWLINE,
        TOK_INDENT,     TOK_IDENTIFIER, TOK_DEDENT, TOK_DEDENT,     TOK_EOF,
    };
    const char *inputs[] = {
        "a\n  b\n    c",
        "a\n  b\n    c   ",
        "a\n  b\n    c // bez LF",
    };
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; i++) {
        expect_tokens(inputs[i], expected, sizeof expected / sizeof *expected);
    }

    const TokenType after_newline[] = {
        TOK_IDENTIFIER, TOK_NEWLINE, TOK_INDENT, TOK_IDENTIFIER, TOK_NEWLINE, TOK_INDENT,
        TOK_IDENTIFIER, TOK_NEWLINE, TOK_DEDENT, TOK_DEDENT,     TOK_EOF,
    };
    const char *with_newline[] = {
        "a\n  b\n    c\n",
        "a\n  b\n    c // s LF\n",
        "a\n  b\n    c\n// komentář bez LF",
        "a\n  b\n    c\n  // komentář s LF\n",
        "a\n  b\n    c\n\n \t",
    };
    for (size_t i = 0; i < sizeof with_newline / sizeof *with_newline; i++) {
        expect_tokens(with_newline[i], after_newline, sizeof after_newline / sizeof *after_newline);
    }
}

static void test_parenthesized_continuation(void) {
    const TokenType expected[] = {
        TOK_IDENTIFIER, TOK_NEWLINE, TOK_INDENT,     TOK_IDENTIFIER, TOK_LPAREN, TOK_IDENTIFIER,
        TOK_RPAREN,     TOK_NEWLINE, TOK_IDENTIFIER, TOK_NEWLINE,    TOK_DEDENT, TOK_EOF,
    };
    expect_tokens("a\n  f(\n    x)\n  b\n", expected, sizeof expected / sizeof *expected);
    expect_tokens("a\n  f( // komentář\n\n\t// ignorovaný řádek\n    x // další\n    )\n  b\n",
                  expected, sizeof expected / sizeof *expected);

    const TokenType nested[] = {
        TOK_IDENTIFIER, TOK_LPAREN, TOK_IDENTIFIER, TOK_LPAREN, TOK_IDENTIFIER,
        TOK_RPAREN,     TOK_RPAREN, TOK_NEWLINE,    TOK_EOF,
    };
    expect_tokens("f(\n    g(\n  x))\n", nested, sizeof nested / sizeof *nested);

    const TokenType after_closing[] = {
        TOK_IDENTIFIER, TOK_NEWLINE, TOK_INDENT, TOK_IDENTIFIER, TOK_LPAREN, TOK_IDENTIFIER,
        TOK_RPAREN,     TOK_NEWLINE, TOK_DEDENT, TOK_IDENTIFIER, TOK_EOF,
    };
    expect_tokens("a\n  f(\n      x)\nb", after_closing,
                  sizeof after_closing / sizeof *after_closing);
}

static void test_invalid_indentation(void) {
    const char *inputs[] = {
        "\ta", " \ta", "a\n  b\n  \tc", "f(\n\tx)", "f(\nx)", "a\n  f(\n  x)", "a\n    b\n  c",
    };
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; i++) {
        expect_error(inputs[i]);
    }
}

static void test_slash_and_positions(void) {
    FILE *input = input_from_text("  /x\n  y /\n  z");
    Lexer lexer = {0};
    LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    const TokenType expected[] = {
        TOK_INDENT, TOK_SLASH,   TOK_IDENTIFIER, TOK_NEWLINE, TOK_IDENTIFIER,
        TOK_SLASH,  TOK_NEWLINE, TOK_IDENTIFIER, TOK_DEDENT,  TOK_EOF,
    };
    const char *names[] = {NULL, NULL, "x", NULL, "y", NULL, NULL, "z", NULL, NULL};
    const size_t lines[] = {1, 1, 1, 1, 2, 2, 2, 3, 3, 3};
    const size_t columns[] = {1, 3, 4, 1, 3, 5, 1, 3, 4, 4};
    Token token;
    token_init(&token);
    for (size_t i = 0; i < sizeof expected / sizeof *expected; i++) {
        result = next_token(&lexer, &token);
        assert(result == LEXER_OK && token.type == expected[i]);
        assert(token.line == lines[i] && token.column == columns[i]);
        if (names[i] != NULL) {
            assert(strcmp(token.data.text, names[i]) == 0);
        }
        token_free(&token);
    }
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_slash_after_indentation(void) {
    const TokenType expected[] = {
        TOK_IDENTIFIER, TOK_NEWLINE,    TOK_INDENT,  TOK_IDENTIFIER, TOK_NEWLINE,
        TOK_INDENT,     TOK_IDENTIFIER, TOK_NEWLINE, TOK_DEDENT,     TOK_DEDENT,
        TOK_SLASH,      TOK_IDENTIFIER, TOK_NEWLINE, TOK_EOF,
    };
    expect_tokens("a\n  b\n    c\n/x\n", expected, sizeof expected / sizeof *expected);

    const TokenType at_eof[] = {TOK_INDENT, TOK_SLASH, TOK_DEDENT, TOK_EOF};
    expect_tokens("  /", at_eof, sizeof at_eof / sizeof *at_eof);

    const TokenType continuation[] = {
        TOK_IDENTIFIER, TOK_LPAREN, TOK_SLASH, TOK_IDENTIFIER, TOK_RPAREN, TOK_NEWLINE, TOK_EOF,
    };
    expect_tokens("f(\n  /x)\n", continuation, sizeof continuation / sizeof *continuation);
}

int main(void) {
    test_dedent_to_outer_block();
    test_ignored_lines();
    test_eof_closes_blocks();
    test_parenthesized_continuation();
    test_invalid_indentation();
    test_slash_and_positions();
    test_slash_after_indentation();
    return 0;
}
