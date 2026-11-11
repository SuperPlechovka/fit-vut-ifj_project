#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include "precedence_stack.h"
#include "node_stack.h"

int parserInit(Parser *parser, Scanner *scanner)
{
    if (parser == NULL || scanner == NULL)
    {
        return EXIT_FAILURE;
    }

    memset(parser, 0, sizeof(*parser));
    parser->scanner = scanner;

    if (scannerNextToken(scanner, &parser->current) != 0)
    {
        return EXIT_FAILURE;
    }

    if (scannerNextToken(scanner, &parser->lookahead) != 0)
    {
        tokenFree(&parser->current);
        memset(&parser->current, 0, sizeof(parser->current));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void parserDestroy(Parser *parser)
{
    if (parser == NULL)
    {
        return;
    }

    tokenFree(&parser->current);
    tokenFree(&parser->lookahead);
}

int parserAdvance(Parser *parser)
{
    if (parser == NULL || parser->scanner == NULL)
    {
        return EXIT_FAILURE;
    }

    Token next;

    if (scannerNextToken(parser->scanner, &next) != 0)
    {
        return EXIT_FAILURE;
    }
    
    tokenFree(&parser->current);
    parser->current = parser->lookahead;
    memset(&parser->lookahead, 0, sizeof(parser->lookahead));
    parser->lookahead = next;

    return EXIT_SUCCESS;
}

static PrecedenceSymbol expressionInputSymbol(TokenKind kind)
{
    switch (kind)
    {
    case TOKEN_NEWLINE:
    case TOKEN_SEMICOLON:
    case TOKEN_COMMA:
    case TOKEN_DEDENT:
    case TOKEN_EOF:
        return PREC_END;
    
    default:
        return tokenToPrecSymbol(kind);
    }
}

int handleReduce(PrecedenceStack *symbols, NodeStack *nodes, ParseError *error)
{
    if (symbols == NULL || nodes == NULL || error == NULL)
    {
        return EXIT_FAILURE;
    }
    
    PrecedenceSymbol handle[4];
    size_t handle_size = 0;
    PrecedenceSymbol symbol;

    while (precedenceStackPeek(symbols, &symbol) == 0 &&
            symbol != PREC_BOUNDARY)
    {
        if (handle_size >= 4)
        {
            // TODO fill error with something
            return EXIT_FAILURE;
        }

        precedenceStackPop(symbols, &handle[handle_size]);
        handle_size++;
    }
    
    if (symbol != PREC_BOUNDARY)
    {
        // TODO fill error with something
        return EXIT_FAILURE;
    }
    
    precedenceStackPop(symbols, &symbol);

    // flip handle as it's backwards
    for (size_t i = 0; i < handle_size / 2; i++)
    {
        PrecedenceSymbol tmp = handle[i];
        handle[i] = handle[handle_size - 1 - i];
        handle[handle_size - 1 - i] = tmp;
    }
    
    if (handle_size == 1 &&
        handle[0] == PREC_OPERAND)
    {
        // E -> id
    } else if (handle_size == 2 &&
                handle[0] == PREC_UMINUS &&
                handle[1] == PREC_OPERAND)
    {
        // E -> -E
    } else if (handle_size == 3)
    {
        if (handle[0] == PREC_OPERAND &&
            handle[2] == PREC_OPERAND)
        {
            if (handle[1] == PREC_PLUS)
            {
                // E -> E + E
            } else if (handle[1] == PREC_MUL)
            {
                // E -> E * E
            } else if (handle[1] == PREC_DIV)
            {
                // E -> E / E
            } else if (handle[1] == PREC_MINUS)
            {
                // E -> E - E
            } else if (handle[1] == PREC_EQ)
            {
                // E -> E == E
            } else if (handle[1] == PREC_NEQ)
            {
                // E -> E != E
            } else if (handle[1] == PREC_LT)
            {
                // E -> E < E
            } else if (handle[1] == PREC_LE)
            {
                // E -> E <= E
            } else if (handle[1] == PREC_GT)
            {
                // E -> E > E
            } else if (handle[1] == PREC_GE)
            {
                // E -> E >= E
            }
        } else if (handle[0] == PREC_LPAREN &&
                    handle[1] == PREC_OPERAND &&
                    handle[2] == PREC_RPAREN)
        {
            // E -> (E)
        }
    }
}

int parseExpression(Parser *parser, ParseError *error, Node *result)
{
    if (parser == NULL || error == NULL || result == NULL)
    {
        return EXIT_FAILURE;
    }

    PrecedenceStack symbols;
    if (precedenceStackInit(&symbols) == EXIT_FAILURE)
    {
        return EXIT_FAILURE;
    }
    if (precedenceStackPush(&symbols, PREC_END) != 0)
    {
        return EXIT_FAILURE;
    }
    
    NodeStack nodes;
    if (nodeStackInit(&nodes) == EXIT_FAILURE)
    {
        return EXIT_FAILURE;
    }
    PrecedenceSymbol prec_symbol_stack_top;
    if ((precedenceStackPeek(&symbols, &prec_symbol_stack_top) != 0)
        && prec_symbol_stack_top != PREC_END)
    {
        return EXIT_FAILURE;
    }

    while (precedenceStackIsFinished(&symbols) != 0 &&
            tokenToPrecSymbol(parser->current.kind) != PREC_END)
    {
        PrecedenceRelation relation;
        if (getPrecedenceRelation(&symbols, &relation) != 0)
        {
            return EXIT_FAILURE;
        }
        
        switch (relation)
        {
        case REL_SHIFT:
            if (precedenceStackPushAfterTopTerminal(
                &symbols, PREC_BOUNDARY) != EXIT_SUCCESS)
            {
                // TODO fill error with something
                return EXIT_FAILURE;
            }
            
        case REL_EQUAL:
            precedenceStackPush(&symbols, tokenToPrecSymbol(parser->current.kind));
            parserAdvance(parser);
            break;
        case REL_REDUCE:
            handleReduce(&symbols, &nodes, error);

            break;
        
        default:
            // TODO fill error with something
            return EXIT_FAILURE;
        }
    }
};
