/**
 * @file
 * @brief Testy blokových komentářů, vnoření a jejich vlivu na řádky a odsazení.
 */
/* AI prompt: Ověř komentáře bez LF jako oddělovače tokenů, komentáře s LF
 * jako jeden konec řádku, zákaz dalšího kódu za nimi na stejném fyzickém řádku,
 * ignorování komentářových řádků, EOF, vnoření, pozice a souběh s řetězci a závorkami.
 * Model: GPT-6, Codex.
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

static void expect_tokens(const char *text, const TokenType *expected, size_t count) {
    FILE *input = input_from_text(text);
    Lexer lexer = {0};
    const LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);
    for (size_t i = 0; i < count; ++i) {
        read_token(&lexer, &token, expected[i]);
        token_free(&token);
    }
    assert(count > 0 && expected[count - 1] == TOK_EOF);
    read_token(&lexer, &token, TOK_EOF);
    token_free(&token);
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_inline_comments(void) {
    const TokenType expected[] = {TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_EOF};
    const char *inputs[] = {
        "a/**/b",   "a/* comment */b",      "a/* outer /* inner */ outer */b", "a/* \" // ( ) */b",
        "a/* **/b", "a/* one *//* two */b",
    };
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        expect_tokens(inputs[i], expected, sizeof expected / sizeof *expected);
    }
    const TokenType terms[] = {
        TOK_KW_VAL, TOK_IDENTIFIER, TOK_ASSIGN, TOK_INT_LITERAL, TOK_INT_LITERAL, TOK_EOF,
    };
    expect_tokens("val/**/x=1/**/2", terms, sizeof terms / sizeof *terms);
}

static void test_comments_only(void) {
    const TokenType expected[] = {TOK_EOF};
    const char *inputs[] = {
        "/**/",
        "/* comment */",
        "/* outer /* inner */ outer */",
        "/* a\nb */",
        "  /* comment */\n\t// line\n",
        "  /* a\n*/   \n  // line\n",
        "\t /* comment */",
        "/* one */ /* two */\n",
    };
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        expect_tokens(inputs[i], expected, sizeof expected / sizeof *expected);
    }
}

static void test_multiline_boundaries(void) {
    const TokenType expected[] = {TOK_IDENTIFIER, TOK_NEWLINE, TOK_IDENTIFIER, TOK_EOF};
    const char *inputs[] = {
        "a/*\n*/\nb",
        "a/*\n\n*/  \nb",
        "a/**//* outer\n/* inner */\n*/\nb",
        "a/* \r\n */ \t\r\nb",
    };
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        expect_tokens(inputs[i], expected, sizeof expected / sizeof *expected);
    }
    const TokenType at_eof[] = {TOK_IDENTIFIER, TOK_NEWLINE, TOK_EOF};
    expect_tokens("a/*\n*/", at_eof, sizeof at_eof / sizeof *at_eof);
}

static void test_indentation(void) {
    const TokenType expected[] = {
        TOK_IDENTIFIER, TOK_NEWLINE, TOK_INDENT, TOK_IDENTIFIER, TOK_NEWLINE,
        TOK_IDENTIFIER, TOK_NEWLINE, TOK_DEDENT, TOK_IDENTIFIER, TOK_EOF,
    };
    expect_tokens("a\n  b\n/* only */\n        /* more\n*/\n  c\nd", expected,
                  sizeof expected / sizeof *expected);
    expect_tokens("a\n  /* note */ b\n  /* one *//* two */   c\nd", expected,
                  sizeof expected / sizeof *expected);

    const TokenType slash[] = {
        TOK_IDENTIFIER, TOK_NEWLINE,    TOK_INDENT,  TOK_IDENTIFIER, TOK_NEWLINE,
        TOK_INDENT,     TOK_IDENTIFIER, TOK_NEWLINE, TOK_DEDENT,     TOK_DEDENT,
        TOK_SLASH,      TOK_IDENTIFIER, TOK_NEWLINE, TOK_EOF,
    };
    expect_tokens("a\n  b\n    c\n/*x*//z\n", slash, sizeof slash / sizeof *slash);
}

static void test_parentheses(void) {
    const TokenType expected[] = {
        TOK_IDENTIFIER,  TOK_LPAREN, TOK_INT_LITERAL, TOK_COMMA,
        TOK_INT_LITERAL, TOK_RPAREN, TOK_NEWLINE,     TOK_EOF,
    };
    expect_tokens("f(1/**/,2)\n", expected, sizeof expected / sizeof *expected);
    expect_tokens("f(1/*\n*/\n  ,2)\n", expected, sizeof expected / sizeof *expected);

    const TokenType prefix[] = {
        TOK_IDENTIFIER, TOK_LPAREN, TOK_IDENTIFIER, TOK_RPAREN, TOK_NEWLINE, TOK_EOF,
    };
    expect_tokens("f(\n  /* same row */  x)\n", prefix, sizeof prefix / sizeof *prefix);
    expect_tokens("f(/*\n*/\n  x)\n", prefix, sizeof prefix / sizeof *prefix);
}

