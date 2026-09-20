// alang self-hosting compiler: Lexer + Parser
// Reads a source file, tokenizes, parses, prints AST
extern fn fopen(path: str, mode: str) (fp: i64)
extern fn fclose(fp: i64) (r: i32)
extern fn fread(buf: i64, size: i64, count: i64, fp: i64) (r: i64)
extern fn malloc(size: i64) (ptr: i64)
extern fn free(ptr: i64)
extern fn puts(s: str) (r: i32)
extern fn putchar(c: i32) (r: i32)

let g_src: i64 = 0
let g_size: i64 = 0
let g_pos: i64 = 0

// Token storage (i64 per entry, two arrays)
let g_tok_type: i64 = 0
let g_tok_val: i64 = 0
let g_tok_count: i64 = 0
let g_tok_idx: i64 = 0

// String pool for identifiers/strings
let g_str_pool: i64 = 0
let g_str_pos: i64 = 0

// Indent for printing
let g_indent: i64 = 0

// Token types: 0=EOF 1=IDENT 2=INT 3=STR 4=OP 5=KW
// Keyword IDs: 1=fn 2=let 3=mut 4=if 5=else 6=while 7=for
//   8=match 9=return 10=break 11=continue 12=struct 13=enum 14=extern

fn is_alpha(c: i32) (r: i32)
{
    if c >= 65 {
        if c <= 90 { mut r = 1 } else {
            if c >= 97 { if c <= 122 { mut r = 1 } else { mut r = 0 } }
        }
    } else {
        if c == 95 { mut r = 1 } else { mut r = 0 }
    }
}

fn is_digit(c: i32) (r: i32)
{
    if c >= 48 { if c <= 57 { mut r = 1 } else { mut r = 0 } }
    else { mut r = 0 }
}

fn is_space(c: i32) (r: i32)
{
    if c == 32 { mut r = 1 } else {
        if c == 10 { mut r = 1 } else {
            if c == 9 { mut r = 1 } else { mut r = 0 }
        }
    }
}

fn peek(off: i64) (r: i32)
{
    mut r = 0
    let p: i64 = 0
    mut p = g_pos + off
    if p < g_size { mut r = __byte_load(g_src, p) }
}

fn next_ch() (r: i32)
{
    mut r = 0
    mut g_pos = g_pos + 1
    if g_pos < g_size { mut r = __byte_load(g_src, g_pos) }
}

fn skip_ws(c: i32) (r: i32)
{
    mut r = c
    while is_space(r) == 1 { mut r = next_ch() }
}

fn copy_str(start: i64, len: i64) (r: i64)
{
    mut r = g_str_pool + g_str_pos
    let i: i64 = 0
    mut i = 0
    while i < len {
        __byte_store(g_str_pool, g_str_pos, __byte_load(g_src, start + i))
        mut g_str_pos = g_str_pos + 1
        mut i = i + 1
    }
    __byte_store(g_str_pool, g_str_pos, 0)
    mut g_str_pos = g_str_pos + 1
}

fn emit_tok(t: i64, v: i64) (r: i64)
{
    __mem_store(g_tok_type + g_tok_count * 8, t)
    __mem_store(g_tok_val + g_tok_count * 8, v)
    mut g_tok_count = g_tok_count + 1
    mut r = 0
}

fn check_kw1(s: i64) (r: i64)
{
    mut r = 0
    if __str_eq(s, "fn") == 1 { mut r = 1 }
    if __str_eq(s, "let") == 1 { mut r = 2 }
    if __str_eq(s, "mut") == 1 { mut r = 3 }
    if __str_eq(s, "if") == 1 { mut r = 4 }
    if __str_eq(s, "else") == 1 { mut r = 5 }
}

fn check_kw2(s: i64) (r: i64)
{
    mut r = 0
    if __str_eq(s, "while") == 1 { mut r = 6 }
    if __str_eq(s, "for") == 1 { mut r = 7 }
    if __str_eq(s, "return") == 1 { mut r = 9 }
    if __str_eq(s, "break") == 1 { mut r = 10 }
    if __str_eq(s, "continue") == 1 { mut r = 11 }
}

