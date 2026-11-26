/**
 * @file
 * @brief Testy společného použití odsazení, komentářů, čísel a řetězců.
 */
/* AI prompt: Doplň regresní testy lexeru na celých programech a kombinacích
 * pravidel z předchozích částí. Kontroluj pořadí tokenů, jejich hodnoty,
 * vybrané zdrojové pozice, EOF a chyby po správně přečtené části programu.
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
        fprintf(stderr, "Očekáván %s, výsledek %d, token %s na %zu:%zu\n",
                token_type_name(expected), result, token_type_name(token->type), token->line,
                token->column);
    }
    assert(result == LEXER_OK);
    assert(token->type == expected);
}

static void expect_tokens(const char *text, const Token *expected, size_t count) {
    FILE *input = input_from_text(text);
    Lexer lexer = {0};
    const LexerResult result = lexer_init(&lexer, input);
    assert(result == LEXER_OK);
    Token token;
    token_init(&token);
    for (size_t i = 0; i < count; ++i) {
        read_token(&lexer, &token, expected[i].type);
        if (token.type == TOK_IDENTIFIER || token.type == TOK_STRING_LITERAL) {
            assert(token.text_length == strlen(expected[i].data.text));
            assert(strcmp(token.data.text, expected[i].data.text) == 0);
        } else if (token.type == TOK_INT_LITERAL) {
            assert(token.data.num_int_val == expected[i].data.num_int_val);
        } else if (token.type == TOK_DOUBLE_LITERAL) {
            assert(token.data.num_float_val == expected[i].data.num_float_val);
        }
        if (expected[i].line != 0) {
            assert(token.line == expected[i].line && token.column == expected[i].column);
            assert(token.end_line == expected[i].end_line &&
                   token.end_column == expected[i].end_column);
        }
        token_free(&token);
    }
    assert(count > 0 && expected[count - 1].type == TOK_EOF);
    read_token(&lexer, &token, TOK_EOF);
    token_free(&token);
    lexer_destroy(&lexer);
    fclose(input);
}

static void test_program_with_nested_blocks(void) {
    const char *text = "/* header /* nested */ */\n"
                       "\n"
                       "import ifj26.*\n"
                       "@main def run(): Unit =\n"
                       "  var n = 1_000\n"
                       "  while n > 0 do\n"
                       "    if n == 1 then\n"
                       "      print(\"done\\n\")\n"
                       "// comment at column 1\n"
                       "/* outer\n"
                       "/* inner */ */\n"
                       "    else\n"
                       "      ()\n"
                       "    n = n - 1\n"
                       "  print(n) // EOF without LF";
    const Token expected[] = {
        {.type = TOK_KW_IMPORT},
        {.type = TOK_IDENTIFIER, .data.text = "ifj26"},
        {.type = TOK_DOT},
        {.type = TOK_STAR},
        {.type = TOK_NEWLINE},
        {.type = TOK_AT},
        {.type = TOK_IDENTIFIER, .data.text = "main"},
        {.type = TOK_KW_DEF},
        {.type = TOK_IDENTIFIER, .data.text = "run"},
        {.type = TOK_LPAREN},
        {.type = TOK_RPAREN},
        {.type = TOK_COLON},
        {.type = TOK_KW_UNIT},
        {.type = TOK_ASSIGN},
        {.type = TOK_NEWLINE},
        {.type = TOK_INDENT},
        {.type = TOK_KW_VAR},
        {.type = TOK_IDENTIFIER, .data.text = "n"},
        {.type = TOK_ASSIGN},
        {.type = TOK_INT_LITERAL, .data.num_int_val = 1000},
        {.type = TOK_NEWLINE},
        {.type = TOK_KW_WHILE},
        {.type = TOK_IDENTIFIER, .data.text = "n"},
        {.type = TOK_GT},
        {.type = TOK_INT_LITERAL, .data.num_int_val = 0},
        {.type = TOK_KW_DO},
        {.type = TOK_NEWLINE},
        {.type = TOK_INDENT},
        {.type = TOK_KW_IF},
        {.type = TOK_IDENTIFIER, .data.text = "n"},
        {.type = TOK_EQ},
        {.type = TOK_INT_LITERAL, .data.num_int_val = 1},
        {.type = TOK_KW_THEN},
        {.type = TOK_NEWLINE},
        {.type = TOK_INDENT},
        {.type = TOK_IDENTIFIER, .data.text = "print"},
        {.type = TOK_LPAREN},
        {.type = TOK_STRING_LITERAL, .data.text = "done\n"},
        {.type = TOK_RPAREN},
        {.type = TOK_NEWLINE},
        {.type = TOK_DEDENT},
        {.type = TOK_KW_ELSE},
        {.type = TOK_NEWLINE},
        {.type = TOK_INDENT},
        {.type = TOK_LPAREN},
        {.type = TOK_RPAREN},
        {.type = TOK_NEWLINE},
        {.type = TOK_DEDENT},
        {.type = TOK_IDENTIFIER, .data.text = "n"},
        {.type = TOK_ASSIGN},
        {.type = TOK_IDENTIFIER, .data.text = "n"},
        {.type = TOK_MINUS},
        {.type = TOK_INT_LITERAL, .data.num_int_val = 1},
        {.type = TOK_NEWLINE},
        {.type = TOK_DEDENT},
        {.type = TOK_IDENTIFIER, .data.text = "print"},
        {.type = TOK_LPAREN},
        {.type = TOK_IDENTIFIER, .data.text = "n"},
        {.type = TOK_RPAREN},
        {.type = TOK_DEDENT},
        {.type = TOK_EOF},
    };
    expect_tokens(text, expected, sizeof expected / sizeof *expected);
}

