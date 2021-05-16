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


## BNF

    token ::=
            "nil" | "true" | "false" | "or" | "and" | "not"
             | "fn" | "import" | "return" | "continue" | "break"
             | "if" | "else" | "while" | "for"
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

    integer_type ::=
            "i8" | "u8" | "i16" | "u16" | "i32" | "u32" | "i64" | "u64"

    fp_type ::=
            "fp64" | "fp80"

    struct_name ::=
            identifier

    struct_type ::=
            "struct" struct_name

    type ::=
            integer_type | fp_type | "string"

    suite ::=
            "{" statement* "}"

    member ::=
            type identifier

    member_list ::=
            member ( "," member )*

    structdef ::=
            "struct" struct_name "{" member_list "}"

    retval ::=
            [ variable ":" ] type

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

    variable ::=
            identifier ":" [ type ]

    primary ::=
            atom ( "." identifier
                  | "[" expression_list_with_comma "]"
                  | "(" expression_list_with_comma ")" )*
    u_expr ::=
            primary | "-" u_expr | "+" u_expr | "~" u_expr

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

    expression ::=
            or_test

    statement ::=
            expression_stmt EOS
             | assignment_stmt EOS
             | fndef

    input ::= 
            (statement)* EOF

