# Syntax

## DATA FLOW PROCESSING

All the data are carried a packet.

## Local variables

    x: i32
    y: i32 = 1


## Pointer

    x: i32*
    y: i32
    y = @x

    x: i32
    y: i32*
    y = &x

## Div/Mod operation

    q = x / y
    r = x % y

    q, r = x / y
    r, q = x % y

## List expression

    a, b = x + y, x + z

## if-else and switch expressions

    r = if condition { true } else { false }

    r = switch ternary { case 0, nil: false; case 1: true }

## Precedence of operators

1. `!` `~` unary ops
1. `*` `/` `%`
1. `+` `-`
1. `<<` `>>`
1. `<` `<=` `>` `>=`
1. `==` `!=`
1. `&`
1. `^`
1. `|`
1. `&&`
1. `||`
1. `:=`
1. `,`


## BNF

    token ::=
            "nil" | "true" | "false"
             | "fn" | "coroutine" | "return" | "continue" | "break"
             | "if" | "else" | "while" | "for" | "switch" | "case"
             | "-" | "+" | "*" | "/" | "%" | "&" | "|" | "~" | "^"
             | "," | "." | "!" | "!=" | "@"
             | "<" | "<<" | "<=" | ">" | ">>" | ">=" | "=" | "==" | ":="
             | "[" | "]" | "{" | "}" | "(" | ")" | ":" | ";"
             | string | integer | float | NEWLINE

    identifier ::=
            (letter | "_") (letter | digit | "_")*

    letter ::=
            lowercase | uppercase

    lowercase ::=
            "a"..."z"

    uppercase ::=
            "A"..."Z"

    digit ::=
            "0"..."9"

    string ::=
            '"' stringitem* '"'

    stringitem ::=
            <ascii character except for \> | escapeseq

    escapeseq ::=
            "\x" [0-9a-fA-F]{2} | "\" [0-9]{1,3} | "\" <any ascii char>

    integer ::=
            0 ("0"..."7")* | 0x (digit | "a"..."f" | "A"..."F")* | digit*

    float ::=
            digit+ "." digit* | "." digit+


## Priority of operators

    "*" | "/" | "%"
    "+" | "-"
    "<<" | ">>"
    "&"
    "^"
    "|"
    "<" | ">" | "==" | ">=" | "<=" | "!="
    not
    and
    or

## Grammar

    EOS ::=
            NEWLINE | ";"

### Datq types

    integer_type ::=
            "i8" | "u8" | "i16" | "u16" | "i32" | "u32" | "i64" | "u64"

    fp_type ::=
            "fp32" | "fp64"

    struct_name ::=
            identifier

    struct_type ::=
            "struct" struct_name

    union_name ::=
            identifier

    union_type ::=
            "union" union_name

    type ::=
            integer_type | fp_type | "string" | struct_type | union_type

    member ::=
            declaration [ ";" ]

    member_list ::=
            member ( "," member )*

    struct_def ::=
            struct_type "{" member_list "}"

    uniond_def ::=
            union_type "{" member_list "}"

    typedef ::=
            "typedef" type identifier

### Syntax

    suite ::=
            "{" statement* "}"

    retval ::=
            [ identifier ":" ] type

    fnname ::=
            identifier

    fndef ::=
            "fn" fnname "(" parameter_list ")" [retval] suite

    atom ::=
            identifier | literal | list_display | dict_display
            | "(" expression_list_with_comma ")"

    #identifier_list ::=
    #        identifer ( "," identifer )* [ "," ]

    expression_list_with_comma ::=
            [ expression ( "," expression )* [","] ]

    literal ::=
            stringliteral | integer | floatnumber

    list_display ::=
            "[" expression_list_with_comma "]"

    dict_display ::=
            "{" key_datum_list "}"

    key_datum_list ::=
            [ key_datum ("," key_datum)* ]

    key_datum ::=
            expression ":" expression

    declaration ::=
            identifer ":" type

    primary ::=
            atom ( "." identifier
                  | "[" expression_list_with_comma "]"
                  | "(" expression_list_with_comma ")" )*
            | "(" expression ")"

    p_expr ::=
            primary | u_expr "++" | u_expr "--"
            | p_expr ( "." identifier
                      | "[" expression_list_with_comma "]"
                      | "(" expression_list_with_comma ")" )*

    u_expr ::=
            p_expr | "-" u_expr | "+" u_expr | "~" u_expr | "++" u_expr
            | "--" u_expr

    m_expr ::=
            u_expr ( ( "*" | "/" | "%" ) u_expr )*

    a_expr ::=
            m_expr ( ( "+" | "-" ) m_expr )*

    shift_expr ::=
            a_expr ( ( "<<" | ">>" ) a_expr )*

    and_expr ::=
            shift_expr ( "&" shift_expr )*

    xor_expr ::=
            and_expr ( "^" and_expr )*

    or_expr ::=
            xor_expr ( "|" xor_expr )*

    comparison ::=
            or_expr [ comp_operator or_expr ]

    comp_operator ::=
            "<" | ">" | "==" | ">=" | "<=" | "!="

    not_test ::=
            comparison | "not" not_test

    and_test ::=
            not_test ( "and" not_test )*

    or_test ::=
            and_test ( "or" and_test )*

    assign_expr ::=
            primary ":=" assign_expr
            | or_test

    else_block ::=
            "else" suite
            | "else" if_expr

    if_expr ::=
            "if" expression suite [ else_block ]

    control_expr ::=
            if_expr
            | switch_expr
            | assign_expr

    expression ::=
            control_expr

    return_stmt ::=
            "return" expression
            | "return" ";"

    fndef ::=
            "fn" identifier funcargs [ funcargs ] suite

    crdef ::=
            "coroutine" identifer funcargs [ funcargs ] suite

    while_stmt ::=
            "while" expression suite

    statement ::=
            expression
            | return_stmt
            | fndef
            | crdef
            | while_stmt

    statements ::=
            ( statement )*

    input ::=
            statements EOF