fn check_kw3(s: i64) (r: i64)
{
    mut r = 0
    if __str_eq(s, "struct") == 1 { mut r = 12 }
    if __str_eq(s, "enum") == 1 { mut r = 13 }
    if __str_eq(s, "extern") == 1 { mut r = 14 }
}

fn check_keyword(s: i64) (r: i64)
{
    mut r = check_kw1(s)
    if r == 0 { mut r = check_kw2(s) }
    if r == 0 { mut r = check_kw3(s) }
}

fn lex_ident(c: i32) (r: i64)
{
    let start: i64 = 0
    mut start = g_pos
    while is_alpha(c) == 1 { mut c = next_ch() }
    while is_digit(c) == 1 { mut c = next_ch() }
    let len: i64 = 0
    mut len = g_pos - start
    let s: i64 = 0
    mut s = copy_str(start, len)
    let kw: i32 = 0
    mut kw = check_keyword(s)
    if kw > 0 {
        mut r = emit_tok(5, kw)
    } else {
        mut r = emit_tok(1, s)
    }
    mut r = c
}

fn lex_number(c: i32) (r: i64)
{
    let val: i64 = 0
    mut val = 0
    while is_digit(c) == 1 {
        mut val = val * 10 + (c - 48)
        mut c = next_ch()
    }
    mut r = emit_tok(2, val)
    mut r = c
}

fn lex_string() (r: i64)
{
    let start: i64 = 0
    mut start = g_pos + 1
    let c: i32 = 0
    mut c = next_ch()
    while c != 34 {
        if g_pos >= g_size { mut c = 34 } else { mut c = next_ch() }
    }
    let end: i64 = 0
    mut end = g_pos
    let len: i64 = 0
    mut len = end - start
    let s: i64 = 0
    mut s = copy_str(start, len)
    mut r = emit_tok(3, s)
    mut r = next_ch()
}

fn lex_op(c: i32) (r: i64)
{
    let n: i32 = 0
    mut n = peek(1)
    let code: i64 = 0
    mut code = c
    if n > 0 {
        if c == 61 {
            if n == 61 { mut code = 15677 } 
            if n == 62 { mut code = 15742 }
        }
        if c == 33 { if n == 61 { mut code = 8645 } }
        if c == 60 { if n == 61 { mut code = 15485 } }
        if c == 62 { if n == 61 { mut code = 15997 } }
        if c == 45 {
            if n == 62 { mut code = 11582 }
        }
        if c == 124 { if n == 124 { mut code = 31870 } }
        if c == 38 { if n == 38 { mut code = 9798 } }
    }
    if code != c { mut g_pos = g_pos + 1 }
    mut r = emit_tok(4, code)
    mut r = next_ch()
}

fn lex() (r: i64)
{
    let c: i32 = 0
    mut c = __byte_load(g_src, g_pos)
    mut c = skip_ws(c)
    while g_pos < g_size {
        if is_alpha(c) == 1 {
            mut c = lex_ident(c)
        } else {
            if is_digit(c) == 1 {
                mut c = lex_number(c)
            } else {
                if c == 34 {
                    mut c = lex_string()
                } else {
                    if c == 47 {
                        let n: i32 = 0
                        mut n = peek(1)
                        if n == 47 {
                            mut g_pos = g_pos + 1
                            mut c = next_ch()
                            while c != 10 {
                                if g_pos >= g_size { mut c = 10 }
                                else { mut c = next_ch() }
                            }
                            mut c = skip_ws(c)
                        } else {
                            if n == 42 {
                                mut g_pos = g_pos + 1
                                mut c = next_ch()
                                while g_pos < g_size {
                                    if c == 42 {
                                        let n2: i32 = 0
                                        mut n2 = peek(1)
                                        if n2 == 47 {
                                            mut g_pos = g_pos + 1
                                            mut c = next_ch()
                                            break
                                        } else { mut c = next_ch() }
                                    } else { mut c = next_ch() }
                                }
                                mut c = skip_ws(c)
                            } else {
                                mut c = lex_op(c)
                            }
                        }
                    } else {
                        mut c = lex_op(c)
                    }
                }
            }
        }
        mut c = skip_ws(c)
    }
    mut r = emit_tok(0, 0)
}

