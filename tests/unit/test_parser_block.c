/**
 * @file
 * @brief Unit testy bloků a složených příkazů (parseBlock, if, while).
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
/* AI prompt: vytvoř unit test pro parseBlock podle docs/ll_grammar.md (pravidla 23-31, 42, 43).
 * Blok je v AST seznam příkazů přes next (žádný uzel bloku). Ověř:
 * - blok s jedním a více příkazy, oddělovače NEWLINE a ; (i ; na konci řádku);
 * - () jako tělo bloku, prázdné řádky uvnitř bloku;
 * - if/else (N_IF: left = podmínka, body = THEN, right = ELSE) a while (N_WHILE: left, body);
 * - vnořené bloky a příkaz za složeným příkazem;
 * - že po bloku zůstane v parser.current token za DEDENT;
 * - konec souboru bez \n (lexer pošle DEDENT/EOF bez NEWLINE);
 * - syntaktické chyby (prázdný blok, chybějící oddělovač, then, do, else, blok na stejném
 *   řádku, neodsazené tělo, ;; a samotný ;) - kód 2;
 * - lexikální chybu uvnitř bloku - kód 1.
 * Vstup začíná pomocným identifikátorem B, který test přeskočí, protože blok vždy následuje
 * po hlavičce a první řádek souboru nesmí být odsazený.
 * Model: Claude Opus 5.5, Claude Code.
 */
#include "../../src/parser.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *input;
    Lexer lexer;
    Parser parser;
    ParseError error;
    Node *result;
    ErrorCode code;
} BlockRun;

static void run_block(BlockRun *run, const char *text) {
    memset(run, 0, sizeof(*run));
    run->input = tmpfile();
    assert(run->input != NULL);
    assert(fputs(text, run->input) >= 0);
    rewind(run->input);

    assert(lexer_init(&run->lexer, run->input) == LEXER_OK);
    assert(parserInit(&run->parser, &run->lexer, &run->error) == ERR_OK);

    /* přeskočí pomocnou "hlavičku" B */
    assert(run->parser.current.type == TOK_IDENTIFIER);
    assert(parserAdvance(&run->parser, &run->error) == ERR_OK);

    run->code = parseBlock(&run->parser, &run->error, &run->result);
}

static void finish(BlockRun *run) {
    nodeFree(run->result);
    parserDestroy(&run->parser);
    lexer_destroy(&run->lexer);
    fclose(run->input);
}

static void append_text(char *buffer, size_t size, const char *text) {
    strncat(buffer, text, size - strlen(buffer) - 1);
}

static const char *op_text(OpKind op) {
    switch (op) {
    case OP_ADD:
        return "+";
    case OP_SUB:
        return "-";
    case OP_MUL:
        return "*";
    case OP_DIV:
        return "/";
    case OP_LT:
        return "<";
    case OP_LE:
        return "<=";
    case OP_GT:
        return ">";
    case OP_GE:
        return ">=";
    case OP_EQ:
        return "==";
    case OP_NE:
        return "!=";
    default:
        return "?";
    }
}

static void node_to_text(const Node *node, char *buffer, size_t size);

/* Seznam příkazů (blok) jako "{a; b; c}". */
static void list_to_text(const Node *list, char *buffer, size_t size) {
    append_text(buffer, size, "{");
    for (const Node *stmt = list; stmt != NULL; stmt = stmt->next) {
        node_to_text(stmt, buffer, size);
        if (stmt->next != NULL) {
            append_text(buffer, size, "; ");
        }
    }
    append_text(buffer, size, "}");
}

static void node_to_text(const Node *node, char *buffer, size_t size) {
    char tmp[64];

    if (node == NULL) {
        append_text(buffer, size, "NULL");
        return;
    }

    switch (node->kind) {
    case N_LIT_INT:
        snprintf(tmp, sizeof(tmp), "%lld", node->ival);
        append_text(buffer, size, tmp);
        break;
    case N_LIT_DOUBLE:
        snprintf(tmp, sizeof(tmp), "%g", node->dval);
        append_text(buffer, size, tmp);
        break;
    case N_LIT_STRING:
        append_text(buffer, size, "\"");
        append_text(buffer, size, node->name);
        append_text(buffer, size, "\"");
        break;
    case N_LIT_UNIT:
        append_text(buffer, size, "()");
        break;
    case N_VAR:
        append_text(buffer, size, node->name);
        break;
    case N_UNARY:
        append_text(buffer, size, "(-");
        node_to_text(node->left, buffer, size);
        append_text(buffer, size, ")");
        break;
    case N_BINARY:
        append_text(buffer, size, "(");
        node_to_text(node->left, buffer, size);
        append_text(buffer, size, " ");
        append_text(buffer, size, op_text(node->op));
        append_text(buffer, size, " ");
        node_to_text(node->right, buffer, size);
        append_text(buffer, size, ")");
        break;
    case N_CALL:
        append_text(buffer, size, node->name);
        append_text(buffer, size, "(");
        for (const Node *arg = node->args; arg != NULL; arg = arg->next) {
            node_to_text(arg, buffer, size);
            if (arg->next != NULL) {
                append_text(buffer, size, ", ");
            }
        }
        append_text(buffer, size, ")");
        break;
    case N_DECL:
        append_text(buffer, size, node->is_var ? "var " : "val ");
        append_text(buffer, size, node->name);
        append_text(buffer, size, " = ");
        node_to_text(node->left, buffer, size);
        break;
    case N_ASSIGN:
        append_text(buffer, size, node->name);
        append_text(buffer, size, " = ");
        node_to_text(node->left, buffer, size);
        break;
    case N_RETURN:
        append_text(buffer, size, "return");
        if (node->left != NULL) {
            append_text(buffer, size, " ");
            node_to_text(node->left, buffer, size);
        }
        break;
    case N_IF:
        append_text(buffer, size, "if ");
        node_to_text(node->left, buffer, size);
        append_text(buffer, size, " ");
        list_to_text(node->body, buffer, size);
        append_text(buffer, size, " else ");
        list_to_text(node->right, buffer, size);
        break;
    case N_WHILE:
        append_text(buffer, size, "while ");
        node_to_text(node->left, buffer, size);
        append_text(buffer, size, " ");
        list_to_text(node->body, buffer, size);
        break;
    default:
        snprintf(tmp, sizeof(tmp), "<kind %d>", (int)node->kind);
        append_text(buffer, size, tmp);
        break;
    }
}

