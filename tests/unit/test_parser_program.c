/**
 * @file
 * @brief Unit testy celého programu: prolog, funkce, parametry, @main (parseProgram).
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
/* AI prompt: vytvoř unit test pro parseProgram podle docs/ll_grammar.md (pravidla 1-16).
 * Ověř:
 * - prolog import ifj26.* (i program bez funkcí - chybějící main je až sémantická chyba);
 * - @main na samostatném řádku i na řádku s def, libovolné jméno vstupní funkce;
 * - jednořádkové tělo (výraz, volání; i bez \n na konci souboru) a tělo jako blok;
 *   jednořádkové tělo je v AST seznam o jednom příkazu;
 * - parametry (0, 1, více, všechny typy) jako N_PARAM v N_FUNC->args;
 * - návratový typ (uvedený / neuvedený = TY_UNKNOWN), pořadí funkcí v seznamu přes next;
 * - pozici funkce (klíčové slovo def);
 * - syntaktické chyby (prolog, @main, hlavička funkce, parametry, tělo, příkaz mimo
 *   funkci) - kód 2; lexikální chybu - kód 1.
 * Případy závislé na opravách lexeru (prázdné řádky, komentáře, blokové komentáře)
 * jsou zvlášť v test_lexer_dependent.
 * Doplněno promptem: napiš testy na opravy z review PR - mezi @ a main nesmí být žádný
 * znak (spec 2.1), koncová čárka v parametrech a argumentech jen s ) na dalším řádku.
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
} ProgramRun;

static void run_program(ProgramRun *run, const char *text) {
    memset(run, 0, sizeof(*run));
    run->input = tmpfile();
    assert(run->input != NULL);
    assert(fputs(text, run->input) >= 0);
    rewind(run->input);

    assert(lexer_init(&run->lexer, run->input) == LEXER_OK);
    assert(parserInit(&run->parser, &run->lexer, &run->error) == ERR_OK);
    run->code = parseProgram(&run->parser, &run->error, &run->result);
}

static void finish(ProgramRun *run) {
    nodeFree(run->result);
    parserDestroy(&run->parser);
    lexer_destroy(&run->lexer);
    fclose(run->input);
}

static void append_text(char *buffer, size_t size, const char *text) {
    strncat(buffer, text, size - strlen(buffer) - 1);
}

static const char *type_text(TypeKind type) {
    switch (type) {
    case TY_INT:
        return "Int";
    case TY_DOUBLE:
        return "Double";
    case TY_STRING:
        return "String";
    case TY_UNIT:
        return "Unit";
    default:
        return "?";
    }
}

static void node_to_text(const Node *node, char *buffer, size_t size);

/* Seznam příkazů jako "{a; b}". */
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

/* Výrazy a příkazy jen zjednodušeně - podrobně je testují test_parser_expr/stmt/block. */
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
    case N_BINARY:
        append_text(buffer, size, "<binary>");
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

/* Funkce jako "@main name(a: Int, b: String): Int = {tělo}". */
static void func_to_text(const Node *func, char *buffer, size_t size) {
    if (func->kind != N_FUNC) {
        append_text(buffer, size, "<not a function>");
        return;
    }
    if (func->is_main) {
        append_text(buffer, size, "@main ");
    }
    append_text(buffer, size, func->name);
    append_text(buffer, size, "(");
    for (const Node *param = func->args; param != NULL; param = param->next) {
        append_text(buffer, size, param->name);
        append_text(buffer, size, ": ");
        append_text(buffer, size, type_text(param->type));
        if (param->next != NULL) {
            append_text(buffer, size, ", ");
        }
    }
    append_text(buffer, size, ")");
    if (func->type != TY_UNKNOWN) {
        append_text(buffer, size, ": ");
        append_text(buffer, size, type_text(func->type));
    }
    append_text(buffer, size, " = ");
    list_to_text(func->body, buffer, size);
}

/* Program jako funkce oddělené " | ", prázdný program jako "". */
static void program_to_text(const Node *program, char *buffer, size_t size) {
    if (program == NULL || program->kind != N_PROGRAM) {
        append_text(buffer, size, "<not a program>");
        return;
    }
    for (const Node *func = program->body; func != NULL; func = func->next) {
        func_to_text(func, buffer, size);
        if (func->next != NULL) {
            append_text(buffer, size, " | ");
        }
    }
}

