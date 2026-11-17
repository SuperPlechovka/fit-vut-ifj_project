# LL gramatika IFJ26

## Legenda

| terminál v gramatice | token |
|---|---|
| `id` | `TOK_IDENTIFIER` |
| `int_lit`, `double_lit`, `string_lit` | `TOK_INT_LITERAL`, `TOK_DOUBLE_LITERAL`, `TOK_STRING_LITERAL` |
| `import def val var if then else while do return` | `TOK_KW_*` |
| `Int Double String Unit` | `TOK_KW_INT`, `TOK_KW_DOUBLE`, `TOK_KW_STRING`, `TOK_KW_UNIT` |
| `( ) , : ; . * = @` | `TOK_LPAREN`, `TOK_RPAREN`, `TOK_COMMA`, `TOK_COLON`, `TOK_SEMICOLON`, `TOK_DOT`, `TOK_STAR`, `TOK_ASSIGN`, `TOK_AT` |
| `NEWLINE INDENT DEDENT EOF` | `TOK_NEWLINE`, `TOK_INDENT`, `TOK_DEDENT`, `TOK_EOF` |
| `expr` | výraz, zpracuje ho precedenční analýza (`parseExpression`); patří sem i literál `()` |
| `eps` | prázdný řetězec |

## Pravidla

```
Program a funkce
 1. <program>         -> <prolog> <funcs> EOF
 2. <prolog>          -> import id . * NEWLINE          // id musí být "ifj26", jinak chyba 2
 3. <funcs>           -> <func_def> <funcs>
 4. <funcs>           -> eps
 5. <func_def>        -> <main_opt> def id ( <params> ) <type_opt> = <func_body>
 6. <main_opt>        -> @ id <nl_opt>                  // id musí být "main", jinak chyba 2
 7. <main_opt>        -> eps
 8. <nl_opt>          -> NEWLINE
 9. <nl_opt>          -> eps
10. <func_body>       -> <expr_or_call> NEWLINE         // jednořádková funkce
11. <func_body>       -> <block>                        // víceřádková funkce

Parametry a typy
12. <params>          -> <param> <params_tail>
13. <params>          -> eps
14. <params_tail>     -> , <param> <params_tail>
15. <params_tail>     -> eps
16. <param>           -> id : <type>
17. <type_opt>        -> : <type>
18. <type_opt>        -> eps
19. <type>            -> Int
20. <type>            -> Double
21. <type>            -> String
22. <type>            -> Unit

Bloky a příkazy
23. <block>           -> NEWLINE INDENT <statements> DEDENT
24. <statements>      -> <statement> <statements_tail>
25. <statements_tail> -> <statement> <statements_tail>
26. <statements_tail> -> eps
27. <statement>       -> <simple_stmt> <stmt_sep>
28. <statement>       -> <if>
29. <statement>       -> <while>
30. <stmt_sep>        -> NEWLINE
31. <stmt_sep>        -> ; <nl_opt>
32. <simple_stmt>     -> <var_def>
33. <simple_stmt>     -> <return>
34. <simple_stmt>     -> id = <expr_or_call>            // LL(2): id a za ním =
35. <simple_stmt>     -> <expr_or_call>
36. <var_def>         -> <var_kind> id <type_opt> = <expr_or_call>
37. <var_kind>        -> val
38. <var_kind>        -> var
39. <return>          -> return <ret_value>
40. <ret_value>       -> <expr_or_call>
41. <ret_value>       -> eps
42. <if>              -> if expr then <block> else <block>
43. <while>           -> while expr do <block>

Výraz nebo volání
44. <expr_or_call>    -> <call>                         // LL(2): id a za ním (
45. <expr_or_call>    -> expr
46. <call>            -> id ( <args> )
47. <args>            -> <term> <args_tail>
48. <args>            -> eps
49. <args_tail>       -> , <term> <args_tail>
50. <args_tail>       -> eps
51. <term>            -> id
52. <term>            -> int_lit
53. <term>            -> double_lit
54. <term>            -> string_lit
```

## Poznámky

Gramatika je LL(1) až na dvě místa, kde více pravidel začíná terminálem `id`:

| neterminál | pravidla | rozhodnutí podle druhého tokenu (`parser->lookahead`) |
|---|---|---|
| `<simple_stmt>` | 34 (`id =`), 35 (`<expr_or_call>`) | `=` -> pravidlo 34, jinak 35 |
| `<expr_or_call>` | 44 (`<call>`), 45 (`expr`) | `(` -> pravidlo 44, jinak 45 |

V těchto dvou místech je tedy analýza LL(2), všude jinde stačí jeden token (`parser->current`).