/* Blok se musí zparsovat na `expected` a zastavit se na tokenu `next`. */
static void expect_block(const char *text, const char *expected, TokenType next) {
    BlockRun run;
    run_block(&run, text);

    if (run.code != ERR_OK) {
        fprintf(stderr, "'%s': očekáváno %s, chyba %d na %zu:%zu (%s)\n", text, expected, run.code,
                run.error.line, run.error.column,
                run.error.message != NULL ? run.error.message : "");
    }
    assert(run.code == ERR_OK);

    char buffer[512] = {0};
    list_to_text(run.result, buffer, sizeof(buffer));
    if (strcmp(buffer, expected) != 0) {
        fprintf(stderr, "'%s':\n  očekáváno %s\n  dostal    %s\n", text, expected, buffer);
    }
    assert(strcmp(buffer, expected) == 0);

    if (run.parser.current.type != next) {
        fprintf(stderr, "'%s': po bloku očekáván token %d, je %d\n", text, (int)next,
                (int)run.parser.current.type);
    }
    assert(run.parser.current.type == next);

    finish(&run);
}

static void expect_error(const char *text, ErrorCode expected) {
    BlockRun run;
    run_block(&run, text);

    if (run.code != expected) {
        fprintf(stderr, "'%s': očekávána chyba %d, dostal %d\n", text, expected, run.code);
    }
    assert(run.code == expected);
    assert((int)run.error.code == (int)expected);
    assert(run.result == NULL);

    finish(&run);
}

static void test_simple_blocks(void) {
    expect_block("B\n    x = 1\n", "{x = 1}", TOK_EOF);
    expect_block("B\n    x = 1\n    y = 2\n    z = 3\n", "{x = 1; y = 2; z = 3}", TOK_EOF);
    expect_block("B\n    val a = 1\n    print(a)\n    return a\n",
                 "{val a = 1; print(a); return a}", TOK_EOF);
    expect_block("B\n    ()\n", "{()}", TOK_EOF);
    expect_block("B\n    return\n", "{return}", TOK_EOF);
}

static void test_separators(void) {
    expect_block("B\n    x = 1; y = 2\n", "{x = 1; y = 2}", TOK_EOF);
    expect_block("B\n    x = 1; y = 2; z = 3\n", "{x = 1; y = 2; z = 3}", TOK_EOF);
    /* pravidlo 31: ; <nl_opt> - středník na konci řádku */
    expect_block("B\n    x = 1;\n    y = 2\n", "{x = 1; y = 2}", TOK_EOF);
    /* prázdné řádky a komentáře uvnitř bloku */
    expect_block("B\n    x = 1\n\n    y = 2\n", "{x = 1; y = 2}", TOK_EOF);
    expect_block("B\n    x = 1 // komentar\n    y = 2\n", "{x = 1; y = 2}", TOK_EOF);
}

static void test_after_block(void) {
    /* po DEDENT zůstane v current první token za blokem */
    expect_block("B\n    x = 1\ny\n", "{x = 1}", TOK_IDENTIFIER);
    expect_block("B\n    x = 1\ndef\n", "{x = 1}", TOK_KW_DEF);
}

static void test_no_trailing_newline(void) {
    /* bez \n na konci souboru přijde DEDENT/EOF místo NEWLINE */
    expect_block("B\n    x = 1", "{x = 1}", TOK_EOF);
    expect_block("B\n    x = 1\n    return x", "{x = 1; return x}", TOK_EOF);
    expect_block("B\n    while a < b do\n        a = a + 1", "{while (a < b) {a = (a + 1)}}",
                 TOK_EOF);
}

