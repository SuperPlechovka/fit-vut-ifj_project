/**
 * @file
 * @brief Ověření vlastnictví tokenů a životního cyklu kostry lexeru.
 * @details Implementace překladače IFJ26 (IFJ/IAL 2026/27).
 * @author Jan Křepinský (xkrepij00@stud.fit.vutbr.cz)
 */
/* AI prompt: vytvoř unit test podle src/token.h a src/lexer.h.
 * Ověř:
 * - token_init - nový token má prázdný text;
 * - token_init(NULL) a token_free(NULL) - proběhnou bezpečně;
 * - TOK_IDENTIFIER a TOK_STRING_LITERAL - token vlastní textová data;
 * - opakované volání token_free - neuvolní token nesprávně;
 * - token_type_name(TOK_KW_INT) - vrátí "TOK_KW_INT";
 * - token_type_name s neznámou hodnotou - vrátí "TOK_UNKNOWN";
 * - lexer_init s NULL vstupem - vrátí LEXER_INTERNAL_ERROR;
 * - lexer_init s NULL lexerem - vrátí LEXER_INTERNAL_ERROR;
 * - lexer_init nad vstupem "abc" - vrátí LEXER_OK;
 * - druhé lexer_init stejného lexeru - vrátí LEXER_INTERNAL_ERROR;
 * - next_token nad vstupem "abc" - vrátí TOK_IDENTIFIER s textem "abc";
 * - opakované volání lexer_destroy - proběhne bezpečně.
 * - lexer_destroy(NULL) - proběhne bezpečně.
 * Model: GPT-6, Codex.
 */
#include "../../src/lexer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/** @brief Ověří přípravu modulů bez předstírání implementované tokenizace.
 * @return Nula při úspěchu; při nesplnění podmínky skončí přes assert.
 */
int main(void) {
    token_init(NULL);
    token_free(NULL);
    Token token;
    token_init(&token);
    assert(token.text_length == 0);
    TokenType text_types[] = {TOK_IDENTIFIER, TOK_STRING_LITERAL};
    for (size_t i = 0; i < sizeof(text_types) / sizeof(text_types[0]); ++i) {
        token.type = text_types[i];
        token.data.text = malloc(4);
        assert(token.data.text != NULL);
        memcpy(token.data.text, "abc", 4);
        token.text_length = 3;
        token_free(&token);
        token_free(&token);
        assert(token.text_length == 0);
    }
    token.type = TOK_INT_LITERAL;
    token.data.num_int_val = 42;
    token_free(&token);
    assert(strcmp(token_type_name(TOK_KW_INT), "TOK_KW_INT") == 0);
    assert(strcmp(token_type_name((TokenType)-1), "TOK_UNKNOWN") == 0);

    FILE *input = tmpfile();
    assert(input != NULL);
    assert(fputs("abc", input) >= 0);
    rewind(input);
    Lexer lexer = {0};
    assert(lexer_init(&lexer, NULL) == LEXER_INTERNAL_ERROR);
    assert(lexer_init(NULL, input) == LEXER_INTERNAL_ERROR);
    lexer_destroy(&lexer);
    assert(lexer_init(&lexer, input) == LEXER_OK);
    assert(lexer_init(&lexer, input) == LEXER_INTERNAL_ERROR);
    assert(next_token(&lexer, &token) == LEXER_OK);
    assert(token.type == TOK_IDENTIFIER);
    assert(token.text_length == 3);
    assert(strcmp(token.data.text, "abc") == 0);
    lexer_destroy(&lexer);
    lexer_destroy(&lexer);
    lexer_destroy(NULL);
    assert(lexer_init(&lexer, input) == LEXER_OK);
    lexer_destroy(&lexer);
    token_free(&token);
    assert(fclose(input) == 0);
    return 0;
}