static void expect_program(const char *text, const char *expected) {
    ProgramRun run;
    run_program(&run, text);

    if (run.code != ERR_OK) {
        fprintf(stderr, "'%s': očekáváno %s, chyba %d na %zu:%zu (%s)\n", text, expected, run.code,
                run.error.line, run.error.column,
                run.error.message != NULL ? run.error.message : "");
    }
    assert(run.code == ERR_OK);

    char buffer[1024] = {0};
    program_to_text(run.result, buffer, sizeof(buffer));
    if (strcmp(buffer, expected) != 0) {
        fprintf(stderr, "'%s':\n  očekáváno %s\n  dostal    %s\n", text, expected, buffer);
    }
    assert(strcmp(buffer, expected) == 0);
    assert(run.parser.current.type == TOK_EOF);

    finish(&run);
}

static void expect_error(const char *text, ErrorCode expected) {
    ProgramRun run;
    run_program(&run, text);

    if (run.code != expected) {
        fprintf(stderr, "'%s': očekávána chyba %d, dostal %d\n", text, expected, run.code);
    }
    assert(run.code == expected);
    assert((int)run.error.code == (int)expected);
    assert(run.result == NULL);

    finish(&run);
}

#define PROLOG "import ifj26.*\n"

static void test_prolog(void) {
    /* bez funkcí je program syntakticky v pořádku (chybějící main = chyba 3) */
    expect_program(PROLOG, "");
    expect_program("import ifj26 . *\n", "");
}

static void test_main(void) {
    expect_program(PROLOG "@main\n"
                          "def main() =\n"
                          "    x = 1\n",
                   "@main main() = {x = 1}");
    expect_program(PROLOG "@main def main(): Unit =\n"
                          "    print(1)\n",
                   "@main main(): Unit = {print(1)}");
    /* jméno vstupní funkce je libovolné */
    expect_program(PROLOG "@main\n"
                          "def runFoo(): Unit =\n"
                          "    val n = readInt()\n",
                   "@main runFoo(): Unit = {val n = readInt()}");
    expect_program(PROLOG "@main def run(): Unit = print(1)\n", "@main run(): Unit = {print(1)}");
}

static void test_one_line_body(void) {
    expect_program(PROLOG "def f(): Int = 5\n", "f(): Int = {5}");
    expect_program(PROLOG "def f() = x\n", "f() = {x}");
    expect_program(PROLOG "def f(a: Int): Int = a + 1\n", "f(a: Int): Int = {<binary>}");
    expect_program(PROLOG "def f() = g(1, \"s\")\n", "f() = {g(1, \"s\")}");
    expect_program(PROLOG "def f(): Unit = ()\n", "f(): Unit = {()}");
    /* konec souboru bez \n */
    expect_program(PROLOG "def f(): Int = 5", "f(): Int = {5}");
}

static void test_block_body(void) {
    expect_program(PROLOG "def f(): Int =\n"
                          "    val a = 1\n"
                          "    return a\n",
                   "f(): Int = {val a = 1; return a}");
    expect_program(PROLOG "def f(): Unit =\n"
                          "    return\n",
                   "f(): Unit = {return}");
    expect_program(PROLOG "def f() =\n"
                          "    while a do\n"
                          "        a = 1\n",
                   "f() = {while a {a = 1}}");
    /* bez \n na konci souboru */
    expect_program(PROLOG "def f() =\n"
                          "    x = 1",
                   "f() = {x = 1}");
}

static void test_params(void) {
    expect_program(PROLOG "def f() = 1\n", "f() = {1}");
    expect_program(PROLOG "def f(a: Int) = 1\n", "f(a: Int) = {1}");
    expect_program(PROLOG "def f(a: Int, b: Double, c: String, d: Unit) = 1\n",
                   "f(a: Int, b: Double, c: String, d: Unit) = {1}");
}

static void test_multiple_functions(void) {
    expect_program(PROLOG "def a() = 1\n"
                          "def b() = 2\n"
                          "def c() = 3\n",
                   "a() = {1} | b() = {2} | c() = {3}");

    /* funkce za blokovou funkcí, main nemusí být první ani poslední */
    expect_program(PROLOG "def helper(x: Int): Int =\n"
                          "    return x\n"
                          "@main def main(): Unit =\n"
                          "    print(1)\n"
                          "def other() = 0\n",
                   "helper(x: Int): Int = {return x} | @main main(): Unit = {print(1)} | "
                   "other() = {0}");
}

