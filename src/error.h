#ifndef ERROR_H
#define ERROR_H

/** Návratové kódy překladače podle specifikace IFJ26 (sekce 6). */
typedef enum {
    ERR_OK = 0,
    ERR_LEXICAL = 1,         // chybná struktura lexému, komentáře nebo odsazení
    ERR_SYNTAX = 2,          // chybná syntaxe, chybějící prolog, porušení struktury bloků
    ERR_UNDEFINED = 3,       // nedefinovaná funkce/proměnná/typ, chybějící @main
    ERR_REDEFINITION = 4,    // redefinice funkce, proměnné, typu
    ERR_CALL = 5,            // špatný počet/typ argumentů, parametry u @main
    ERR_TYPE = 6,            // typová kompatibilita nebo odvození typu
    ERR_IMMUTABLE = 7,       // zápis do val nebo do parametru funkce
    ERR_SEMANTIC_OTHER = 10, // ostatní sémantické chyby (např. nepovolený return)
    ERR_INTERNAL = 99,       // interní chyba překladače (např. alokace)
} ErrorCode;

#endif