// === Parser helpers ===

fn cur_type() (r: i64)
{
    mut r = __mem_load(g_tok_type + g_tok_idx * 8)
}

fn cur_val() (r: i64)
{
    mut r = __mem_load(g_tok_val + g_tok_idx * 8)
}

fn advance() (r: i64)
{
    mut g_tok_idx = g_tok_idx + 1
    mut r = 0
}

fn is_op(code: i64) (r: i64)
{
    mut r = 0
    if cur_type() == 4 {
        if cur_val() == code { mut r = 1 }
    }
}

fn is_kw(kw: i64) (r: i64)
{
    mut r = 0
    if cur_type() == 5 {
        if cur_val() == kw { mut r = 1 }
    }
}

fn print_indent() (r: i32)
{
    let i: i32 = 0
    mut i = 0
    while i < g_indent {
        putchar(32)
        putchar(32)
        mut i = i + 1
    }
    mut r = 0
}

fn print_str(s: i64) (r: i32)
{
    let c: i32 = 0
    mut c = __byte_load(s, 0)
    while c != 0 {
        putchar(c)
        mut s = s + 1
        mut c = __byte_load(s, 0)
    }
    mut r = 0
}

// === Expression parser (precedence climbing) ===
// Prints expressions as they're parsed

fn parse_primary() (r: i32)
{
    let t: i64 = 0
    mut t = cur_type()
    if t == 2 {
        print_indent()
        puts("INT")
        mut r = advance()
    } else {
        if t == 3 {
            print_indent()
        puts("STR")
            mut r = advance()
        } else {
            if t == 1 {
                print_indent()
                print_str(cur_val())
                putchar(10)
                mut r = advance()
            } else {
                if is_op(40) == 1 {
                    mut r = advance()
                    mut r = parse_expr()
                    if is_op(41) == 1 { mut r = advance() }
                } else {
                    print_indent()
                    puts("ERROR: unexpected token in expr")
                    mut r = advance()
                }
            }
        }
    }
}

fn parse_unary() (r: i32)
{
    if is_op(45) == 1 {
        mut r = advance()
        print_indent()
        puts("NEG")
        mut g_indent = g_indent + 1
        mut r = parse_unary()
        mut g_indent = g_indent - 1
    } else {
        if is_op(33) == 1 {
            mut r = advance()
            print_indent()
            puts("NOT")
            mut g_indent = g_indent + 1
            mut r = parse_unary()
            mut g_indent = g_indent - 1
        } else {
            mut r = parse_primary()
        }
    }
}

fn parse_mul() (r: i32)
{
    mut r = parse_unary()
    while is_op(42) == 1 {
        mut r = advance()
        mut r = parse_unary()
        print_indent()
        puts("MUL")
    }
    while is_op(47) == 1 {
        mut r = advance()
        mut r = parse_unary()
        print_indent()
        puts("DIV")
    }
    while is_op(37) == 1 {
        mut r = advance()
        mut r = parse_unary()
        print_indent()
        puts("MOD")
    }
}

fn parse_add() (r: i32)
{
    mut r = parse_mul()
    while is_op(43) == 1 {
        mut r = advance()
        mut r = parse_mul()
        print_indent()
        puts("ADD")
    }
    while is_op(45) == 1 {
        mut r = advance()
        mut r = parse_mul()
        print_indent()
        puts("SUB")
    }
}

fn parse_cmp() (r: i32)
{
    mut r = parse_add()
    while is_op(60) == 1 {
        mut r = advance()
        mut r = parse_add()
        print_indent()
        puts("LT")
    }
    while is_op(62) == 1 {
        mut r = advance()
        mut r = parse_add()
        print_indent()
        puts("GT")
    }
    while is_op(15485) == 1 {
        mut r = advance()
        mut r = parse_add()
        print_indent()
        puts("LE")
    }
    while is_op(15997) == 1 {
        mut r = advance()
        mut r = parse_add()
        print_indent()
        puts("GE")
    }
}