/* syntakticky v pořádku - tyhle chyby řeší až sémantická analýza */
static void test_semantic_not_syntax(void) {
    expect_program(PROLOG "def f(a: Int, a: Int) = 1\n", "f(a: Int, a: Int) = {1}"); /* chyba 4 */
    expect_program(PROLOG "def f() = 1\ndef f() = 2\n", "f() = {1} | f() = {2}");    /* chyba 4 */
    expect_program(PROLOG "@main def a() = 1\n@main def b() = 2\n",
                   "@main a() = {1} | @main b() = {2}"); /* chyba 4 */
    expect_program(PROLOG "@main def main(x: Int): Int = x\n",
                   "@main main(x: Int): Int = {x}");     /* chyba 5 a 6 */
    expect_program(PROLOG "def f() = 1\n", "f() = {1}"); /* chybí main - chyba 3 */
}

static void test_fields(void) {
    ProgramRun run;
    run_program(&run, PROLOG "def f(a: Int, b: String): Double = 1\n"
                             "@main def main() =\n"
                             "    x = 1\n");
    assert(run.code == ERR_OK);
    assert(run.result != NULL && run.result->kind == N_PROGRAM);

    Node *f = run.result->body;
    assert(f != NULL && f->kind == N_FUNC);
    assert(strcmp(f->name, "f") == 0);
    assert(f->is_main == 0);
    assert(f->type == TY_DOUBLE);
    assert(f->line == 2 && f->col == 1);

    /* parametr = N_PARAM se jménem, typem a pozicí jména */
    Node *a = f->args;
    assert(a != NULL && a->kind == N_PARAM);
    assert(strcmp(a->name, "a") == 0 && a->type == TY_INT);
    assert(a->line == 2 && a->col == 7);
    Node *b = a->next;
    assert(b != NULL && b->kind == N_PARAM);
    assert(strcmp(b->name, "b") == 0 && b->type == TY_STRING && b->next == NULL);

    /* jednořádkové tělo = seznam o jednom příkazu */
    assert(f->body != NULL && f->body->kind == N_LIT_INT && f->body->next == NULL);

    Node *main_func = f->next;
    assert(main_func != NULL && main_func->kind == N_FUNC);
    assert(strcmp(main_func->name, "main") == 0);
    assert(main_func->is_main == 1);
    assert(main_func->type == TY_UNKNOWN);
    assert(main_func->args == NULL);
    assert(main_func->body != NULL && main_func->body->kind == N_ASSIGN);
    assert(main_func->next == NULL);

    finish(&run);
}

