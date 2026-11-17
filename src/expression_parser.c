/**
 * @file
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
#include "ast.h"
#include "error.h"
#include "node_stack.h"
#include "parser.h"
#include "precedence_stack.h"
#include <stdlib.h>
#include <string.h>

// like tokenToPrecSymbol, but tokens that do not
// belong in an expression are converted to PREC_END instead
static PrecedenceSymbol tokenToPrecSymbolOrEnd(TokenType kind) {
    PrecedenceSymbol symbol = tokenToPrecSymbol(kind);
    return symbol == PREC_INVALID ? PREC_END : symbol;
}

static OpKind binaryOpFromPrec(PrecedenceSymbol symbol) {
    switch (symbol) {
    case PREC_PLUS:
        return OP_ADD;
    case PREC_MINUS:
        return OP_SUB;
    case PREC_MUL:
        return OP_MUL;
    case PREC_DIV:
        return OP_DIV;
    case PREC_LT:
        return OP_LT;
    case PREC_LE:
        return OP_LE;
    case PREC_GT:
        return OP_GT;
    case PREC_GE:
        return OP_GE;
    case PREC_EQ:
        return OP_EQ;
    case PREC_NEQ:
        return OP_NE;
    default:
        return OP_NONE;
    }
}

static ErrorCode handleReduce(PrecedenceStack *symbols, NodeStack *nodes, ParseError *error,
                              const Token *token, SourcePos lparen) {
    if (symbols == NULL || nodes == NULL || error == NULL) {
        return setParserError(error, ERR_INTERNAL, token, "invalid function arguments");
    }

    PrecedenceSymbol handle[4];
    size_t handle_size = 0;
    PrecedenceSymbol symbol;

    while (precedenceStackPeek(symbols, &symbol) == 0 && symbol != PREC_BOUNDARY) {
        if (handle_size >= 4) {
            return setParserError(error, ERR_SYNTAX, token, "Invalid expression");
        }

        precedenceStackPop(symbols, &handle[handle_size]);
        handle_size++;
    }

    if (symbol != PREC_BOUNDARY) {
        return setParserError(error, ERR_INTERNAL, token, "Invalid expression");
    }

    precedenceStackPop(symbols, &symbol);

    // flip handle as it's backwards
    for (size_t i = 0; i < handle_size / 2; i++) {
        PrecedenceSymbol tmp = handle[i];
        handle[i] = handle[handle_size - 1 - i];
        handle[handle_size - 1 - i] = tmp;
    }

    if (handle_size == 1 && handle[0] == PREC_OPERAND) {
        // E -> id
    } else if (handle_size == 2 && handle[0] == PREC_UMINUS && handle[1] == PREC_NONTERMINAL) {
        // E -> -E
        Node *operand;

        if (nodeStackPop(nodes, &operand) != EXIT_SUCCESS) {
            return setParserError(error, ERR_INTERNAL, token, "invalid stack pop arguments");
        }

        int line = operand->line, col = operand->col;
        Node *unary = nodeUnary(OP_NEG, operand);
        nodeSetPos(unary, line, col);

        if (unary == NULL || nodeStackPush(nodes, unary) != EXIT_SUCCESS) {
            nodeFree(unary);
            return setParserError(error, ERR_INTERNAL, token,
                                  "out of memory (failed to push onto stack)");
        }
    } else if (handle_size == 2 && handle[0] == PREC_LPAREN && handle[1] == PREC_RPAREN) {
        // E -> ()
        Node *n = nodeLitUnit();

        if (n == NULL || nodeStackPush(nodes, n) != EXIT_SUCCESS) {
            nodeFree(n);
            return setParserError(error, ERR_INTERNAL, token,
                                  "out of memory ( failed to push onto stack )");
        }

        nodeSetPos(n, (int)lparen.line, (int)lparen.column);

    } else if (handle_size == 3 && handle[0] == PREC_NONTERMINAL && handle[2] == PREC_NONTERMINAL &&
               binaryOpFromPrec(handle[1]) != OP_NONE) {
        // E -> E op E
        Node *right, *left;

        if (nodeStackPop(nodes, &right) != EXIT_SUCCESS) {
            return setParserError(error, ERR_INTERNAL, token, "invalid stack pop arguments");
        }

        if (nodeStackPop(nodes, &left) != EXIT_SUCCESS) {
            nodeFree(right);
            return setParserError(error, ERR_INTERNAL, token, "invalid stack pop arguments");
        }

        int line = left->line, col = left->col;
        Node *binary = nodeBinary(binaryOpFromPrec(handle[1]), left, right);
        nodeSetPos(binary, line, col);

        if (binary == NULL || nodeStackPush(nodes, binary) != EXIT_SUCCESS) {
            nodeFree(binary);
            return setParserError(error, ERR_INTERNAL, token,
                                  "out of memory (failed to push onto stack)");
        }
    } else if (handle_size == 3 && handle[0] == PREC_LPAREN && handle[1] == PREC_NONTERMINAL &&
               handle[2] == PREC_RPAREN) {
        // E -> (E)
    } else {
        return setParserError(error, ERR_SYNTAX, token, "unsupported precedence rule");
    }

    if (precedenceStackPush(symbols, PREC_NONTERMINAL) != EXIT_SUCCESS) {
        return setParserError(error, ERR_INTERNAL, token,
                              "out of memory (failed to push onto stack)");
    }

    return ERR_OK;
}

static ErrorCode runPrecedenceAnalysis(Parser *parser, PrecedenceStack *symbols, NodeStack *nodes,
                                       ParseError *error) {
    if (precedenceStackPush(symbols, PREC_END) != EXIT_SUCCESS) {
        return setParserError(error, ERR_INTERNAL, &parser->current,
                              "out of memory (failed to push onto stack)");
    }

    SourcePos lparen = {0};

    while (true) {
        PrecedenceSymbol top;
        if (stackTopTerminal(symbols, &top) != EXIT_SUCCESS) {
            return setParserError(error, ERR_INTERNAL, NULL, "no terminals on precedence stack");
        }
        PrecedenceSymbol input = tokenToPrecSymbolOrEnd(parser->current.type);

        if (input == PREC_MINUS) {
            PrecedenceSymbol prev;
            if (precedenceStackPeek(symbols, &prev) != EXIT_SUCCESS) {
                return setParserError(error, ERR_INTERNAL, NULL, "empty precedence stack");
            }
            if (prev != PREC_OPERAND && prev != PREC_RPAREN && prev != PREC_NONTERMINAL) {
                input = PREC_UMINUS;
            }
        }

        if (top == PREC_END && input == PREC_END) {
            break; // end of expression
        }

        switch (getPrecedenceRelation(top, input)) {
        case REL_SHIFT:
            if (precedenceStackPushAfterTopTerminal(symbols, PREC_BOUNDARY) != EXIT_SUCCESS) {
                return setParserError(error, ERR_INTERNAL, NULL,
                                      "out of memory (failed to push onto stack)");
            }
        // fall-through
        case REL_EQUAL:
            if (input == PREC_LPAREN) {
                lparen.line = parser->current.line;
                lparen.column = parser->current.column;
            }

            if (input == PREC_OPERAND) {
                Node *leaf = nodeFromToken(parser);
                if (leaf == NULL || nodeStackPush(nodes, leaf) != EXIT_SUCCESS) {
                    nodeFree(leaf);
                    return setParserError(error, ERR_INTERNAL, NULL,
                                          "out of memory (failed to push onto stack)");
                }
            }

            if (precedenceStackPush(symbols, input) != EXIT_SUCCESS) {
                return setParserError(error, ERR_INTERNAL, NULL,
                                      "out of memory (failed to push onto stack)");
            }

            ErrorCode rc = parserAdvance(parser, error);
            if (rc != ERR_OK) {
                return rc;
            }

            break;
        case REL_REDUCE: {
            ErrorCode rc = handleReduce(symbols, nodes, error, &parser->current, lparen);
            if (rc != ERR_OK) {
                return rc;
            }
            break;
        }

        default: // REL_ERROR
            return setParserError(error, ERR_SYNTAX, &parser->current,
                                  "invalid expression (undefined precedence table combination)");
        }
    }

    if (!precedenceStackIsFinished(symbols)) {
        return setParserError(error, ERR_SYNTAX, &parser->current, "expression is empty");
    }

    return ERR_OK;
}

ErrorCode parseExpression(Parser *parser, ParseError *error, Node **result) {
    if (parser == NULL || result == NULL) {
        return setParserError(error, ERR_INTERNAL, NULL, "invalid parser arguments");
    }

    *result = NULL;

    PrecedenceStack symbols = {0};
    NodeStack nodes = {0};
    ErrorCode rc;

    if (precedenceStackInit(&symbols) != EXIT_SUCCESS || nodeStackInit(&nodes) != EXIT_SUCCESS) {
        rc = setParserError(error, ERR_INTERNAL, NULL, "failed to allocate memory");
    } else {
        rc = runPrecedenceAnalysis(parser, &symbols, &nodes, error);
    }

    if (rc == ERR_OK && nodeStackPop(&nodes, result) != EXIT_SUCCESS) {
        rc = setParserError(error, ERR_INTERNAL, NULL, "expression did not result in a tree");
    }

    nodeStackDestroy(&nodes);
    precedenceStackDestroy(&symbols);
    return rc;
}