static void test_multiline_call_in_block(void) {
    const char *text = "def message(): String =\n"
                       "  val text = concat(\n"
                       "    \"\"\"first\n"
                       "/* not a comment */ ( ) \\n\n"
                       "last\"\"\", /* outer\n"
                       "/* inner */ */\n"
                       "    \"end\\n\", // line\n"
                       "    )\n"
                       "  return text";
    const Token expected[] = {
        {.type = TOK_KW_DEF},
        {.type = TOK_IDENTIFIER, .data.text = "message"},
        {.type = TOK_LPAREN},
        {.type = TOK_RPAREN},
        {.type = TOK_COLON},
        {.type = TOK_KW_STRING},
        {.type = TOK_ASSIGN},
        {.type = TOK_NEWLINE},
        {.type = TOK_INDENT},
        {.type = TOK_KW_VAL},
        {.type = TOK_IDENTIFIER, .data.text = "text"},
        {.type = TOK_ASSIGN},
        {.type = TOK_IDENTIFIER, .data.text = "concat"},
        {.type = TOK_LPAREN},
        {.type = TOK_STRING_LITERAL, .data.text = "first\n/* not a comment */ ( ) \\n\nlast"},
        {.type = TOK_COMMA},
        {.type = TOK_STRING_LITERAL, .data.text = "end\n"},
        {.type = TOK_COMMA},
        {.type = TOK_RPAREN},
        {.type = TOK_NEWLINE},
        {.type = TOK_KW_RETURN},
        {.type = TOK_IDENTIFIER, .data.text = "text"},
        {.type = TOK_DEDENT},
        {.type = TOK_EOF},
    };
    expect_tokens(text, expected, sizeof expected / sizeof *expected);
}

static void test_positions_after_combined_literals(void) {
    const char *text = "f(\n"
                       "  .5D,/* note */\n"
                       "  \"\"\"a\n"
                       "b\"\"\",1_000\n"
                       "  )/2";
    const Token expected[] = {
        {.type = TOK_IDENTIFIER,
         .data.text = "f",
         .line = 1,
         .column = 1,
         .end_line = 1,
         .end_column = 2},
        {.type = TOK_LPAREN, .line = 1, .column = 2, .end_line = 1, .end_column = 3},
        {.type = TOK_DOUBLE_LITERAL,
         .data.num_float_val = 0.5,
         .line = 2,
         .column = 3,
         .end_line = 2,
         .end_column = 6},
        {.type = TOK_COMMA, .line = 2, .column = 6, .end_line = 2, .end_column = 7},
        {.type = TOK_STRING_LITERAL,
         .data.text = "a\nb",
         .line = 3,
         .column = 3,
         .end_line = 4,
         .end_column = 5},
        {.type = TOK_COMMA, .line = 4, .column = 5, .end_line = 4, .end_column = 6},
        {.type = TOK_INT_LITERAL,
         .data.num_int_val = 1000,
         .line = 4,
         .column = 6,
         .end_line = 4,
         .end_column = 11},
        {.type = TOK_RPAREN, .line = 5, .column = 3, .end_line = 5, .end_column = 4},
        {.type = TOK_SLASH, .line = 5, .column = 4, .end_line = 5, .end_column = 5},
        {.type = TOK_INT_LITERAL,
         .data.num_int_val = 2,
         .line = 5,
         .column = 5,
         .end_line = 5,
         .end_column = 6},
        {.type = TOK_EOF, .line = 5, .column = 6, .end_line = 5, .end_column = 6},
    };
    expect_tokens(text, expected, sizeof expected / sizeof *expected);
}

static void test_error_after_valid_program_prefix(void) {
    const char *prefix = "import ifj26.*\n"
                         "@main def run(): Unit =\n"
                         "  val text = \"\"\"a\n"
                         "/* text */\"\"\"\n"
                         "  print(text) /* outer /* inner */ */\n";
    const char *invalid[] = {
        "  val n = 1__0\n",
        "  print(\"bad\\q\")\n",
        "  /* open /* nested */",
        "  print(1,\n  2)\n",
        "  print(text) /* first\nlast */ + 1\n",
        "\tprint(text)\n",
    };
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        FILE *input = tmpfile();
        assert(input != NULL);
        const int prefix_written = fputs(prefix, input);
        const int suffix_written = fputs(invalid[i], input);
        assert(prefix_written >= 0 && suffix_written >= 0);
        rewind(input);
        Lexer lexer = {0};
        LexerResult result = lexer_init(&lexer, input);
        assert(result == LEXER_OK);
        Token token;
        token_init(&token);
        while ((result = next_token(&lexer, &token)) == LEXER_OK && token.type != TOK_EOF) {
            token_free(&token);
        }
        if (result != LEXER_ERROR) {
            fprintf(stderr, "Vstup <%s>: očekávána lexikální chyba, výsledek %d\n", invalid[i],
                    result);
        }
        assert(result == LEXER_ERROR);
        assert(lexer.line >= 6);
        token_free(&token);
        lexer_destroy(&lexer);
        fclose(input);
    }
}

int main(void) {
    test_program_with_nested_blocks();
    test_multiline_call_in_block();
    test_positions_after_combined_literals();
    test_error_after_valid_program_prefix();
    return 0;
}
