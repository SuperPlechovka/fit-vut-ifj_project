#ifndef EXPRESSION_PARSER_H
#define EXPRESSION_PARSER_H

#include <stddef.h>
#include "../tmp/ast.h"
#include "parser.h"
#include "error.h"

ErrorCode parseExpression(Parser *parser, ParseError *error, Node **result);

#endif
