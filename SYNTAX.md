# Syntax

## EBNF Grammar (ISO/IEC 14977)

    token =
            "true" | "false"
             | "fn" | "coro" | "return" | "break"
             | "if" | "else" | "while" | "for" | "loop" | "match" | "in"
             | "let" | "mut" | "yield" | "await" | "node" | "source" | "sink" | "graph"
             | "struct" | "enum" | "type" | "as"
             | "pub" | "mod" | "use" (* reserved for future module system *)
             | "-" | "+" | "*" | "/" | "%" | "&" | "|" | "~" | "^"
             | "," | "." | "!" | "!="
             | "<" | "<<" | "<=" | ">" | ">>" | ">=" | "=" | "==" 
             | "<-" (* channel receive *)
             | "->" | "=>" | "|>"
             | "[" | "]" | "{" | "}" | "(" | ")" | ":" | ";"
             | ".." | "..=" | "!.." | "!..="
             | string | integer | float | NEWLINE

    identifier =
            (letter | "_") (letter | digit | "_")*

    letter =
            lowercase | uppercase

    lowercase =
            "a"..."z"

    uppercase =
            "A"..."Z"

    digit =
            "0"..."9"

    (* LITERALS *)

    string =
            '"' stringitem* '"'

    stringitem =
            <ascii character except for \> | escapeseq

    escapeseq =
            "\x" [0-9a-fA-F]{2} | "\" [0-9]{1,3} | "\" <any ascii char>

    binint =
            0b ("0" | "1")*

    hexint =
            0x (digit | "a"..."f" | "A"..."F")*

    decint =
            digit*

    integer =
            hexint | decint

    float =
            digit+ "." digit* | "." digit+

    literal_list =
            literal ("," literal)*

    literal =
            string | integer | float

    (* DATA TYPES *)

    integer_type =
            "i8" | "u8" | "i16" | "u16" | "i32" | "u32" | "i64" | "u64"

    fp_type =
            "fp4" | "fp8" | "f16" | "f32" | "f64"

    string_type =
            "string"

    boolean_type =
            "bool"

    struct_name =
            identifier

    struct_type =
            "struct" struct_name

    enum_name =
            identifier

    enum_type =
            "enum" enum_name

    stream_type =
            "stream" "<" type ">"

    chan_type =
            "chan" "<" type ">"

    reference_type =
            "&" [ "mut" ] type

    type =
            integer_type | fp_type | string_type | boolean_type
            | struct_type | enum_type | stream_type | chan_type
            | reference_type

    field =
            [ "mut" ] identifier ":" type

    member =
            field [ ";" ]

    member_list =
            member ( "," member )*

    struct_def =
            struct_type "{" member_list "}"

    type_list =
            type ( "," type )*

    enum_variant =
            identifier
            | identifier "(" [ type_list ] ")"
            | identifier "{" member_list "}"

    enum_variant_list =
            enum_variant ( "," enum_variant )* [ "," ]

    enum_def =
            enum_type "{" [ enum_variant_list ] "}"

    type_alias =
            "type" identifier "=" type

    (* PRIMITIVES *)

    declaration =
            "let" [ "mut" ] identifier [ ":" type ] "=" expression

    reassign =
            "mut" identifier assign_op expression

    assign_op =
            "=" | "+=" | "-=" | "*=" | "/=" | "%="
            | "&=" | "|=" | "^=" | "<<=" | ">>="

    atom =
            literal | identifier

    primary =
            atom | "(" expression_list ")"

    (* EXPRESSIONS *)

    p_expr =
            primary
            | p_expr ( "." identifier
                      | "[" expression_list "]"
                      | "(" expression_list ")" )*

    u_expr =
            p_expr | "*" u_expr | "-" u_expr | "+" u_expr  | "!" u_expr | "~" u_expr

    cast_expr =
            u_expr ( "as" type )*

    m_expr =
            cast_expr ( ( "*" | "/" | "%" ) cast_expr )*

    a_expr =
            m_expr ( ( "+" | "-" ) m_expr )*

    shift_expr =
            a_expr ( ( "<<" | ">>" ) a_expr )*

    comparison =
            shift_expr ( ("<" | ">" | "<=" | ">=") shift_expr )*

    comparison_eq =
            comparison ( ("==" | "!=") comparison )*

    and_expr =
            comparison_eq ( "&" comparison_eq )*

    xor_expr =
            and_expr ( "^" and_expr )*

    or_expr =
            xor_expr ( "|" xor_expr )*

    and_test =
            or_expr ( "&&" or_expr )*

    or_test =
            and_test ( "||" and_test )*

    assign_expr =
            reassign | or_test

    else_block =
            "else" block
            | "else" if_expr

    if_expr =
            "if" expression block [ else_block ]

    match_arm =
            pattern [ "if" expression ] "=>" expression

    match_expr =
            "match" expression "{" match_arm ( "," match_arm )* [ "," ] "}"

    control_expr =
            if_expr
            | match_expr
            | assign_expr

    expression =
            control_expr

    expression_list =
            expression ( "," expression )*

    range =
            expression ( ".." | "..=" | "!.." | "!..=" ) expression
            | expression ".."
            | ".." expression
            | ".."

    (* PATTERNS *)

    pattern =
            literal | identifier | "_"
            | "(" pattern_list ")"
            | "[" pattern_list "]"

    pattern_list =
            [ pattern ( "," pattern )* ]

    (* STATEMENTS *)

    return_stmt =
            "return" expression
            | "return" ";"

    while_expr =
            "while" expression block

    for_expr =
            "for" pattern "in" ( range | expression ) block

    loop_expr =
            "loop" block

    statement =
            declaration
            | reassign
            | expression_list
            | return_stmt
            | while_expr
            | for_expr
            | loop_expr
            | fndef
            | crdef

    statements =
            statement*

    (* BLOCKS *)

    block =
            "{" statement* [ expression ] "}"

    suite =
            block

    graphsuite =
            "{" statement* "}"

    (* FUNCTION / COROUTINE *)

    funcarg =
            [ "mut" ] identifier ":" type

    funcargs =
            "(" [ funcarg ( "," funcarg )* ] ")"

    retval =
            type

    generic_params =
            "<" identifier ( "," identifier )* ">"

    (* DIRECTIVES *)
    (* Module system deferred for initial implementation *)

    (* Top-level declaration *)

    nodedef =
            "node" identifier funcargs [ "->" retval ] suite

    graphdef =
            "graph" identifier [ funcargs ] [ "->" retval ] graphsuite

    fndef =
            "fn" identifier [ generic_params ] funcargs [ "->" retval ] suite

    crdef =
            "coro" identifier [ generic_params ] funcargs [ "->" retval ] suite

    top_level_decl =
            nodedef
            | graphdef
            | fndef
            | crdef

    top_level =
            top_level_decl*

    input =
            top_level EOF