fn parse_expr() (r: i32)
{
    mut r = parse_cmp()
    while is_op(15677) == 1 {
        mut r = advance()
        mut r = parse_cmp()
        print_indent()
        puts("EQ")
    }
    while is_op(8645) == 1 {
        mut r = advance()
        mut r = parse_cmp()
        print_indent()
        puts("NE")
    }
}

// === Statement parser ===

fn parse_type() (r: i32)
{
    if cur_type() == 1 {
        print_indent()
        puts("TYPE: ")
        print_str(cur_val())
        putchar(10)
        mut r = advance()
    }
}

fn parse_params() (r: i32)
{
    if is_op(40) == 1 { mut r = advance() }
    while is_op(41) == 0 {
        if cur_type() == 1 {
            print_indent()
            puts("PARAM: ")
            print_str(cur_val())
            putchar(10)
            mut r = advance()
            if is_op(58) == 1 { mut r = advance() }
            mut r = parse_type()
        }
        if is_op(44) == 1 { mut r = advance() }
        if cur_type() == 0 { mut r = 1 }
    }
    if is_op(41) == 1 { mut r = advance() }
}

fn parse_block() (r: i32)
{
    if is_op(123) == 1 { mut r = advance() }
    mut g_indent = g_indent + 1
    while is_op(125) == 0 {
        if cur_type() == 0 { mut r = 1 } else { mut r = parse_stmt() }
    }
    mut g_indent = g_indent - 1
    if is_op(125) == 1 { mut r = advance() }
}

fn parse_let() (r: i32)
{
    mut r = advance()
    print_indent()
    puts("LET")
    mut g_indent = g_indent + 1
    if cur_type() == 1 {
        print_indent()
        print_str(cur_val())
        putchar(10)
        mut r = advance()
    }
    if is_op(58) == 1 { mut r = advance() }
    if cur_type() == 1 { mut r = parse_type() }
    if is_op(61) == 1 {
        mut r = advance()
        mut r = parse_expr()
    }
    mut g_indent = g_indent - 1
}

fn parse_assign() (r: i32)
{
    mut r = advance()
    print_indent()
    puts("ASSIGN")
    mut g_indent = g_indent + 1
    if cur_type() == 1 {
        print_indent()
        print_str(cur_val())
        putchar(10)
        mut r = advance()
    }
    if is_op(61) == 1 { mut r = advance() }
    mut r = parse_expr()
    mut g_indent = g_indent - 1
}

fn parse_if() (r: i32)
{
    mut r = advance()
    print_indent()
    puts("IF")
    mut g_indent = g_indent + 1
    mut r = parse_expr()
    mut r = parse_block()
    if is_kw(5) == 1 {
        mut r = advance()
        print_indent()
        puts("ELSE")
        mut r = parse_block()
    }
    mut g_indent = g_indent - 1
}

fn parse_while() (r: i32)
{
    mut r = advance()
    print_indent()
    puts("WHILE")
    mut g_indent = g_indent + 1
    mut r = parse_expr()
    mut r = parse_block()
    mut g_indent = g_indent - 1
}

fn parse_return() (r: i32)
{
    mut r = advance()
    print_indent()
    puts("RETURN")
    mut g_indent = g_indent + 1
    if is_op(125) == 0 {
        if cur_type() == 0 { } else { mut r = parse_expr() }
    }
    mut g_indent = g_indent - 1
}

fn parse_stmt() (r: i32)
{
    if is_kw(2) == 1 {
        mut r = parse_let()
    } else {
        if is_kw(3) == 1 {
            mut r = parse_assign()
        } else {
            if is_kw(4) == 1 {
                mut r = parse_if()
            } else {
                if is_kw(6) == 1 {
                    mut r = parse_while()
                } else {
                    if is_kw(9) == 1 {
                        mut r = parse_return()
                    } else {
                        if is_kw(10) == 1 {
                            mut r = advance()
                            print_indent()
                            puts("BREAK")
                        } else {
                            if is_kw(11) == 1 {
                                mut r = advance()
                                print_indent()
                                puts("CONTINUE")
                            } else {
                                mut r = parse_expr()
                            }
                        }
                    }
                }
            }
        }
    }
}

