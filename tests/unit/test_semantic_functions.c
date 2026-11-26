/**
 * @file
 * @brief Unit testy sémantické analýzy - průchod 1: funkce, vestavěné funkce, @main.
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
/* AI prompt: připrav unit testy na sémantiku (průchod 1 analyseSemantics) podle
 * specifikace IFJ26, sekce 2 a 2.3 a 4. Ověř:
 * - platné programy (libovolné jméno @main, více funkcí, parametry, volání před definicí);
 * - chybějící @main - kód 3; více @main - kód 4; @main s parametrem - kód 5;
 *   @main s jiným návratovým typem než Unit - kód 6;
 * - redefinici funkce se stejnými i jinými parametry (přetěžování) - kód 4;
 * - redefinici vestavěné funkce (stejný i jiný počet parametrů) - kód 4;
 * - duplicitní jméno parametru - kód 4;
 * - pozici chyby (druhá definice, druhý parametr).
 * Těla funkcí jsou triviální, aby testy nezávisely na průchodu 2 ani na chybách lexeru
 * (prázdné řádky, DEDENT). Program se nejdřív musí zparsovat bez chyby.
 * Model: Claude Opus 5.5, Claude Code.
 */
#include "../../src/parser.h"
#include "../../src/semantic_analysis.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *input;
    Lexer lexer;
    Parser parser;
    ParseError error;
    Node *program;
    ErrorCode code;
} SemanticRun;

/* Zparsuje text (musí projít bez chyby) a spustí na něm sémantickou analýzu. */
static void run_semantic(SemanticRun *run, const char *text) {
    memset(run, 0, sizeof(*run));
    run->input = tmpfile();
    assert(run->input != NULL);
    assert(fputs(text, run->input) >= 0);
    rewind(run->input);

    assert(lexer_init(&run->lexer, run->input) == LEXER_OK);
    assert(parserInit(&run->parser, &run->lexer, &run->error) == ERR_OK);

    ErrorCode parse_code = parseProgram(&run->parser, &run->error, &run->program);
    if (parse_code != ERR_OK) {
        fprintf(stderr, "test je špatně - program se nezparsoval (chyba %d, %zu:%zu %s):\n%s\n",
                parse_code, run->error.line, run->error.column,
                run->error.message != NULL ? run->error.message : "", text);
    }
    assert(parse_code == ERR_OK);

    run->code = analyseSemantics(run->program, &run->error);
}

static void finish(SemanticRun *run) {
    nodeFree(run->program);
    parserDestroy(&run->parser);
    lexer_destroy(&run->lexer);
    fclose(run->input);
}

static void expect_ok(const char *text) {
    SemanticRun run;
    run_semantic(&run, text);
    if (run.code != ERR_OK) {
        fprintf(stderr, "očekáváno OK, chyba %d (%zu:%zu %s):\n%s\n", run.code, run.error.line,
                run.error.column, run.error.message != NULL ? run.error.message : "", text);
    }
    assert(run.code == ERR_OK);
    assert(run.error.code == ERR_OK);
    finish(&run);
}

static void expect_error(const char *text, ErrorCode expected) {
    SemanticRun run;
    run_semantic(&run, text);
    if (run.code != expected) {
        fprintf(stderr, "očekávána chyba %d, dostal %d (%s):\n%s\n", expected, run.code,
                run.error.message != NULL ? run.error.message : "", text);
    }
    assert(run.code == expected);
    assert((int)run.error.code == (int)expected);
    finish(&run);
}

/* Navíc ověří pozici chyby (řádek, sloupec). */
static void expect_error_at(const char *text, ErrorCode expected, size_t line, size_t column) {
    SemanticRun run;
    run_semantic(&run, text);
    if (run.code != expected || run.error.line != line || run.error.column != column) {
        fprintf(stderr, "očekávána chyba %d na %zu:%zu, dostal %d na %zu:%zu:\n%s\n", expected,
                line, column, run.code, run.error.line, run.error.column, text);
    }
    assert(run.code == expected);
    assert(run.error.line == line && run.error.column == column);
    finish(&run);
}

static void test_valid_programs(void) {
    expect_ok("import ifj26.*\n"
              "@main def run(): Unit = ()\n");
    /* jméno vstupní funkce je libovolné, @main může být na samostatném řádku */
    expect_ok("import ifj26.*\n"
              "@main\n"
              "def main(): Unit = ()\n");
    /* více funkcí, parametry všech typů, funkce za @main */
    expect_ok("import ifj26.*\n"
              "def f(a: Int, b: Double, c: String, d: Unit): Int = 1\n"
              "@main def run(): Unit = ()\n"
              "def g(): String = \"s\"\n");
    /* stejné jméno parametru v různých funkcích je v pořádku */
    expect_ok("import ifj26.*\n"
              "def f(x: Int): Int = x\n"
              "def g(x: Int): Int = x\n"
              "@main def run(): Unit = ()\n");
    /* parametr se smí jmenovat jako funkce (jiný jmenný prostor) */
    expect_ok("import ifj26.*\n"
              "def f(print: Int): Int = 1\n"
              "@main def run(): Unit = ()\n");
}