## Fundamental features

- Pre-defined types
- User-defined types
- Variables

### Pre-defined types

Unsigned and signed integers:
- i8: signed 8-bit integer
- u8: unsigned 8-bit integer
- i16: signed 16-bit integer
- u16: unsigned 16-bit integer
- i32: signed 32-bit integer
- u32: unsigned 32-bit integer
- i64: signed 64-bit integer
- u64: unsigned 64-bit integer

Floating points:
- f16: 16-bit half-precision float (IEEE 754 binary16)
- f32: 32-bit floating point (IEEE 754 binary32)
- f64: 64-bit floating point (IEEE 754 binary64)
- fp4: 4-bit low-precision float (E2M1)
- fp8: 8-bit low-precision float (E4M3 / E5M2)

String:
- string: string

Boolean
- bool: boolean


### User-defined types

In addition to the pre-defined types, custom types can be defined.


### Variables

A variable is declared with `let`, optionally with `mut` for mutability.
The type of a variable is annotated following a `:`.

    let x: i32 = 0
    let mut y: i32 = 1
    mut y = 2
    mut y += 1

Bare `=` (without `let` or `mut`) is a compile error, eliminating
the `=` vs `==` bug class.


## Advanced features


## DATA FLOW PROCESSING

All the data are carried a packet.


## References and Pointers

The language uses Rust-style references for borrowed values. Raw
pointers (C-style `*T`) are not supported (no `unsafe` mode, no FFI).

    let x: i32 = 0
    let y: &i32 = &x          // shared reference (borrow)
    let z: i32 = *y           // dereference

    let mut w: i32 = 0
    let r: &mut i32 = &mut w  // mutable reference
    mut *r = 42               // dereference and assign

Heap allocation uses `Box<T>` (owned, future implementation).

## Div/Mod operation

    let q = x / y
    let r = x % y
    let q, r = x / y
    let _, r = x / y

## List expression

    let a, b = x + y, x + z

## if-else and match expressions

    let r = if condition { true } else { false }

    let r = match x {
        0 => "zero",
        1 => "one",
        _ => "many",
    }

## Range operators

    0..10       # [0, 10)  half-open (default)
    0..=10      # [0, 10]  inclusive
    0!..10      # (0, 10)  open
    0!..=10     # (0, 10]  half-open-left
    0..         # [0, inf)
    ..10        # (-inf, 10)
    ..          # (-inf, inf)

## Precedence of operators

1. `()`, `[]`, `.`
1. `!`, `~`, unary `+` `-`
1. `as` (type cast)
1. `*`, `/`, `%`
1. `+` `-`
1. `<<` `>>`
1. `<` `<=` `>` `>=`
1. `==` `!=`
1. `&`
1. `^`
1. `|`
1. `&&`
1. `||`
1. range (`..` `..=` `!..` `!..=`; non-associative)
1. pipe `|>` (left-associative)
1. `return` / `yield` / keyword constructs
1. `let` / `mut`
1. `,`

## Grammar (EBNF)