fn parse_fn() (r: i32)
{
    mut r = advance()
    print_indent()
    puts("FUNC")
    mut g_indent = g_indent + 1
    if cur_type() == 1 {
        print_indent()
        print_str(cur_val())
        putchar(10)
        mut r = advance()
    }
    mut r = parse_params()
    // Return types: (name: type) or nothing
    if is_op(40) == 1 {
        print_indent()
        puts("RETS")
        mut g_indent = g_indent + 1
        mut r = parse_params()
        mut g_indent = g_indent - 1
    }
    mut r = parse_block()
    mut g_indent = g_indent - 1
}

fn parse_program() (r: i32)
{
    while cur_type() != 0 {
        if is_kw(14) == 1 {
            mut r = advance()
            if is_kw(1) == 1 { mut r = parse_fn() }
        } else {
            if is_kw(1) == 1 {
                mut r = parse_fn()
            } else {
                if is_kw(12) == 1 {
                    mut r = advance()
                    print_indent()
                    puts("STRUCT")
                    mut g_indent = g_indent + 1
                    if cur_type() == 1 {
                        print_indent()
                        print_str(cur_val())
                        putchar(10)
                        mut r = advance()
                    }
                    if is_op(123) == 1 { mut r = advance() }
                    while is_op(125) == 0 {
                        if cur_type() == 0 { mut r = 1 } else {
                            if cur_type() == 1 {
                                print_indent()
                                puts("FIELD:")
                                mut r = advance()
                            }
                            if is_op(58) == 1 { mut r = advance() }
                            if cur_type() == 1 { mut r = parse_type() }
                            if is_op(44) == 1 { mut r = advance() }
                        }
                    }
                    if is_op(125) == 1 { mut r = advance() }
                    mut g_indent = g_indent - 1
                } else {
                    if is_kw(13) == 1 {
                        mut r = advance()
                        print_indent()
                        puts("ENUM")
                        mut g_indent = g_indent + 1
                        if cur_type() == 1 {
                            print_indent()
                            print_str(cur_val())
                            putchar(10)
                            mut r = advance()
                        }
                        if is_op(123) == 1 { mut r = advance() }
                        while is_op(125) == 0 {
                            if cur_type() == 0 { mut r = 1 } else {
                                if cur_type() == 1 {
                                    print_indent()
                                    puts("VARIANT:")
                                    mut r = advance()
                                }
                                if is_op(44) == 1 { mut r = advance() }
                            }
                        }
                        if is_op(125) == 1 { mut r = advance() }
                        mut g_indent = g_indent - 1
                    } else {
                        print_indent()
                        puts("ERROR: expected fn/struct/enum")
                        mut r = advance()
                    }
                }
            }
        }
    }
}

fn main(argc: i32, argv: i64) (r: i32)
{
    let argv_ptr: i64 = 0
    mut argv_ptr = argv
    let arg1_ptr: i64 = 0
    mut arg1_ptr = __mem_load(argv_ptr + 8)
    let fp: i64 = 0
    mut fp = fopen(arg1_ptr, "r")
    if fp == 0 {
        puts("fopen failed")
        mut r = 1
    } else {
        mut g_src = malloc(65536)
        mut g_size = fread(g_src, 1, 65535, fp)
        fclose(fp)
        mut g_tok_type = malloc(16384)
        mut g_tok_val = malloc(16384)
        mut g_str_pool = malloc(65536)
        mut g_str_pos = 0
        mut g_tok_count = 0
        mut g_tok_idx = 0
        mut g_indent = 0
        mut r = lex()
        mut g_tok_idx = 0
        mut r = parse_program()
        puts("PARSE DONE")
        mut r = 0
    }
}
