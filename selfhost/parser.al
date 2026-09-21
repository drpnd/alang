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
let g_done: i64 = 0

// AST nd storage (5 parallel arrays, indexed by nd id)
// Node kinds: 1=INT 2=STR 3=IDENT 4=CALL 5=BINOP 6=UNOP 7=FIELD
//   8=INDEX 9=ASSIGN 10=LET 11=IF 12=WHILE 13=FOR 14=RETURN
//   15=BREAK 16=CONTINUE 17=MATCH 18=CASE 19=FUNC 20=STMTLIST
//   21=PARAM 22=ELSEIF
let g_ast_kind: i64 = 0
let g_ast_val: i64 = 0
let g_ast_a: i64 = 0
let g_ast_b: i64 = 0
let g_ast_c: i64 = 0
let g_ast_count: i64 = 0

// Token types: 0=EOF 1=IDENT 2=INT 3=STR 4=OP 5=KW
// Keyword IDs: 1=fn 2=let 3=mut 4=if 5=else 6=while 7=for
//   8=match 9=return 10=break 11=continue 12=struct 13=enum 14=extern

fn is_alpha(c: i32) (r: i32)
{
    mut r = 0
    if c >= 65 {
        if c <= 90 { mut r = 1 }
    }
    if c >= 97 {
        if c <= 122 { mut r = 1 }
    }
    if c == 95 { mut r = 1 }
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
    if __str_eq(s, "match") == 1 { mut r = 8 }
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
        if c == 46 { if n == 46 { mut code = 11822 } }
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

fn emit_node(kind: i64, val: i64, a: i64, b: i64, c: i64) (r: i64)
{
    let base: i64 = 0
    mut base = g_ast_count * 8
    __mem_store(g_ast_kind + base, kind)
    __mem_store(g_ast_val + base, val)
    __mem_store(g_ast_a + base, a)
    __mem_store(g_ast_b + base, b)
    __mem_store(g_ast_c + base, c)
    mut g_ast_count = g_ast_count + 1
    mut r = g_ast_count - 1
}

fn emit_int(val: i64) (r: i64)
{
    mut r = emit_node(1, val, 0, 0, 0)
}

fn emit_str(val: i64) (r: i64)
{
    mut r = emit_node(2, val, 0, 0, 0)
}

fn emit_ident(val: i64) (r: i64)
{
    mut r = emit_node(3, val, 0, 0, 0)
}

fn emit_binop(op: i64, left: i64, right: i64) (r: i64)
{
    mut r = emit_node(5, op, left, right, 0)
}

fn emit_unop(op: i64, operand: i64) (r: i64)
{
    mut r = emit_node(6, op, operand, 0, 0)
}

fn emit_call(name: i64, first_arg: i64) (r: i64)
{
    mut r = emit_node(4, name, first_arg, 0, 0)
}

fn emit_field(name: i64, obj: i64) (r: i64)
{
    mut r = emit_node(7, name, obj, 0, 0)
}

fn emit_index(arr: i64, idx: i64) (r: i64)
{
    mut r = emit_node(8, 0, arr, idx, 0)
}

fn emit_assign(target: i64, val: i64) (r: i64)
{
    mut r = emit_node(9, 0, target, val, 0)
}

fn emit_let(name: i64, ty: i64, init: i64) (r: i64)
{
    mut r = emit_node(10, name, ty, init, 0)
}

fn emit_if(cond: i64, then_blk: i64, else_blk: i64) (r: i64)
{
    mut r = emit_node(11, 0, cond, then_blk, else_blk)
}

fn emit_while(cond: i64, body: i64) (r: i64)
{
    mut r = emit_node(12, 0, cond, body, 0)
}

fn emit_for(var_name: i64, start: i64, end_val: i64, body: i64) (r: i64)
{
    mut r = emit_node(13, var_name, start, end_val, body)
}

fn emit_return(val: i64) (r: i64)
{
    mut r = emit_node(14, 0, val, 0, 0)
}

fn emit_break() (r: i64)
{
    mut r = emit_node(15, 0, 0, 0, 0)
}

fn emit_continue() (r: i64)
{
    mut r = emit_node(16, 0, 0, 0, 0)
}

fn emit_match(scrutinee: i64, first_case: i64) (r: i64)
{
    mut r = emit_node(17, 0, scrutinee, first_case, 0)
}

fn emit_case(pattern: i64, bind_var: i64, body: i64) (r: i64)
{
    mut r = emit_node(18, pattern, bind_var, body, 0)
}

fn emit_func(name: i64, params: i64, rets: i64, body: i64) (r: i64)
{
    mut r = emit_node(19, name, params, rets, body)
}

fn emit_stmtlist(stmt: i64, next: i64) (r: i64)
{
    mut r = emit_node(20, 0, stmt, next, 0)
}

fn emit_param(name: i64, ty: i64) (r: i64)
{
    mut r = emit_node(21, name, ty, 0, 0)
}

fn emit_elseif(cond: i64, then_blk: i64, next_else: i64) (r: i64)
{
    mut r = emit_node(22, 0, cond, then_blk, next_else)
}

fn parse_primary() (r: i32)
{
    let t: i64 = 0
    let nd: i64 = 0
    let name: i64 = 0
    let first_arg: i64 = 0
    let next_arg: i64 = 0
    mut t = cur_type()
    if t == 2 {
        mut nd = emit_int(cur_val())
        mut r = advance()
    } else {
        if t == 3 {
            mut nd = emit_str(cur_val())
            mut r = advance()
        } else {
            if t == 1 {
                mut name = cur_val()
                mut r = advance()
                if is_op(40) == 1 {
                    mut r = advance()
                    if is_op(41) == 0 {
                        mut first_arg = parse_expr()
                        while is_op(44) == 1 {
                            mut r = advance()
                            mut next_arg = parse_expr()
                            mut first_arg = emit_stmtlist(next_arg, first_arg)
                        }
                    }
                    if is_op(41) == 1 { mut r = advance() }
                    mut nd = emit_call(name, first_arg)
                } else {
                    mut nd = emit_ident(name)
                }
            } else {
                if is_op(40) == 1 {
                    mut r = advance()
                    mut nd = parse_expr()
                    if is_op(41) == 1 { mut r = advance() }
                } else {
                    mut nd = emit_node(0, 0, 0, 0, 0)
                    mut r = advance()
                }
            }
        }
    }
    mut r = nd
}
fn parse_postfix() (r: i32)
{
    let nd: i64 = 0
    let fname: i64 = 0
    let idx: i64 = 0
    mut nd = parse_primary()
    mut g_done = 0
    while g_done == 0 {
        if is_op(46) == 1 {
            mut r = advance()
            mut fname = 0
            if cur_type() == 1 {
                mut fname = cur_val()
                mut r = advance()
            }
            mut nd = emit_field(fname, nd)
        } else {
            if is_op(91) == 1 {
                mut r = advance()
                mut idx = parse_expr()
                if is_op(93) == 1 { mut r = advance() }
                mut nd = emit_index(nd, idx)
            } else {
                mut g_done = 1
            }
        }
    }
    mut r = nd
}

fn parse_unary() (r: i32)
{
    let nd: i64 = 0
    let operand: i64 = 0
    if is_op(45) == 1 {
        mut r = advance()
        mut operand = parse_unary()
        mut nd = emit_unop(45, operand)
    } else {
        if is_op(33) == 1 {
            mut r = advance()
            mut operand = parse_unary()
            mut nd = emit_unop(33, operand)
        } else {
            mut nd = parse_postfix()
        }
    }
    mut r = nd
}
fn parse_mul() (r: i32)
{
    let nd: i64 = 0
    let rhs: i64 = 0
    mut nd = parse_unary()
    while is_op(42) == 1 {
        mut r = advance()
        mut rhs = parse_unary()
        mut nd = emit_binop(42, nd, rhs)
    }
    while is_op(47) == 1 {
        mut r = advance()
        mut rhs = parse_unary()
        mut nd = emit_binop(47, nd, rhs)
    }
    while is_op(37) == 1 {
        mut r = advance()
        mut rhs = parse_unary()
        mut nd = emit_binop(37, nd, rhs)
    }
    mut r = nd
}

fn parse_add() (r: i32)
{
    let nd: i64 = 0
    let rhs: i64 = 0
    mut nd = parse_mul()
    while is_op(43) == 1 {
        mut r = advance()
        mut rhs = parse_mul()
        mut nd = emit_binop(43, nd, rhs)
    }
    while is_op(45) == 1 {
        mut r = advance()
        mut rhs = parse_mul()
        mut nd = emit_binop(45, nd, rhs)
    }
    mut r = nd
}

fn parse_cmp() (r: i32)
{
    let nd: i64 = 0
    let rhs: i64 = 0
    mut nd = parse_add()
    while is_op(60) == 1 {
        mut r = advance()
        mut rhs = parse_add()
        mut nd = emit_binop(60, nd, rhs)
    }
    while is_op(62) == 1 {
        mut r = advance()
        mut rhs = parse_add()
        mut nd = emit_binop(62, nd, rhs)
    }
    while is_op(15485) == 1 {
        mut r = advance()
        mut rhs = parse_add()
        mut nd = emit_binop(15485, nd, rhs)
    }
    while is_op(15997) == 1 {
        mut r = advance()
        mut rhs = parse_add()
        mut nd = emit_binop(15997, nd, rhs)
    }
    mut r = nd
}

fn parse_expr() (r: i32)
{
    let nd: i64 = 0
    let rhs: i64 = 0
    mut nd = parse_cmp()
    while is_op(15677) == 1 {
        mut r = advance()
        mut rhs = parse_cmp()
        mut nd = emit_binop(15677, nd, rhs)
    }
    while is_op(8645) == 1 {
        mut r = advance()
        mut rhs = parse_cmp()
        mut nd = emit_binop(8645, nd, rhs)
    }
    mut r = nd
}

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
        mut g_done = 0
        while g_done == 0 {
            if is_op(46) == 1 {
                mut r = advance()
                print_indent()
                puts("FIELD")
                mut g_indent = g_indent + 1
                if cur_type() == 1 {
                    print_indent()
                    print_str(cur_val())
                    putchar(10)
                    mut r = advance()
                }
                mut g_indent = g_indent - 1
            } else {
                if is_op(91) == 1 {
                    mut r = advance()
                    print_indent()
                    puts("INDEX")
                    mut g_indent = g_indent + 1
                    mut r = parse_expr()
                    if is_op(93) == 1 { mut r = advance() }
                    mut g_indent = g_indent - 1
                } else {
                    mut g_done = 1
                }
            }
        }
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

fn parse_for() (r: i32)
{
    mut r = advance()
    print_indent()
    puts("FOR")
    mut g_indent = g_indent + 1
    if cur_type() == 1 {
        print_indent()
        puts("VAR")
        mut g_indent = g_indent + 1
        print_indent()
        print_str(cur_val())
        putchar(10)
        mut g_indent = g_indent - 1
        mut r = advance()
    }
    if cur_type() == 1 { mut r = advance() }
    mut r = parse_expr()
    if is_op(11822) == 1 {
        mut r = advance()
        print_indent()
        puts("RANGE")
        mut g_indent = g_indent + 1
        mut r = parse_expr()
        mut g_indent = g_indent - 1
    }
    mut r = parse_block()
    mut g_indent = g_indent - 1
}

fn parse_match() (r: i32)
{
    mut r = advance()
    print_indent()
    puts("MATCH")
    mut g_indent = g_indent + 1
    mut r = parse_expr()
    if is_op(123) == 1 { mut r = advance() }
    while is_op(125) == 0 {
        if cur_type() == 0 { mut r = 1 } else {
            if cur_type() == 1 {
                print_indent()
                puts("CASE")
                mut g_indent = g_indent + 1
                print_indent()
                print_str(cur_val())
                putchar(10)
                mut r = advance()
                if is_op(40) == 1 {
                    mut r = advance()
                    print_indent()
                    puts("BIND")
                    if cur_type() == 1 {
                        print_indent()
                        print_str(cur_val())
                        putchar(10)
                        mut r = advance()
                    }
                    if is_op(41) == 1 { mut r = advance() }
                }
                if is_op(125) == 1 { mut r = 1 } else {
                    if is_op(15742) == 1 { mut r = advance() }
                    mut r = parse_stmt()
                    mut g_indent = g_indent - 1
                    if is_op(44) == 1 { mut r = advance() }
                }
            } else {
                mut r = advance()
            }
        }
    }
    if is_op(125) == 1 { mut r = advance() }
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
                    if is_kw(7) == 1 {
                        mut r = parse_for()
                    } else {
                        if is_kw(8) == 1 {
                            mut r = parse_match()
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
                                    if is_op(40) == 1 {
                                        mut r = advance()
                                        while is_op(41) == 0 {
                                            if cur_type() == 0 { mut r = 1 } else {
                                                mut r = advance()
                                            }
                                        }
                                        if is_op(41) == 1 { mut r = advance() }
                                    }
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
fn print_int(val: i64) (r: i64)
{
    let buf: i64 = 0
    mut buf = malloc(32)
    let neg: i64 = 0
    let v: i64 = 0
    mut v = val
    if v < 0 {
        mut neg = 1
        mut v = 0 - v
    }
    let pos: i64 = 30
    __byte_store(buf, pos, 0)
    mut pos = pos - 1
    if v == 0 {
        __byte_store(buf, pos, 48)
    } else {
        while v > 0 {
            __byte_store(buf, pos, 48 + v % 10)
            mut v = v / 10
            mut pos = pos - 1
        }
    }
    if neg == 1 {
        __byte_store(buf, pos, 45)
        mut pos = pos - 1
    }
    mut r = puts(buf + pos + 1)
    free(buf)
}

fn print_ast(nd: i64, depth: i64) (r: i64)
{
    let k: i64 = 0
    mut k = __mem_load(g_ast_kind + nd * 8)
    let v: i64 = 0
    mut v = __mem_load(g_ast_val + nd * 8)
    let a: i64 = 0
    mut a = __mem_load(g_ast_a + nd * 8)
    let b: i64 = 0
    mut b = __mem_load(g_ast_b + nd * 8)
    let c: i64 = 0
    mut c = __mem_load(g_ast_c + nd * 8)
    let i: i64 = 0
    mut i = 0
    while i < depth {
        putchar(32)
        putchar(32)
        mut i = i + 1
    }
    if k == 1 {
        puts("INT ")
        print_int(v)
        putchar(10)
    } else {
        if k == 2 {
            puts("STR")
            putchar(10)
        } else {
            if k == 3 {
                puts("IDENT ")
                print_str(v)
                putchar(10)
            } else {
                if k == 4 {
                    puts("CALL ")
                    print_str(v)
                    putchar(10)
                    if a > 0 { mut r = print_ast(a, depth + 1) }
                    if b > 0 { mut r = print_ast(b, depth + 1) }
                } else {
                    if k == 5 {
                        puts("BINOP")
                        if a > 0 { mut r = print_ast(a, depth + 1) }
                        if b > 0 { mut r = print_ast(b, depth + 1) }
                    } else {
                        if k == 9 {
                            puts("ASSIGN")
                            if a > 0 { mut r = print_ast(a, depth + 1) }
                            if b > 0 { mut r = print_ast(b, depth + 1) }
                        } else {
                            if k == 19 {
                                puts("FUNC ")
                                print_str(v)
                                putchar(10)
                                if c > 0 { mut r = print_ast(c, depth + 1) }
                            } else {
                                if k == 20 {
                                    if a > 0 { mut r = print_ast(a, depth) }
                                    if b > 0 { mut r = print_ast(b, depth) }
                                } else {
                                    puts("NODE ")
                                    print_int(k)
                                    putchar(10)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    mut r = 0
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
        mut g_ast_kind = malloc(65536)
        mut g_ast_val = malloc(65536)
        mut g_ast_a = malloc(65536)
        mut g_ast_b = malloc(65536)
        mut g_ast_c = malloc(65536)
        mut g_ast_count = 0
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
        print_int(g_ast_count)
        putchar(10)
        mut r = 0
    }
}
