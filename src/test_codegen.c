#include "ast.h"
#include "ilist.h"
#include "insidecodegene.h"
#include <stdio.h>
#include <stdlib.h>

/*AI: please generate tests for my code*/
static int raw = 0;

static void runTest(const char *test_name, Node *prog) {
    ListOfInstr out;
    listInit(&out);
    int res = codegenProgram(prog, &out);

    if (raw) {
        if (res == 0) {
            printf(".IFJcode26\n");
            listPrint(&out, stdout);
        }
        listFree(&out);
        nodeFree(prog);
        return;
    }

    printf("========================================\n");
    printf("TEST: %s\n", test_name);
    printf("========================================\n");

    if (res == 0) {
        printf("[PASS] Generování kódu proběhlo úspěšně!\n");
        printf("--- Vygenerovaný vnitřní 3-adresný kód ---\n");
        listPrint(&out, stdout);
    } else {
        printf("[FAIL] Chyba při generování kódu: %d\n", res);
    }

    printf("\n");
    listFree(&out);
    nodeFree(prog);
}

/* TEST 1: Aritmetika, přiřazení a write */
static void testExpressions(void) {
    Node *body = NULL;

    /* var x : Int = 5 */
    nodeAppend(&body, nodeDecl("x", 1, TY_INT, nodeLitInt(5)));

    /* var y : Int = 10 */
    nodeAppend(&body, nodeDecl("y", 1, TY_INT, nodeLitInt(10)));

    /* x = x + y */
    Node *add_expr = nodeBinary(OP_ADD, nodeVar("x"), nodeVar("y"));
    add_expr->type = TY_INT;
    nodeAppend(&body, nodeAssign("x", add_expr));

    /* write(x) */
    nodeAppend(&body, nodeWrite(nodeVar("x")));

    Node *main_func = nodeFunc("main", 1, TY_UNIT, body);
    Node *prog = nodeProgram(main_func);

    runTest("Aritmetika a přiřazení v main()", prog);
}

/* TEST 2: Funkce s parametry a návratovou hodnotou */
static void testFunctionCallWithParams(void) {
    Node *funcs = NULL;

    /* func add(a: Int, b: Int) : Int { return a + b } */
    Node *param_a = nodeDecl("a", 0, TY_INT, NULL);
    Node *param_b = nodeDecl("b", 0, TY_INT, NULL);
    nodeAppend(&param_a, param_b);

    Node *add_expr = nodeBinary(OP_ADD, nodeVar("a"), nodeVar("b"));
    add_expr->type = TY_INT;
    Node *add_body = nodeReturn(add_expr);

    Node *func_add = nodeFunc("add", 0, TY_INT, add_body);
    func_add->args = param_a;
    nodeAppend(&funcs, func_add);

    /* main() { var res : Int = add(10, 20); write(res); } */
    Node *main_body = NULL;

    Node *arg1 = nodeLitInt(10);
    Node *arg2 = nodeLitInt(20);
    nodeAppend(&arg1, arg2);

    Node *call_add = nodeCall("add", arg1);
    call_add->type = TY_INT;

    nodeAppend(&main_body, nodeDecl("res", 1, TY_INT, call_add));
    nodeAppend(&main_body, nodeWrite(nodeVar("res")));

    nodeAppend(&funcs, nodeFunc("main", 1, TY_UNIT, main_body));

    Node *prog = nodeProgram(funcs);

    runTest("Předávání parametrů a volání add(10, 20)", prog);
}

/* TEST 3: Unární mínus */
static void testUnary(void) {
    Node *body = NULL;

    /* var x : Int = -5 */
    Node *un_expr = nodeUnary(OP_NEG, nodeLitInt(5));
    un_expr->type = TY_INT;

    nodeAppend(&body, nodeDecl("x", 1, TY_INT, un_expr));
    nodeAppend(&body, nodeWrite(nodeVar("x")));

    Node *main_func = nodeFunc("main", 1, TY_UNIT, body);
    Node *prog = nodeProgram(main_func);

    runTest("Unární operátor (-5)", prog);
}

/* TEST 4: Relační konverze (1 < 2.5 -> INT2FLOAT) */
static void testRelationalCoercion(void) {
    Node *body = NULL;

    /* var res : Bool = 1 < 2.5 */
    Node *left_lit = nodeLitInt(1);
    Node *right_lit = nodeLitDouble(2.5);

    Node *rel_expr = nodeBinary(OP_LT, left_lit, right_lit);
    rel_expr->type = TY_BOOL;

    nodeAppend(&body, nodeDecl("res", 1, TY_BOOL, rel_expr));

    Node *main_func = nodeFunc("main", 1, TY_UNIT, body);
    Node *prog = nodeProgram(main_func);

    runTest("Implicitní konverze u porovnání (1 < 2.5)", prog);
}

/* TEST 5: Řídicí struktury (while cyklus a if-else) */
static void testControlFlow(void) {
    Node *body = NULL;

    /* var i : Int = 0 */
    nodeAppend(&body, nodeDecl("i", 1, TY_INT, nodeLitInt(0)));

    /* while (i < 3) { write(i); i = i + 1; } */
    Node *cond_while = nodeBinary(OP_LT, nodeVar("i"), nodeLitInt(3));
    cond_while->type = TY_BOOL;

    Node *while_body = NULL;
    nodeAppend(&while_body, nodeWrite(nodeVar("i")));

    Node *inc_expr = nodeBinary(OP_ADD, nodeVar("i"), nodeLitInt(1));
    inc_expr->type = TY_INT;
    nodeAppend(&while_body, nodeAssign("i", inc_expr));

    nodeAppend(&body, nodeWhile(cond_while, while_body));

    /* if (i == 3) { write(1) } else { write(0) } */
    Node *cond_if = nodeBinary(OP_EQ, nodeVar("i"), nodeLitInt(3));
    cond_if->type = TY_BOOL;

    Node *then_b = nodeWrite(nodeLitInt(1));
    Node *else_b = nodeWrite(nodeLitInt(0));

    nodeAppend(&body, nodeIf(cond_if, then_b, else_b));

    Node *main_func = nodeFunc("main", 1, TY_UNIT, body);
    Node *prog = nodeProgram(main_func);

    runTest("Řídicí struktury (while cyklus a if-else)", prog);
}

