#ifndef EXPR_H
#define EXPR_H

#include "ast.h"

/* Zpracuje jeden výraz z tokenů. Vrací 0 (úspěch, *out je kořen stromu)
 * nebo 2 (syntaktická chyba, *out je NULL, nic neuniká). */
int parse_expression(Node **out);

#endif