static void test_eof_closes_blocks(void) {
    const TokenType expected[] = {
        TOK_IDENTIFIER, TOK_NEWLINE, TOK_INDENT, TOK_IDENTIFIER, TOK_DEDENT, TOK_EOF,
    };
    expect_tokens("a\n  b/**/", expected, sizeof expected / sizeof *expected);
    expect_tokens("a\n  b/**/   ", expected, sizeof expected / sizeof *expected);

    const TokenType after_newline[] = {
        TOK_IDENTIFIER, TOK_NEWLINE, TOK_INDENT, TOK_IDENTIFIER, TOK_NEWLINE, TOK_DEDENT, TOK_EOF,
    };
    expect_tokens("a\n  b/*\n*/", after_newline, sizeof after_newline / sizeof *after_newline);
    expect_tokens("a\n  b\n/* comment */", after_newline,
                  sizeof after_newline / sizeof *after_newline);
    expect_tokens("a\n  b\n/*\n*/", after_newline, sizeof after_newline / sizeof *after_newline);
}

static void test_invalid_comments(void) {
    const char *inputs[] = {
        "/*",
        "/*/",
        "/* outer /* inner */",
        "a/*\n*/b",
        "a/*\n*/ +1",
        "/*\n*/x",
        "f(/*\n*/x)",
        "a\n  /*\n*/  x",
        " \t/* comment */x",
        "f(\n/* same row */  x)",
        "/* *\n/",
    };
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        FILE *input = input_from_text(inputs[i]);
        Lexer lexer = {0};
        LexerResult result = lexer_init(&lexer, input);
        assert(result == LEXER_OK);
        Token token;
        token_init(&token);
        while ((result = next_token(&lexer, &token)) == LEXER_OK && token.type != TOK_EOF) {
            token_free(&token);
        }
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

static void test_strings_and_division(void) {
    FILE *input = input_from_text("a/b/**//c \"/*text*/\" \"\"\"/*\n*/\"\"\"");
    const TokenType expected[] = {
        TOK_IDENTIFIER, TOK_SLASH,          TOK_IDENTIFIER,     TOK_SLASH,
        TOK_IDENTIFIER, TOK_STRING_LITERAL, TOK_STRING_LITERAL, TOK_EOF,
    };
    Lexer lexer = {0};
    const LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);
    for (size_t i = 0; i < sizeof expected / sizeof *expected; ++i) {
        read_token(&lexer, &token, expected[i]);
        if (i == 5) {
            assert(strcmp(token.data.text, "/*text*/") == 0);
        } else if (i == 6) {
            assert(strcmp(token.data.text, "/*\n*/") == 0);
        }
        token_free(&token);
    }
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_source_positions(void) {
    FILE *input = input_from_text("a/*x*/b\n  /* c */d/*\n*/ \nq");
    const TokenType expected[] = {
        TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_NEWLINE,    TOK_INDENT, TOK_IDENTIFIER,
        TOK_NEWLINE,    TOK_DEDENT,     TOK_IDENTIFIER, TOK_EOF,
    };
    const size_t lines[] = {1, 1, 1, 2, 2, 2, 4, 4, 4};
    const size_t columns[] = {1, 7, 1, 1, 10, 1, 1, 1, 2};
    const char *names[] = {"a", "b", NULL, NULL, "d", NULL, NULL, "q", NULL};
    Lexer lexer = {0};
    const LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);
    for (size_t i = 0; i < sizeof expected / sizeof *expected; ++i) {
        read_token(&lexer, &token, expected[i]);
        assert(token.line == lines[i] && token.column == columns[i]);
        if (names[i] != NULL) {
            assert(strcmp(token.data.text, names[i]) == 0);
        }
        if (i == 5) {
            assert(token.end_line == 4 && token.end_column == 1);
        }
        token_free(&token);
    }
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_deep_nesting(void) {
    char text[1024];
    size_t length = 0;
    for (size_t i = 0; i < 128; ++i) {
        memcpy(text + length, "/*", 2);
        length += 2;
    }
    memcpy(text + length, "abc", 3);
    length += 3;
    for (size_t i = 0; i < 128; ++i) {
        memcpy(text + length, "*/", 2);
        length += 2;
    }
    text[length] = '\0';
    const TokenType expected[] = {TOK_EOF};
    expect_tokens(text, expected, sizeof expected / sizeof *expected);
}

int main(void) {
    test_inline_comments();
    test_comments_only();
    test_multiline_boundaries();
    test_indentation();
    test_parentheses();
    test_eof_closes_blocks();
    test_invalid_comments();
    test_strings_and_division();
    test_source_positions();
    test_deep_nesting();
    return 0;
}
