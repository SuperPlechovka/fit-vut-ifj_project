/**
 * @file
 * @brief Ověření vlastnictví tokenů a životního cyklu lexeru.
 * @details Implementace překladače IFJ26 (IFJ/IAL 2026/27).
 * @author Jan Křepinský (xkrepij00@stud.fit.vutbr.cz)
 */
/* AI prompt: vytvoř unit testy podle src/token.h a src/lexer.h.
 * Rozděl kontroly do menších funkcí a spusť je z main.
 * Ověř:
 * - token_init - nový token má prázdný text;
 * - token_init(NULL) a token_free(NULL) - proběhnou bezpečně;
 * - TOK_IDENTIFIER a TOK_STRING_LITERAL - token vlastní textová data;
 * - po prvním i druhém token_free je délka textu 0;
 * - token_free u celočíselného tokenu nastaví druh na TOK_IDENTIFIER a délku textu na 0;
 * - token_type_name(TOK_KW_INT) - vrátí "TOK_KW_INT";
 * - token_type_name s neznámou hodnotou - vrátí "TOK_UNKNOWN";
 * - lexer_init s NULL vstupem - vrátí LEXER_INTERNAL_ERROR;
 * - lexer_init s NULL lexerem - vrátí LEXER_INTERNAL_ERROR;
 * - lexer_init s prázdným dočasným souborem vrátí LEXER_OK a uloží předaný vstup;
 * - druhé lexer_init stejného lexeru - vrátí LEXER_INTERNAL_ERROR;
 * - lexer_destroy nastaví vstup a oba buffery na NULL;
 * - opakované uvolnění, vynulovaný lexer a NULL - proběhnou bezpečně;
 * - inicializace po lexer_destroy znovu vrátí LEXER_OK.
 * Model: GPT-6, Codex.
 */
#include "../../src/lexer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void test_token_initialization(void) {
    token_init(NULL);
    Token token;
    token_init(&token);
    assert(token.text_length == 0);
}

static void test_token_text_cleanup(void) {
    token_free(NULL);
    TokenType text_types[] = {TOK_IDENTIFIER, TOK_STRING_LITERAL};
    for (size_t i = 0; i < sizeof(text_types) / sizeof(text_types[0]); ++i) {
        Token token;
        token_init(&token);
        token.type = text_types[i];
        token.data.text = malloc(4);
        assert(token.data.text != NULL);
        memcpy(token.data.text, "abc", 4);
        token.text_length = 3;
        token_free(&token);
        assert(token.text_length == 0);
        token_free(&token);
        assert(token.text_length == 0);
    }
}

static void test_token_number_cleanup(void) {
    Token token;
    token_init(&token);
    token.type = TOK_INT_LITERAL;
    token.data.num_int_val = 42;
    token_free(&token);
    assert(token.type == TOK_IDENTIFIER);
    assert(token.text_length == 0);
}

static void test_token_type_names(void) {
    assert(strcmp(token_type_name(TOK_KW_INT), "TOK_KW_INT") == 0);
    assert(strcmp(token_type_name((TokenType)-1), "TOK_UNKNOWN") == 0);
}

static void test_lexer_null_initialization(void) {
    FILE *input = tmpfile();
    assert(input != NULL);
    Lexer lexer = {0};
    assert(lexer_init(&lexer, NULL) == LEXER_INTERNAL_ERROR);
    assert(lexer_init(NULL, input) == LEXER_INTERNAL_ERROR);
    lexer_destroy(&lexer);
    assert(fclose(input) == 0);
}

static void test_lexer_initialization(void) {
    FILE *input = tmpfile();
    assert(input != NULL);
    Lexer lexer = {0};
    assert(lexer_init(&lexer, input) == LEXER_OK);
    assert(lexer.input == input);
    lexer_destroy(&lexer);
    assert(fclose(input) == 0);
}

static void test_lexer_repeated_initialization(void) {
    FILE *input = tmpfile();
    assert(input != NULL);
    Lexer lexer = {0};
    assert(lexer_init(&lexer, input) == LEXER_OK);
    assert(lexer_init(&lexer, input) == LEXER_INTERNAL_ERROR);
    lexer_destroy(&lexer);
    assert(fclose(input) == 0);
}

static void test_lexer_cleanup(void) {
    FILE *input = tmpfile();
    assert(input != NULL);
    Lexer lexer = {0};
    lexer_destroy(NULL);
    lexer_destroy(&lexer);
    assert(lexer_init(&lexer, input) == LEXER_OK);
    lexer_destroy(&lexer);
    assert(lexer.input == NULL);
    assert(lexer.lexem == NULL);
    assert(lexer.indents == NULL);
    lexer_destroy(&lexer);
    assert(fclose(input) == 0);
}

static void test_lexer_reinitialization(void) {
    FILE *input = tmpfile();
    assert(input != NULL);
    Lexer lexer = {0};
    assert(lexer_init(&lexer, input) == LEXER_OK);
    lexer_destroy(&lexer);
    assert(lexer_init(&lexer, input) == LEXER_OK);
    lexer_destroy(&lexer);
    assert(fclose(input) == 0);
}

int main(void) {
    test_token_initialization();
    test_token_text_cleanup();
    test_token_number_cleanup();
    test_token_type_names();
    test_lexer_null_initialization();
    test_lexer_initialization();
    test_lexer_repeated_initialization();
    test_lexer_cleanup();
    test_lexer_reinitialization();
    return 0;
}