static void test_if(void) {
    expect_block("B\n"
                 "    if a < b then\n"
                 "        x = 1\n"
                 "    else\n"
                 "        x = 2\n",
                 "{if (a < b) {x = 1} else {x = 2}}", TOK_EOF);

    expect_block("B\n"
                 "    if x == 0 then\n"
                 "        print(x)\n"
                 "        y = 1\n"
                 "    else\n"
                 "        ()\n"
                 "    z = 3\n",
                 "{if (x == 0) {print(x); y = 1} else {()}; z = 3}", TOK_EOF);
}

static void test_if_fields(void) {
    BlockRun run;
    run_block(&run, "B\n"
                    "    if a then\n"
                    "        x = 1\n"
                    "    else\n"
                    "        y = 2\n");
    assert(run.code == ERR_OK);
    Node *stmt = run.result;
    assert(stmt != NULL && stmt->kind == N_IF && stmt->next == NULL);
    assert(stmt->left != NULL && stmt->left->kind == N_VAR);
    assert(stmt->body != NULL && stmt->body->kind == N_ASSIGN);
    assert(strcmp(stmt->body->name, "x") == 0 && stmt->body->next == NULL);
    assert(stmt->right != NULL && stmt->right->kind == N_ASSIGN);
    assert(strcmp(stmt->right->name, "y") == 0 && stmt->right->next == NULL);
    assert(stmt->line == 2 && stmt->col == 5);
    finish(&run);
}

static void test_while(void) {
    expect_block("B\n"
                 "    while i < 10 do\n"
                 "        i = i + 1\n",
                 "{while (i < 10) {i = (i + 1)}}", TOK_EOF);

    expect_block("B\n"
                 "    var i = 0\n"
                 "    while i < 3 do\n"
                 "        print(i)\n"
                 "        i = i + 1\n"
                 "    return i\n",
                 "{var i = 0; while (i < 3) {print(i); i = (i + 1)}; return i}", TOK_EOF);
}

static void test_while_fields(void) {
    BlockRun run;
    run_block(&run, "B\n"
                    "    while a do\n"
                    "        x = 1\n");
    assert(run.code == ERR_OK);
    Node *stmt = run.result;
    assert(stmt != NULL && stmt->kind == N_WHILE && stmt->next == NULL);
    assert(stmt->left != NULL && stmt->left->kind == N_VAR);
    assert(stmt->body != NULL && stmt->body->kind == N_ASSIGN && stmt->body->next == NULL);
    assert(stmt->line == 2 && stmt->col == 5);
    finish(&run);
}

static void test_nested(void) {
    expect_block("B\n"
                 "    while a do\n"
                 "        if b then\n"
                 "            x = 1\n"
                 "        else\n"
                 "            while c do\n"
                 "                y = 2\n"
                 "        z = 3\n"
                 "    w = 4\n",
                 "{while a {if b {x = 1} else {while c {y = 2}}; z = 3}; w = 4}", TOK_EOF);
}

static void test_errors(void) {
    /* blok */
    expect_error("B\n", ERR_SYNTAX);                      /* prázdný blok */
    expect_error("B x = 1\n", ERR_SYNTAX);                /* chybí NEWLINE */
    expect_error("B\nx = 1\n", ERR_SYNTAX);               /* chybí INDENT */
    expect_error("B\n    x = 1 y = 2\n", ERR_SYNTAX);     /* chybí oddělovač */
    expect_error("B\n    x = 1;; y = 2\n", ERR_SYNTAX);   /* dva středníky */
    expect_error("B\n    ;\n", ERR_SYNTAX);               /* samotný středník */
    expect_error("B\n    x = 1\n    else\n", ERR_SYNTAX); /* else bez if */

    /* if */
    expect_error("B\n    if a\n        x = 1\n    else\n        x = 2\n", ERR_SYNTAX);
    expect_error("B\n    if a then\n        x = 1\n", ERR_SYNTAX);
    expect_error("B\n    if a then x = 1\n    else\n        x = 2\n", ERR_SYNTAX);
    expect_error("B\n    if a then\n        x = 1\n    else x = 2\n", ERR_SYNTAX);
    expect_error("B\n    if a then\n        x = 1\n        else\n            x = 2\n", ERR_SYNTAX);
    expect_error("B\n    if then\n        x = 1\n    else\n        x = 2\n", ERR_SYNTAX);

    /* while */
    expect_error("B\n    while a\n        x = 1\n", ERR_SYNTAX);
    expect_error("B\n    while a do\n    x = 1\n", ERR_SYNTAX);
    expect_error("B\n    while a do x = 1\n", ERR_SYNTAX);
    expect_error("B\n    while do\n        x = 1\n", ERR_SYNTAX);

    /* chyba až uvnitř vnořeného bloku - rozpracovaný strom se musí uvolnit (ASan) */
    expect_error("B\n    x = 1\n    while a do\n        y = 2\n        z =\n", ERR_SYNTAX);

    /* lexikální chyba */
    expect_error("B\n    x = 1\n    y = 1.2.3\n", ERR_LEXICAL);
}

int main(void) {
    test_simple_blocks();
    test_separators();
    test_after_block();
    test_no_trailing_newline();
    test_if();
    test_if_fields();
    test_while();
    test_while_fields();
    test_nested();
    test_errors();
    return 0;
}
