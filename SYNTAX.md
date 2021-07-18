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

## Grammar (BNF)

    EOS ::=
            NEWLINE | ";"

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

### Literals

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

    literal_list ::=
            literal ("," literal)*

    literal ::=
            string | integer | float

### Datq types

    integer_type ::=
            "i8" | "u8" | "i16" | "u16" | "i32" | "u32" | "i64" | "u64"

    fp_type ::=
            "fp32" | "fp64"

    string_type ::=
            "string"

    boolean_type ::=
            "bool"

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

## Primitives

    declaration ::=
            identifer ":" type

    atom ::=
            identifier | literal | declaration

    primary ::=
            atom | "(" expression ")"

### Function / Coroutine

    funcarg ::=
            identifier ":" type

    funcargs ::=
            "(" [ funcarg ( "," funcarg )* ] ")"

    retval ::=
            [ identifier ":" ] type

    retvals ::=
            "(" [ retval ( "," retval )* ] ")"

    fndef ::=
            "fn" identifier funcargs [ retvals ] suite

    crdef ::=
            "coroutine" identifer funcargs [ retvals ] suite

### Expressions

    p_expr ::=
            primary | p_expr "++" | p_expr "--"
            | p_expr ( "." identifier
                      | "[" expression_list "]"
                      | "(" expression_list ")" )*

    u_expr ::=
            p_expr | "-" u_expr | "+" u_expr  | "!" u_expr | "~" u_expr
            | "++" u_expr | "--" u_expr

    m_expr ::=
            u_expr ( ( "*" | "/" | "%" ) u_expr )*

    a_expr ::=
            m_expr ( ( "+" | "-" ) m_expr )*

    shift_expr ::=
            a_expr ( ( "<<" | ">>" ) a_expr )*

    compariosn ::=
            shift_expr ( ("<" | ">" | "<=" | ">=") shift_expr )*

    comparison_eq ::=
            comparison ( ("==" | "!=") comparison )*

    and_expr ::=
            comparison_eq ( "&" comparison_eq )*

    xor_expr ::=
            and_expr ( "^" and_expr )*

    or_expr ::=
            xor_expr ( "|" xor_expr )*

    and_test ::=
            or_expr ( "&&" or_expr )*

    or_test ::=
            and_test ( "||" and_test )*

    assign_expr ::=
            primary ":=" assign_expr
            | or_test

    else_block ::=
            "else" suite
            | "else" if_expr

    if_expr ::=
            "if" expression suite [ else_block ]

    switch_case ::=
            "case" literal_list ":" statements
            | "default" ":" statements

    switch_cases ::=
            [ switch_case* ]

    switch_expr ::=
            "switch" expression "{" switch_cases "}"

    control_expr ::=
            if_expr
            | switch_expr
            | assign_expr

    expression ::=
            control_expr

    expression_list ::=
            expression ( "," expression )*

### Statements

    return_stmt ::=
            "return" expression
            | "return" ";"

    while_stmt ::=
            "while" expression suite

    statement ::=
            expression_list
            | return_stmt
            | while_stmt
            | fndef
            | crdef

    statements ::=
            statement*

    suite ::=
            "{" statement* "}"

### File

    input ::=
            statements EOF