static void test_prolog_errors(void) {
    expect_error("", ERR_SYNTAX);
    expect_error("def f() = 1\n", ERR_SYNTAX);
    expect_error("import ifj25.*\n", ERR_SYNTAX);
    expect_error("import foo.*\n", ERR_SYNTAX);
    expect_error("import ifj26\n", ERR_SYNTAX);
    expect_error("import ifj26.\n", ERR_SYNTAX);
    expect_error("import .*\n", ERR_SYNTAX);
    expect_error("import 5.*\n", ERR_SYNTAX);
    expect_error("import ifj26.*.*\n", ERR_SYNTAX);
    expect_error("import ifj26.* def f() = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "import ifj26.*\n", ERR_SYNTAX);
    expect_error(PROLOG "def f() = 1\nimport ifj26.*\n", ERR_SYNTAX);
}

static void test_main_errors(void) {
    expect_error(PROLOG "@mian def f() = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "@ def f() = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "@main\n", ERR_SYNTAX);
    expect_error(PROLOG "@main x = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "main def f() = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "@main @main def f() = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "@5 def f() = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "@main\n@main\ndef f() = 1\n", ERR_SYNTAX);
    /* mezi @ a main nesmí být žádný znak (spec 2.1) */
    expect_error(PROLOG "@ main def f() = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "@  main\ndef f() = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "@\nmain def f() = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "@/* c */main def f() = 1\n", ERR_SYNTAX);
}

/* Koncová čárka v parametrech (spec 1.2) - ) musí být až na dalším řádku.
 * Vyžaduje lexer, který uvnitř závorek neposílá NEWLINE (feature/lexer od 4956f0b). */
static void test_params_trailing_comma(void) {
    expect_program(PROLOG "def f(a: Int,\n) = 1\n", "f(a: Int) = {1}");
    expect_program(PROLOG "def f(\n    a: Int,\n    b: String,\n): Int = 1\n",
                   "f(a: Int, b: String): Int = {1}");
    expect_program(PROLOG "def f() = g(1,\n    2,\n)\n", "f() = {g(1, 2)}");
    expect_error(PROLOG "def f(a: Int, ) = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f(,\n) = 1\n", ERR_SYNTAX);
}

static void test_header_errors(void) {
    expect_error(PROLOG "def () = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def 5() = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f( = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f() 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f(): = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f(): Foo = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f(): Int\n", ERR_SYNTAX);
    expect_error(PROLOG "def = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def def() = 1\n", ERR_SYNTAX);
}

static void test_param_errors(void) {
    expect_error(PROLOG "def f(a) = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f(a:) = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f(a Int) = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f(a: Foo) = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f(a: Int,) = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f(a: Int b: Int) = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f(, a: Int) = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f(1) = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f(\"a\": Int) = 1\n", ERR_SYNTAX);
}

static void test_body_errors(void) {
    expect_error(PROLOG "def f() =\n", ERR_SYNTAX);              /* prázdný blok */
    expect_error(PROLOG "def f() =\ndef g() = 1\n", ERR_SYNTAX); /* neodsazené tělo */
    expect_error(PROLOG "def f() = 1 2\n", ERR_SYNTAX);          /* dva výrazy */
    expect_error(PROLOG "def f() = val x = 1\n", ERR_SYNTAX);    /* příkaz v jednořádkové */
    expect_error(PROLOG "def f() = return 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f() = 1; def g() = 2\n", ERR_SYNTAX);
    expect_error(PROLOG "def f() = 1 def g() = 2\n", ERR_SYNTAX);
    /* chyba hluboko v těle - rozpracovaný strom se musí uvolnit (ASan) */
    expect_error(PROLOG "def f() = 1\n"
                        "def g() =\n"
                        "    x = 1\n"
                        "    while a do\n"
                        "        y =\n",
                 ERR_SYNTAX);
}

static void test_top_level_errors(void) {
    /* po prologu smí být jen definice funkcí */
    expect_error(PROLOG "x = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "val x = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "print(1)\n", ERR_SYNTAX);
    expect_error(PROLOG "def f() = 1\nx = 1\n", ERR_SYNTAX);
    expect_error(PROLOG "def f() = 1\n5\n", ERR_SYNTAX);
    expect_error(PROLOG "def f() =\n    x = 1\ny = 2\n", ERR_SYNTAX);
}

static void test_lexical_error(void) {
    expect_error(PROLOG "def f() = 1.2.3\n", ERR_LEXICAL);
    expect_error(PROLOG "def f() =\n    x = \"abc\n", ERR_LEXICAL);
}

/* Čeká na opravy lexeru: prázdný řádek a řádek jen s komentářem nemají posílat NEWLINE,
 * blokový komentář se má přeskočit. Do té doby v main zakomentováno. */
static void test_lexer_dependent(void) {
    expect_program("// komentar\n" PROLOG "def f() = 1\n", "f() = {1}");
    expect_program("\n\n" PROLOG "def f() = 1\n", "f() = {1}");
    expect_program("/* blok */ import ifj26.*\ndef f() = 1\n", "f() = {1}");
    expect_program(PROLOG "\n"
                          "def a() = 1\n"
                          "\n"
                          "def b() = 2\n",
                   "a() = {1} | b() = {2}");
    expect_program(PROLOG "// pomocna funkce\n"
                          "def a() = 1\n",
                   "a() = {1}");
}

int main(void) {
    test_prolog();
    test_main();
    test_one_line_body();
    test_block_body();
    test_params();
    test_multiple_functions();
    test_semantic_not_syntax();
    test_fields();
    test_prolog_errors();
    test_main_errors();
    test_params_trailing_comma();
    test_header_errors();
    test_param_errors();
    test_body_errors();
    test_top_level_errors();
    test_lexical_error();
    // TODO: odkomentovat po opravě lexeru (prázdné řádky, komentáře)
    (void)test_lexer_dependent;
    return 0;
}
