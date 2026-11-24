#ifndef SEMANTIC_ANALYSIS_H
#define SEMANTIC_ANALYSIS_H

#include "error.h"
#include "ast.h"
#include "parser.h"


ErrorCode analyseSemantics(Node *program, ParseError *error);


#endif