/* TEST 6: Logické operátory (AND / OR se short-circuiting) */
static void testShortCircuit(void) {
    Node *body = NULL;

    /* var res1 : Bool = (1 < 2) && (3 == 3) */
    Node *e1 = nodeBinary(OP_LT, nodeLitInt(1), nodeLitInt(2));
    e1->type = TY_BOOL;
    Node *e2 = nodeBinary(OP_EQ, nodeLitInt(3), nodeLitInt(3));
    e2->type = TY_BOOL;

    Node *and_expr = nodeBinary(OP_AND, e1, e2);
    and_expr->type = TY_BOOL;

    nodeAppend(&body, nodeDecl("res1", 1, TY_BOOL, and_expr));

    /* var res2 : Bool = (1 == 1) || (5 < 2) */
    Node *e3 = nodeBinary(OP_EQ, nodeLitInt(1), nodeLitInt(1));
    e3->type = TY_BOOL;
    Node *e4 = nodeBinary(OP_LT, nodeLitInt(5), nodeLitInt(2));
    e4->type = TY_BOOL;

    Node *or_expr = nodeBinary(OP_OR, e3, e4);
    or_expr->type = TY_BOOL;

    nodeAppend(&body, nodeDecl("res2", 1, TY_BOOL, or_expr));

    Node *main_func = nodeFunc("main", 1, TY_UNIT, body);
    Node *prog = nodeProgram(main_func);

    runTest("Logické operátory AND / OR (zkrácené vyhodnocování)", prog);
}
/* TEST 7: Neznámý / Neplatný uzel v AST (Chybový stav + paměťová čistota pro ASan) */
static void testUnknownNodeError(void) {
    printf("========================================\n");
    printf("TEST: Neznámý uzel v AST (Chybový kód + ASan)\n");
    printf("========================================\n");

    /* Vytvoření neplatného uzlu s neexistujícím NodeKind */
    Node *bad_node = nodeNew((NodeKind)999);
    Node *main_body = NULL;
    nodeAppend(&main_body, bad_node);

    Node *main_func = nodeFunc("main", 1, TY_UNIT, main_body);
    Node *prog = nodeProgram(main_func);

    ListOfInstr out;
    listInit(&out);

    int res = codegenProgram(prog, &out);

    if (res != 0) {
        printf("[PASS] Generátor správně zachytil neplatný uzel a vrátil chybový kód: %d\n", res);
    } else {
        printf("[FAIL] Generátor měl selhat na neplatném uzlu, ale vrátil 0!\n");
    }

    /* Uvolnění paměti - ASan zkontroluje, že nezůstal žádný memory leak */
    listFree(&out);
    nodeFree(prog);
    printf("\n");
}
static void exportTestToFile(const char *filename, Node *prog) {
    ListOfInstr out;
    listInit(&out);

    if (codegenProgram(prog, &out) == 0) {
        FILE *f = fopen(filename, "w");
        if (f != NULL) {
            /* Povinná hlavička cílového jazyka IFJcode26 */
            fprintf(f, ".IFJcode26\n");
            listPrint(&out, f);
            fclose(f);
            printf("[INFO] Kód úspěšně uložen do souboru: %s\n", filename);
        } else {
            fprintf(stderr, "Chyba při otevírání souboru %s pro zápis\n", filename);
        }
    }
    listFree(&out);
    nodeFree(prog);
}
static void generateToFile(void) {
    Node *body = NULL;

    /* var x : Int = 5 */
    nodeAppend(&body, nodeDecl("x", 1, TY_INT, nodeLitInt(5)));
    /* var y : Int = 10 */
    nodeAppend(&body, nodeDecl("y", 1, TY_INT, nodeLitInt(10)));
    /* x = x + y */
    Node *add = nodeBinary(OP_ADD, nodeVar("x"), nodeVar("y"));
    add->type = TY_INT;
    nodeAppend(&body, nodeAssign("x", add));
    /* write(x) -> vytiskne 15 */
    nodeAppend(&body, nodeWrite(nodeVar("x")));

    Node *main_func = nodeFunc("main", 1, TY_UNIT, body);
    Node *prog = nodeProgram(main_func);

    exportTestToFile("main_test.ifjcode", prog);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        raw = 1;
        switch (argv[1][0]) {
        case '1':
            testExpressions();
            break;
        case '2':
            testFunctionCallWithParams();
            break;
        case '3':
            testUnary();
            break;
        case '4':
            testRelationalCoercion();
            break;
        case '5':
            testControlFlow();
            break;
        case '6':
            testShortCircuit();
            break;
        default:
            return 1;
        }
        return 0;
    }
    generateToFile();
    testExpressions();
    testFunctionCallWithParams();
    testUnary();
    testRelationalCoercion();
    testControlFlow();
    testShortCircuit();
    testUnknownNodeError();

    return 0;
}