static void test_main(void) {
    /* chybějící vstupní funkce - 3 */
    expect_error("import ifj26.*\n", ERR_UNDEFINED);
    expect_error("import ifj26.*\n"
                 "def run(): Unit = ()\n",
                 ERR_UNDEFINED);

    /* více vstupních funkcí - 4, hlásí se u druhé (pozice def) */
    expect_error_at("import ifj26.*\n"
                    "@main def a(): Unit = ()\n"
                    "@main def b(): Unit = ()\n",
                    ERR_REDEFINITION, 3, 7);

    /* parametr u @main - 5 */
    expect_error("import ifj26.*\n"
                 "@main def run(x: Int): Unit = ()\n",
                 ERR_CALL);

    /* jiný návratový typ než Unit - 6 */
    expect_error("import ifj26.*\n"
                 "@main def run(): Int = 1\n",
                 ERR_TYPE);
    expect_error("import ifj26.*\n"
                 "@main def run(): String = \"s\"\n",
                 ERR_TYPE);
}

static void test_function_redefinition(void) {
    /* stejná signatura - 4, hlásí se u druhé definice */
    expect_error_at("import ifj26.*\n"
                    "def f(): Int = 1\n"
                    "def f(): Int = 2\n"
                    "@main def run(): Unit = ()\n",
                    ERR_REDEFINITION, 3, 1);
    /* jiný návratový typ, stejné parametry - 4 */
    expect_error("import ifj26.*\n"
                 "def f(a: Int): Int = 1\n"
                 "def f(b: Int): String = \"s\"\n"
                 "@main def run(): Unit = ()\n",
                 ERR_REDEFINITION);
    /* přetěžování jiným počtem parametrů - 4 (tabulka to jako duplicitu nepozná!) */
    expect_error_at("import ifj26.*\n"
                    "def f(a: Int): Int = 1\n"
                    "def f(a: Int, b: Int): Int = 2\n"
                    "@main def run(): Unit = ()\n",
                    ERR_REDEFINITION, 3, 1);
    expect_error("import ifj26.*\n"
                 "def f(a: Int, b: Int): Int = 2\n"
                 "def f(): Int = 1\n"
                 "@main def run(): Unit = ()\n",
                 ERR_REDEFINITION);
    /* přetěžování jinými typy parametrů - 4 */
    expect_error("import ifj26.*\n"
                 "def f(a: Int): Int = 1\n"
                 "def f(a: String): Int = 2\n"
                 "@main def run(): Unit = ()\n",
                 ERR_REDEFINITION);
    /* vstupní funkce se stejným jménem jako jiná funkce - 4 */
    expect_error("import ifj26.*\n"
                 "def run(a: Int): Int = 1\n"
                 "@main def run(): Unit = ()\n",
                 ERR_REDEFINITION);
}

static void test_builtin_redefinition(void) {
    /* stejný počet parametrů jako vestavěná funkce */
    expect_error_at("import ifj26.*\n"
                    "def print(x: Int): Unit = ()\n"
                    "@main def run(): Unit = ()\n",
                    ERR_REDEFINITION, 2, 1);
    expect_error("import ifj26.*\n"
                 "def readLine(): String = \"s\"\n"
                 "@main def run(): Unit = ()\n",
                 ERR_REDEFINITION);
    /* jiný počet parametrů než vestavěná funkce - pořád přetěžování */
    expect_error("import ifj26.*\n"
                 "def print(): Unit = ()\n"
                 "@main def run(): Unit = ()\n",
                 ERR_REDEFINITION);
    expect_error("import ifj26.*\n"
                 "def subStr(s: String): String = s\n"
                 "@main def run(): Unit = ()\n",
                 ERR_REDEFINITION);
    /* vstupní funkce pojmenovaná jako vestavěná */
    expect_error("import ifj26.*\n"
                 "@main def chr(): Unit = ()\n",
                 ERR_REDEFINITION);
}

static void test_parameters(void) {
    /* duplicitní jméno parametru - 4, hlásí se u druhého parametru */
    expect_error_at("import ifj26.*\n"
                    "def f(a: Int, a: Int): Int = 1\n"
                    "@main def run(): Unit = ()\n",
                    ERR_REDEFINITION, 2, 15);
    expect_error("import ifj26.*\n"
                 "def f(a: Int, b: String, a: Double): Int = 1\n"
                 "@main def run(): Unit = ()\n",
                 ERR_REDEFINITION);
}

int main(void) {
    test_valid_programs();
    test_main();
    test_function_redefinition();
    test_builtin_redefinition();
    test_parameters();
    return 0;
}
