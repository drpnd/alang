// alang self-hosting compiler: Lexer + Parser
// Reads a source file, tokenizes, parses, prints AST
extern fn fopen(path: str, mode: str) (fp: i64)
extern fn fclose(fp: i64) (r: i32)
extern fn fread(buf: i64, size: i64, count: i64, fp: i64) (r: i64)
extern fn malloc(size: i64) (ptr: i64)
extern fn fwrite(buf: i64, size: i64, count: i64, fp: i64) (r: i64)
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
let g_first_fn: i64 = 0
let g_func_list: i64 = 0

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

fn parse_primary() (r: i64)
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
fn parse_postfix() (r: i64)
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

fn parse_unary() (r: i64)
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
fn parse_mul() (r: i64)
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

fn parse_add() (r: i64)
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

fn parse_cmp() (r: i64)
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

fn parse_expr() (r: i64)
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

fn parse_type() (r: i64)
{
    let t: i64 = 0
    if cur_type() == 1 {
        mut t = cur_val()
        mut r = advance()
    }
    mut r = t
}

fn parse_params() (r: i64)
{
    let pname: i64 = 0
    let pty: i64 = 0
    let first: i64 = 0
    let last: i64 = 0
    if is_op(40) == 1 { mut r = advance() }
    while is_op(41) == 0 {
        if cur_type() == 1 {
            mut pname = cur_val()
            mut r = advance()
            if is_op(58) == 1 { mut r = advance() }
            mut pty = parse_type()
            let p: i64 = 0
            mut p = emit_param(pname, pty)
            if first == 0 {
                mut first = p
            } else {
                mut first = emit_stmtlist(p, first)
            }
        }
        if is_op(44) == 1 { mut r = advance() }
        if cur_type() == 0 { mut r = 1 }
    }
    if is_op(41) == 1 { mut r = advance() }
    mut r = first
}

fn parse_block() (r: i64)
{
    let first: i64 = 0
    let s: i64 = 0
    if is_op(123) == 1 { mut r = advance() }
    while is_op(125) == 0 {
        if cur_type() == 0 {
            mut r = 1
        } else {
            mut s = parse_stmt()
            if first == 0 {
                mut first = s
            } else {
                mut first = emit_stmtlist(s, first)
            }
        }
    }
    if is_op(125) == 1 { mut r = advance() }
    mut r = first
}

fn parse_let() (r: i64)
{
    let name: i64 = 0
    let ty: i64 = 0
    let init: i64 = 0
    mut r = advance()
    if cur_type() == 1 {
        mut name = cur_val()
        mut r = advance()
    }
    if is_op(58) == 1 { mut r = advance() }
    if cur_type() == 1 { mut ty = parse_type() }
    if is_op(61) == 1 {
        mut r = advance()
        mut init = parse_expr()
    }
    mut r = emit_let(name, ty, init)
}

fn parse_assign() (r: i64)
{
    let name: i64 = 0
    let fname: i64 = 0
    let idx: i64 = 0
    let target: i64 = 0
    let val: i64 = 0
    mut r = advance()
    if cur_type() == 1 {
        mut name = cur_val()
        mut target = emit_ident(name)
        mut r = advance()
        mut g_done = 0
        while g_done == 0 {
            if is_op(46) == 1 {
                mut r = advance()
                mut fname = 0
                if cur_type() == 1 {
                    mut fname = cur_val()
                    mut r = advance()
                }
                mut target = emit_field(fname, target)
            } else {
                if is_op(91) == 1 {
                    mut r = advance()
                    mut idx = parse_expr()
                    if is_op(93) == 1 { mut r = advance() }
                    mut target = emit_index(target, idx)
                } else {
                    mut g_done = 1
                }
            }
        }
    }
    if is_op(61) == 1 { mut r = advance() }
    mut val = parse_expr()
    mut r = emit_assign(target, val)
}

fn parse_if() (r: i64)
{
    let cond: i64 = 0
    let then_blk: i64 = 0
    let else_blk: i64 = 0
    mut r = advance()
    mut cond = parse_expr()
    mut then_blk = parse_block()
    if is_kw(5) == 1 {
        mut r = advance()
        mut else_blk = parse_block()
    }
    mut r = emit_if(cond, then_blk, else_blk)
}

fn parse_while() (r: i64)
{
    let cond: i64 = 0
    let body: i64 = 0
    mut r = advance()
    mut cond = parse_expr()
    mut body = parse_block()
    mut r = emit_while(cond, body)
}

fn parse_return() (r: i64)
{
    let val: i64 = 0
    mut r = advance()
    if is_op(125) == 0 {
        if cur_type() == 0 {
        } else {
            mut val = parse_expr()
        }
    }
    mut r = emit_return(val)
}

fn parse_for() (r: i64)
{
    let var_name: i64 = 0
    let start: i64 = 0
    let end_val: i64 = 0
    let body: i64 = 0
    mut r = advance()
    if cur_type() == 1 {
        mut var_name = cur_val()
        mut r = advance()
    }
    if cur_type() == 1 { mut r = advance() }
    mut start = parse_expr()
    if is_op(11822) == 1 {
        mut r = advance()
        mut end_val = parse_expr()
    }
    mut body = parse_block()
    mut r = emit_for(var_name, start, end_val, body)
}

fn parse_match() (r: i64)
{
    let scrutinee: i64 = 0
    let pattern: i64 = 0
    let bind_var: i64 = 0
    let body: i64 = 0
    let first_case: i64 = 0
    let c: i64 = 0
    mut r = advance()
    mut scrutinee = parse_expr()
    if is_op(123) == 1 { mut r = advance() }
    while is_op(125) == 0 {
        if cur_type() == 0 {
            mut r = 1
        } else {
            if cur_type() == 1 {
                mut pattern = cur_val()
                mut r = advance()
                mut bind_var = 0
                if is_op(40) == 1 {
                    mut r = advance()
                    if cur_type() == 1 {
                        mut bind_var = cur_val()
                        mut r = advance()
                    }
                    if is_op(41) == 1 { mut r = advance() }
                }
                if is_op(125) == 1 {
                    mut r = 1
                } else {
                    if is_op(15742) == 1 { mut r = advance() }
                    mut body = parse_stmt()
                    mut c = emit_case(pattern, bind_var, body)
                    if first_case == 0 {
                        mut first_case = c
                    } else {
                        mut first_case = emit_stmtlist(c, first_case)
                    }
                    if is_op(44) == 1 { mut r = advance() }
                }
            } else {
                mut r = advance()
            }
        }
    }
    if is_op(125) == 1 { mut r = advance() }
    mut r = emit_match(scrutinee, first_case)
}

fn parse_stmt() (r: i64)
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
                                    mut r = emit_break()
                                } else {
                                    if is_kw(11) == 1 {
                                        mut r = advance()
                                        mut r = emit_continue()
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

fn parse_fn() (r: i64)
{
    let name: i64 = 0
    let fnode: i64 = 0
    let params: i64 = 0
    let rets: i64 = 0
    let body: i64 = 0
    mut r = advance()
    if cur_type() == 1 {
        mut name = cur_val()
        mut r = advance()
    }
    mut params = parse_params()
    if is_op(40) == 1 {
        mut rets = parse_params()
    }
    mut body = parse_block()
    mut fnode = emit_func(name, params, rets, body)
    if g_first_fn == 0 { mut g_first_fn = fnode }
    mut g_func_list = emit_stmtlist(fnode, g_func_list)
    mut r = fnode
}
fn parse_program() (r: i64)
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
                    if cur_type() == 1 { mut r = advance() }
                    if is_op(123) == 1 { mut r = advance() }
                    while is_op(125) == 0 {
                        if cur_type() == 0 { mut r = 1 } else {
                            if cur_type() == 1 { mut r = advance() }
                            if is_op(58) == 1 { mut r = advance() }
                            if cur_type() == 1 { mut r = parse_type() }
                            if is_op(44) == 1 { mut r = advance() }
                        }
                    }
                    if is_op(125) == 1 { mut r = advance() }
                } else {
                    if is_kw(13) == 1 {
                        mut r = advance()
                        if cur_type() == 1 { mut r = advance() }
                        if is_op(123) == 1 { mut r = advance() }
                        while is_op(125) == 0 {
                            if cur_type() == 0 { mut r = 1 } else {
                                if cur_type() == 1 {
                                    mut r = advance()
                                    if is_op(40) == 1 {
                                        mut r = advance()
                                        while is_op(41) == 0 {
                                            if cur_type() == 0 { mut r = 1 } else { mut r = advance() }
                                        }
                                        if is_op(41) == 1 { mut r = advance() }
                                    }
                                }
                                if is_op(44) == 1 { mut r = advance() }
                            }
                        }
                        if is_op(125) == 1 { mut r = advance() }
                    } else {
                        mut r = advance()
                    }
                }
            }
        }
    }
}
// === Code Generator: AST -> aarch64 machine code -> Mach-O ===
// Stack machine approach: expressions evaluated into X0, temps on stack

// Machine code buffer
let g_code: i64 = 0
let g_code_pos: i64 = 0

// Variable table (name string offset -> stack offset from FP)
let g_var_name: i64 = 0
let g_var_off: i64 = 0
let g_var_count: i64 = 0

// Function table (name string offset -> code offset)
let g_fn_name: i64 = 0
let g_fn_off: i64 = 0
let g_fn_count: i64 = 0

// Patch table for BL instructions (position -> function name)
let g_patch_pos: i64 = 0
let g_patch_name: i64 = 0
let g_patch_count: i64 = 0

// String table for Mach-O symbols
let g_symtab: i64 = 0
let g_symtab_pos: i64 = 0

// Emit a 32-bit instruction (little-endian)
fn emit32(val: i64) (r: i64)
{
    __byte_store(g_code, g_code_pos, val & 255)
    __byte_store(g_code, g_code_pos + 1, (val >> 8) & 255)
    __byte_store(g_code, g_code_pos + 2, (val >> 16) & 255)
    __byte_store(g_code, g_code_pos + 3, (val >> 24) & 255)
    mut g_code_pos = g_code_pos + 4
    mut r = 0
}

// === aarch64 instruction encoders ===

// MOVZ Xd, #imm16 (64-bit)
fn gen_movz(rd: i64, imm16: i64) (r: i64)
{
    mut r = emit32(0xD2800000 | ((imm16 & 65535) << 5) | (rd & 31))
}

// ADD Xd, Xn, Xm (64-bit register)
fn gen_add(rd: i64, rn: i64, rm: i64) (r: i64)
{
    mut r = emit32(0x8B000000 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
}

// SUB Xd, Xn, Xm
fn gen_sub(rd: i64, rn: i64, rm: i64) (r: i64)
{
    mut r = emit32(0xCB000000 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
}

// MUL Xd, Xn, Xm
fn gen_mul(rd: i64, rn: i64, rm: i64) (r: i64)
{
    mut r = emit32(0x9B007C00 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
}

// MOV Xd, Xm (ORR Xd, XZR, Xm)
fn gen_mov(rd: i64, rm: i64) (r: i64)
{
    mut r = emit32(0xAA0003E0 | ((rm & 31) << 16) | (rd & 31))
}

// RET
fn gen_ret() (r: i64)
{
    mut r = emit32(0xD65F03C0)
}

// ADD Xd, Xn, #imm12
fn gen_add_imm(rd: i64, rn: i64, imm12: i64) (r: i64)
{
    mut r = emit32(0x91000000 | ((imm12 & 4095) << 10) | ((rn & 31) << 5) | (rd & 31))
}

// SUB Xd, Xn, #imm12
fn gen_sub_imm(rd: i64, rn: i64, imm12: i64) (r: i64)
{
    mut r = emit32(0xD1000000 | ((imm12 & 4095) << 10) | ((rn & 31) << 5) | (rd & 31))
}

// STR Xt, [Xn, #imm12*8]
fn gen_str(rt: i64, rn: i64, imm12: i64) (r: i64)
{
    mut r = emit32(0xF9000000 | ((imm12 & 4095) << 10) | ((rn & 31) << 5) | (rt & 31))
}

// LDR Xt, [Xn, #imm12*8]
fn gen_ldr(rt: i64, rn: i64, imm12: i64) (r: i64)
{
    mut r = emit32(0xF9400000 | ((imm12 & 4095) << 10) | ((rn & 31) << 5) | (rt & 31))
}

// STP Xt1, Xt2, [Xn, #imm7*8]! (pre-index)
fn gen_stp_pre(rt1: i64, rt2: i64, rn: i64, imm7: i64) (r: i64)
{
    mut r = emit32(0xA9800000 | ((imm7 & 127) << 15) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (rt1 & 31))
}

// LDP Xt1, Xt2, [Xn], #imm7*8 (post-index)
fn gen_ldp_post(rt1: i64, rt2: i64, rn: i64, imm7: i64) (r: i64)
{
    mut r = emit32(0xA8C00000 | ((imm7 & 127) << 15) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (rt1 & 31))
}

// CMP Xn, Xm (SUBS XZR, Xn, Xm)
fn gen_cmp(rn: i64, rm: i64) (r: i64)
{
    mut r = emit32(0xEB00001F | ((rm & 31) << 16) | ((rn & 31) << 5))
}

// B.cond offset (condition codes: 0=EQ, 1=NE, 10=GE, 11=LT, 12=GT, 13=LE)
fn gen_bcond(cond: i64, offset: i64) (r: i64)
{
    let off26: i64 = 0
    mut off26 = (offset >> 2) & 0x7FFFF
    mut r = emit32(0x54000000 | (off26) | ((cond & 15) << 12))
}

// B offset (unconditional)
fn gen_b(offset: i64) (r: i64)
{
    let off26: i64 = 0
    mut off26 = (offset >> 2) & 0x3FFFFFF
    mut r = emit32(0x14000000 | off26)
}

// BL offset
fn gen_bl(offset: i64) (r: i64)
{
    let off26: i64 = 0
    mut off26 = (offset >> 2) & 0x3FFFFFF
    mut r = emit32(0x94000000 | off26)
}

// SDIV Xd, Xn, Xm
fn gen_sdiv(rd: i64, rn: i64, rm: i64) (r: i64)
{
    mut r = emit32(0x9AC00C00 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
}

// MSUB Xd, Xn, Xm, X0 (for modulo: result = X0 - Xn * Xm)
fn gen_msub(rd: i64, rn: i64, rm: i64, ra: i64) (r: i64)
{
    mut r = emit32(0x9B008000 | ((rm & 31) << 16) | ((ra & 31) << 10) | ((rn & 31) << 5) | (rd & 31))
}

// Push X0 to stack (STR X0, [SP, #-16]!)
fn gen_push() (r: i64)
{
    mut r = emit32(0xF81F0FE0)  // STR X0, [SP, #-16]!
}

// Pop to X1 (LDR X1, [SP], #16)
fn gen_pop_x1() (r: i64)
{
    mut r = emit32(0xF84107E1)  // LDR X1, [SP], #16
}

// === Variable table ===
fn var_lookup(name: i64) (r: i64)
{
    let i: i64 = 0
    mut i = 0
    mut r = 0
    while i < g_var_count {
        if __mem_load(g_var_name + i * 8) == name {
            mut r = __mem_load(g_var_off + i * 8)
        }
        mut i = i + 1
    }
}

fn var_add(name: i64, offset: i64) (r: i64)
{
    __mem_store(g_var_name + g_var_count * 8, name)
    __mem_store(g_var_off + g_var_count * 8, offset)
    mut g_var_count = g_var_count + 1
    mut r = 0
}

// === Function table ===
fn my_str_eq(s1: i64, s2: i64) (r: i64)
{
    let c1: i64 = 0
    let c2: i64 = 0
    let result: i64 = 0
    mut c1 = __byte_load(s1, 0)
    mut c2 = __byte_load(s2, 0)
    mut result = 1
    while c1 != 0 {
        if c1 != c2 {
            mut result = 0
        }
        mut s1 = s1 + 1
        mut s2 = s2 + 1
        mut c1 = __byte_load(s1, 0)
        mut c2 = __byte_load(s2, 0)
    }
    if c2 != 0 {
        mut result = 0
    }
    mut r = result
}
fn str_hash(s: i64) (r: i64)
{
    let h: i64 = 0
    let c: i64 = 0
    let i: i64 = 0
    mut h = 5381
    mut i = 0
    mut c = __byte_load(s, 0)
    while c != 0 {
        mut h = h * 33 + c
        mut i = i + 1
        mut c = __byte_load(s + i, 0)
    }
    mut r = h
}

fn fn_add(name: i64, offset: i64) (r: i64)
{
    let h: i64 = 0
    mut h = str_hash(name)
    __mem_store(g_fn_name + g_fn_count * 8, h)
    __mem_store(g_fn_off + g_fn_count * 8, offset)
    mut g_fn_count = g_fn_count + 1
    mut r = 0
}

fn fn_lookup(name: i64) (r: i64)
{
    let i: i64 = 0
    let h: i64 = 0
    let stored: i64 = 0
    let result: i64 = 0
    mut h = str_hash(name)
    mut i = 0
    while i < g_fn_count {
        mut stored = __mem_load(g_fn_name + i * 8)
        if stored == h {
            mut result = __mem_load(g_fn_off + i * 8)
        }
        mut i = i + 1
    }
    mut r = result
}


fn patch_one(ppos: i64, pname: i64) (r: i64)
{
    let foff: i64 = 0
    let rel: i64 = 0
    let off26: i64 = 0
    mut foff = fn_lookup(pname)
    if foff > 0 {
        mut rel = foff - ppos
        mut off26 = (rel >> 2) & 0x3FFFFFF
        mut r = emit32_at(ppos, 0x94000000 | off26)
    }
    mut r = 0
}

fn patch_calls() (r: i64)
{
    let i: i64 = 0
    let ppos: i64 = 0
    let pname: i64 = 0
    mut i = 0
    while i < g_patch_count {
        mut ppos = __mem_load(g_patch_pos + i * 8)
        mut pname = __mem_load(g_patch_name + i * 8)
        mut r = patch_one(ppos, pname)
        mut i = i + 1
    }
    mut r = 0
}
fn gen_cmpop_eq(op: i64) (r: i64)
{
    if op == 15677 {
        mut r = gen_cset(0, 0)
    } else {
        if op == 8645 {
            mut r = gen_cset(0, 1)
        }
    }
    mut r = 0
}

fn gen_cmpop_lt(op: i64) (r: i64)
{
    if op == 60 {
        mut r = gen_cset(0, 11)
    } else {
        if op == 62 {
            mut r = gen_cset(0, 12)
        } else {
            if op == 15485 {
                mut r = gen_cset(0, 13)
            } else {
                if op == 15997 {
                    mut r = gen_cset(0, 10)
                }
            }
        }
    }
    mut r = 0
}

fn gen_cmpop(op: i64) (r: i64)
{
    mut r = gen_cmp(1, 0)
    if op == 15677 {
        mut r = gen_cset(0, 0)
    } else {
        if op == 8645 {
            mut r = gen_cset(0, 1)
        } else {
            mut r = gen_cmpop_lt(op)
        }
    }
    mut r = 0
}

fn gen_binop(op: i64) (r: i64)
{
    let done: i64 = 0
    mut done = 0
    if op == 43 {
        mut r = gen_add(0, 1, 0)
        mut done = 1
    }
    if done == 0 {
        if op == 45 {
            mut r = gen_sub(0, 1, 0)
            mut done = 1
        }
    }
    if done == 0 {
        if op == 42 {
            mut r = gen_mul(0, 1, 0)
            mut done = 1
        }
    }
    if done == 0 {
        if op == 47 {
            mut r = gen_sdiv(0, 1, 0)
            mut done = 1
        }
    }
    if done == 0 {
        if op == 37 {
            mut r = gen_sdiv(2, 1, 0)
            mut r = gen_msub(0, 2, 0, 1)
            mut done = 1
        }
    }
    if done == 0 {
        mut r = gen_cmpop(op)
    }
    mut r = 0
}
fn gen_expr_ident(v: i64) (r: i64)
{
    let off: i64 = 0
    if off > 0 {
        mut r = gen_ldr(0, 31, off)
    } else {
        mut r = gen_movz(0, 0)
    }
    mut r = 0
}

fn gen_expr_assign(a: i64, b: i64) (r: i64)
{
    let off: i64 = 0
    mut r = gen_expr(b)
    mut off = var_lookup(__mem_load(g_ast_val + a * 8))
    if off > 0 {
        mut r = gen_str(0, 31, off)
    }
    mut r = 0
}

fn gen_expr(nd: i64) (r: i64)
{
    let k: i64 = 0
    let v: i64 = 0
    let a: i64 = 0
    let b: i64 = 0
    mut k = __mem_load(g_ast_kind + nd * 8)
    mut v = __mem_load(g_ast_val + nd * 8)
    mut a = __mem_load(g_ast_a + nd * 8)
    mut b = __mem_load(g_ast_b + nd * 8)
    if k == 1 {
        mut r = gen_movz(0, v)
    } else {
        if k == 3 {
            mut r = gen_expr_ident(v)
        } else {
            if k == 5 {
                mut r = gen_expr(a)
                mut r = gen_push()
                mut r = gen_expr(b)
                mut r = gen_pop_x1()
                mut r = gen_binop(v)
            } else {
                if k == 4 {
                    mut r = gen_call(v, a)
                } else {
                    if k == 9 {
                        mut r = gen_expr_assign(a, b)
                    } else {
                        mut r = gen_movz(0, 0)
                    }
                }
            }
        }
    }
    mut r = 0
}
// CSET Xd, cond (set Xd to 1 if condition, 0 otherwise)
fn gen_cset(rd: i64, cond: i64) (r: i64)
{
    // CSINC Xd, XZR, XZR, invert(cond)
    let inv: i64 = 0
    mut inv = cond ^ 1
    mut r = emit32(0x9A800400 | ((inv & 15) << 12) | (rd & 31))
}

// Generate function call
fn gen_pop_args(arg_count: i64) (r: i64)
{
    let i: i64 = 0
    mut i = arg_count
    while i > 0 {
        mut i = i - 1
        if i == 0 {
            mut r = gen_pop_x0()
        } else {
            if i == 1 {
                mut r = gen_pop_x1()
            } else {
                if i == 2 {
                    mut r = gen_pop_x2()
                } else {
                    if i == 3 {
                        mut r = gen_pop_x3()
                    }
                }
            }
        }
    }
    mut r = 0
}

fn gen_call(name: i64, first_arg: i64) (r: i64)
{
    let arg_count: i64 = 0
    let arg_nd: i64 = 0
    let fn_off: i64 = 0
    let k: i64 = 0
    let actual: i64 = 0
    let next_arg: i64 = 0
    mut arg_count = 0
    mut arg_nd = first_arg
    while arg_nd > 0 {
        mut k = __mem_load(g_ast_kind + arg_nd * 8)
        mut actual = arg_nd
        mut next_arg = 0
        if k == 20 {
            mut actual = __mem_load(g_ast_a + arg_nd * 8)
            mut next_arg = __mem_load(g_ast_b + arg_nd * 8)
        }
        mut r = gen_expr(actual)
        mut r = gen_push()
        mut arg_count = arg_count + 1
        mut arg_nd = next_arg
    }
    mut r = gen_pop_args(arg_count)
    mut fn_off = fn_lookup(name)
    if fn_off > 0 {
        let rel: i64 = 0
        mut rel = fn_off - g_code_pos
        mut r = gen_bl(rel)
    } else {
        __mem_store(g_patch_pos + g_patch_count * 8, g_code_pos)
        __mem_store(g_patch_name + g_patch_count * 8, name)
        mut g_patch_count = g_patch_count + 1
        mut r = gen_bl(0)
    }
    mut r = 0
}
fn gen_pop_x0() (r: i64)
{
    mut r = emit32(0xF84107E0)
}

// Pop to X2
fn gen_pop_x2() (r: i64)
{
    mut r = emit32(0xF84107E2)
}

// Pop to X3
fn gen_pop_x3() (r: i64)
{
    mut r = emit32(0xF84107E3)
}

// === Statement code generator ===

fn gen_let_stmt(v: i64, b: i64, c: i64) (r: i64)
{
    if c > 0 {
        mut r = gen_expr(b)
    } else {
        mut r = gen_movz(0, 0)
    }
    mut r = gen_str(0, 31, v)
    mut r = 0
}

fn gen_assign_stmt(a: i64, b: i64) (r: i64)
{
    mut r = gen_expr(b)
    mut r = gen_str(0, 31, var_lookup(__mem_load(g_ast_val + a * 8)))
    mut r = 0
}

fn gen_return_stmt(a: i64) (r: i64)
{
    if a > 0 {
        mut r = gen_expr(a)
    } else {
        mut r = gen_movz(0, 0)
    }
    mut r = gen_ret()
    mut r = 0
}

fn gen_stmt(nd: i64) (r: i64)
{
    let k: i64 = 0
    let v: i64 = 0
    let a: i64 = 0
    let b: i64 = 0
    let c: i64 = 0
    mut k = __mem_load(g_ast_kind + nd * 8)
    mut v = __mem_load(g_ast_val + nd * 8)
    mut a = __mem_load(g_ast_a + nd * 8)
    mut b = __mem_load(g_ast_b + nd * 8)
    mut c = __mem_load(g_ast_c + nd * 8)
    if k == 10 {
        mut r = gen_let_stmt(v, b, c)
    } else {
        if k == 9 {
            mut r = gen_assign_stmt(a, b)
        } else {
            if k == 14 {
                mut r = gen_return_stmt(a)
            } else {
                if k == 11 {
                    mut r = gen_if(nd)
                } else {
                    if k == 12 {
                        mut r = gen_while(nd)
                    } else {
                        if k == 20 {
                            mut r = gen_block(nd)
                        } else {
                            if k == 4 {
                                mut r = gen_call(v, a)
                            } else {
                                if k > 0 {
                                    mut r = gen_expr(nd)
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
fn gen_block(nd: i64) (r: i64)
{
    let s: i64 = 0
    mut s = nd
    while s > 0 {
        let k: i64 = 0
        mut k = __mem_load(g_ast_kind + s * 8)
        if k == 20 {
            // STMTLIST: a = stmt, b = next
            let stmt: i64 = 0
            mut stmt = __mem_load(g_ast_a + s * 8)
            if stmt > 0 {
                mut r = gen_stmt(stmt)
            }
            mut s = __mem_load(g_ast_b + s * 8)
        } else {
            mut r = gen_stmt(s)
            mut s = 0
        }
    }
    mut r = 0
}

fn gen_if(nd: i64) (r: i64)
{
    let cond: i64 = 0
    let then_blk: i64 = 0
    let else_blk: i64 = 0
    let cmp_pos: i64 = 0
    let beq_pos: i64 = 0
    let else_pos: i64 = 0
    let bend_pos: i64 = 0
    mut cond = __mem_load(g_ast_a + nd * 8)
    mut then_blk = __mem_load(g_ast_b + nd * 8)
    mut else_blk = __mem_load(g_ast_c + nd * 8)
    // Evaluate condition
    mut r = gen_expr(cond)
    mut r = gen_cmp(0, 31)  // CMP X0, XZR (compare with zero)
    // BEQ to else (if condition is false/zero)
    mut beq_pos = g_code_pos
    mut r = gen_bcond(0, 8)  // placeholder: skip to else (will patch)
    // Then block
    mut r = gen_block(then_blk)
    if else_blk > 0 {
        // B to end
        mut bend_pos = g_code_pos
        mut r = gen_b(0)  // placeholder
        // Patch BEQ to jump here (else block)
        let else_off: i64 = 0
        mut else_off = g_code_pos - beq_pos
        mut r = patch_bcond(beq_pos, else_off)
        // Else block
        mut r = gen_block(else_blk)
        // Patch B to jump here (end)
        let end_off: i64 = 0
        mut end_off = g_code_pos - bend_pos
        mut r = patch_b(bend_pos, end_off)
    } else {
        // Patch BEQ to jump here (end)
        let end_off: i64 = 0
        mut end_off = g_code_pos - beq_pos
        mut r = patch_bcond(beq_pos, end_off)
    }
    mut r = 0
}

fn gen_while(nd: i64) (r: i64)
{
    let cond: i64 = 0
    let body: i64 = 0
    let loop_start: i64 = 0
    let beq_pos: i64 = 0
    mut cond = __mem_load(g_ast_a + nd * 8)
    mut body = __mem_load(g_ast_b + nd * 8)
    mut loop_start = g_code_pos
    mut r = gen_expr(cond)
    mut r = gen_cmp(0, 31)
    mut beq_pos = g_code_pos
    mut r = gen_bcond(0, 0)  // placeholder: BEQ to end
    mut r = gen_block(body)
    // B back to loop_start
    let back_off: i64 = 0
    mut back_off = loop_start - g_code_pos
    mut r = gen_b(back_off)
    // Patch BEQ to jump here (end)
    let end_off: i64 = 0
    mut end_off = g_code_pos - beq_pos
    mut r = patch_bcond(beq_pos, end_off)
    mut r = 0
}

// Patch a B.cond instruction at pos with new offset
fn patch_bcond(pos: i64, offset: i64) (r: i64)
{
    let off19: i64 = 0
    mut off19 = (offset >> 2) & 0x7FFFF
    mut r = emit32_at(pos, 0x54000000 | off19 | (__byte_load(g_code, pos + 3) & 0xF0) << 4)
    mut r = 0
}

// Patch a B instruction at pos with new offset
fn patch_b(pos: i64, offset: i64) (r: i64)
{
    let off26: i64 = 0
    mut off26 = (offset >> 2) & 0x3FFFFFF
    mut r = emit32_at(pos, 0x14000000 | off26)
    mut r = 0
}

// Emit a 32-bit value at a specific position (patching)
fn emit32_at(pos: i64, val: i64) (r: i64)
{
    __byte_store(g_code, pos, val & 255)
    __byte_store(g_code, pos + 1, (val >> 8) & 255)
    __byte_store(g_code, pos + 2, (val >> 16) & 255)
    __byte_store(g_code, pos + 3, (val >> 24) & 255)
    mut r = 0
}

// === Function code generator ===
fn extract_name(p: i64) (r: i64)
{
    let pk: i64 = 0
    let inner: i64 = 0
    let result: i64 = 0
    mut pk = __mem_load(g_ast_kind + p * 8)
    if pk == 20 {
        mut inner = __mem_load(g_ast_a + p * 8)
        mut result = __mem_load(g_ast_val + inner * 8)
    } else {
        if pk == 21 {
            mut result = __mem_load(g_ast_val + p * 8)
        }
    }
    mut r = result
}

fn extract_next(p: i64) (r: i64)
{
    let pk: i64 = 0
    let result: i64 = 0
    mut pk = __mem_load(g_ast_kind + p * 8)
    if pk == 20 {
        mut result = __mem_load(g_ast_b + p * 8)
    }
    mut r = result
}

fn gen_params(params: i64) (r: i64)
{
    let p: i64 = 0
    let reg: i64 = 0
    let pname: i64 = 0
    mut reg = 0
    mut p = params
    while p > 0 {
        mut pname = extract_name(p)
        if pname > 0 {
            mut r = var_add(pname, reg)
            mut r = gen_str(reg, 31, reg)
            mut reg = reg + 1
        }
        mut p = extract_next(p)
    }
    mut r = 0
}

fn gen_retval(rets: i64) (r: i64)
{
    let p: i64 = 0
    let pname: i64 = 0
    let off: i64 = 0
    mut p = rets
    if p > 0 {
        mut pname = extract_name(p)
        if pname > 0 {
            mut off = var_lookup(pname)
            if off > 0 {
                mut r = gen_ldr(0, 31, off)
            }
        }
    }
    mut r = 0
}
fn gen_func(nd: i64) (r: i64)
{
    let name: i64 = 0
    let params: i64 = 0
    let rets: i64 = 0
    let body: i64 = 0
    let func_off: i64 = 0
    mut name = __mem_load(g_ast_val + nd * 8)
    mut params = __mem_load(g_ast_a + nd * 8)
    mut rets = __mem_load(g_ast_b + nd * 8)
    mut body = __mem_load(g_ast_c + nd * 8)
    mut func_off = g_code_pos
    mut r = fn_add(name, func_off)
    mut g_var_count = 0
    mut r = gen_stp_pre(29, 30, 31, 65534)
    mut r = gen_add_imm(29, 31, 0)
    mut r = gen_params(params)
    mut r = gen_params(rets)
    mut r = gen_sub_imm(31, 31, 32)
    mut r = gen_block(body)
    mut r = gen_retval(rets)
    mut r = gen_add_imm(31, 31, 32)
    mut r = gen_ldp_post(29, 30, 31, 2)
    mut r = gen_ret()
    mut r = 0
}

fn register_funcs() (r: i64)
{
    let list: i64 = 0
    let nd: i64 = 0
    let name: i64 = 0
    mut list = g_func_list
    while list > 0 {
        mut nd = __mem_load(g_ast_a + list * 8)
        if nd > 0 {
            mut name = __mem_load(g_ast_val + nd * 8)
            mut r = fn_add(name, 0)
        }
        mut list = __mem_load(g_ast_b + list * 8)
    }
    mut r = 0
}

fn gen_all_funcs() (r: i64)
{
    let list: i64 = 0
    let nd: i64 = 0
    mut list = g_func_list
    while list > 0 {
        mut nd = __mem_load(g_ast_a + list * 8)
        if nd > 0 {
            mut r = gen_func(nd)
        }
        mut list = __mem_load(g_ast_b + list * 8)
    }
    mut r = 0
}
fn write_header(fp: i64, ncmds: i64, sizeofcmds: i64) (r: i64)
{
    mut r = write32(fp, 0xFEEDFACF)
    mut r = write32(fp, 0x0100000C)
    mut r = write32(fp, 0)
    mut r = write32(fp, 1)
    mut r = write32(fp, ncmds)
    mut r = write32(fp, sizeofcmds)
    mut r = write32(fp, 0)
    mut r = write32(fp, 0)
    mut r = 0
}

fn write_segment(fp: i64, text_off: i64, code_size: i64) (r: i64)
{
    let total: i64 = 0
    mut total = code_size + 32
    mut r = write32(fp, 0x19)
    mut r = write32(fp, 152)
    mut r = write_str(fp, "__TEXT")
    mut r = write64(fp, 0)
    mut r = write64(fp, total)
    mut r = write64(fp, text_off)
    mut r = write64(fp, total)
    mut r = write32(fp, 7)
    mut r = write32(fp, 7)
    mut r = write32(fp, 1)
    mut r = write32(fp, 0)
    mut r = 0
}

fn write_section(fp: i64, text_off: i64, code_size: i64) (r: i64)
{
    mut r = write_str(fp, "__text")
    mut r = write_str(fp, "__TEXT")
    mut r = write64(fp, 0)
    mut r = write64(fp, code_size)
    mut r = write32(fp, text_off)
    mut r = write32(fp, 4)
    mut r = write32(fp, 0)
    mut r = write32(fp, 0)
    mut r = write32(fp, 0x80000400)
    mut r = write32(fp, 0)
    mut r = write32(fp, 0)
    mut r = write32(fp, 0)
    mut r = 0
}

fn write_version(fp: i64) (r: i64)
{
    mut r = write32(fp, 0x24)
    mut r = write32(fp, 16)
    mut r = write32(fp, 0xA0C00)
    mut r = write32(fp, 0)
    mut r = 0
}
fn write_macho(path: i64, code_size: i64) (r: i64)
{
    let arg2_ptr: i64 = 0
    let fp: i64 = 0
    let text_off: i64 = 0
    let sym_off: i64 = 0
    let str_off: i64 = 0
    mut fp = fopen(path, "w")
    if fp == 0 {
        puts("Cannot open output file")
        mut r = 1
    } else {
        mut text_off = 32 + 152 + 16 + 24
        mut sym_off = text_off + code_size
        mut str_off = sym_off + 16
        mut r = write_header(fp, 3, 152 + 16 + 24)
        mut r = write_segment(fp, text_off, code_size)
        mut r = write_section(fp, text_off, code_size)
        mut r = write_version(fp)
        mut r = write_symtab_header(fp, sym_off, str_off)
        mut r = write_code_bytes(fp, code_size)
        mut r = write_nlist(fp, 0)
        mut r = write_main_sym(fp)
        mut r = fclose(fp)
        puts("Mach-O written")
        mut r = 0
    }
}
fn find_main_name() (r: i64)
{
    let i: i64 = 0
    let n: i64 = 0
    let found: i64 = 0
    mut i = 0
    while i < g_fn_count {
        mut n = __mem_load(g_fn_name + i * 8)
        if __str_eq(n, "main") == 1 {
            mut found = n
        }
        mut i = i + 1
    }
    mut r = found
}
fn find_main() (r: i64)
{
    let i: i64 = 0
    let n: i64 = 0
    let found: i64 = 0
    mut i = 0
    mut r = 0
    while i < g_fn_count {
        mut n = __mem_load(g_fn_name + i * 8)
        if __str_eq(n, "main") == 1 {
            mut found = __mem_load(g_fn_off + i * 8)
        }
        mut i = i + 1
    }
    mut r = found
}
fn write_symtab_header(fp: i64, sym_off: i64, str_off: i64) (r: i64)
{
    mut r = write32(fp, 2)
    mut r = write32(fp, 24)
    mut r = write32(fp, sym_off)
    mut r = write32(fp, 1)
    mut r = write32(fp, str_off)
    mut r = write32(fp, 16)
    mut r = 0
}

fn write_code_bytes(fp: i64, code_size: i64) (r: i64)
{
    let i: i64 = 0
    mut i = 0
    while i < code_size {
        mut r = write_byte(fp, __byte_load(g_code, i))
        mut i = i + 1
    }
    mut r = 0
}

fn write_nlist(fp: i64, code_off: i64) (r: i64)
{
    mut r = write32(fp, 0)
    mut r = write_byte(fp, 0x0F)
    mut r = write_byte(fp, 1)
    mut r = write16(fp, 0)
    mut r = write64(fp, code_off + find_main())
    mut r = 0
}

fn write_main_sym(fp: i64) (r: i64)
{
    mut r = write_str(fp, "_main")
    mut r = 0
}
fn write32(fp: i64, val: i64) (r: i64)
{
    let buf: i64 = 0
    mut buf = malloc(4)
    __byte_store(buf, 0, val & 255)
    __byte_store(buf, 1, (val >> 8) & 255)
    __byte_store(buf, 2, (val >> 16) & 255)
    __byte_store(buf, 3, (val >> 24) & 255)
    mut r = fwrite(buf, 1, 4, fp)
    free(buf)
}

fn write16(fp: i64, val: i64) (r: i64)
{
    let buf: i64 = 0
    mut buf = malloc(2)
    __byte_store(buf, 0, val & 255)
    __byte_store(buf, 1, (val >> 8) & 255)
    mut r = fwrite(buf, 1, 2, fp)
    free(buf)
}

fn write64(fp: i64, val: i64) (r: i64)
{
    let buf: i64 = 0
    mut buf = malloc(8)
    let i: i64 = 0
    mut i = 0
    while i < 8 {
        __byte_store(buf, i, (val >> (i * 8)) & 255)
        mut i = i + 1
    }
    mut r = fwrite(buf, 1, 8, fp)
    free(buf)
}

fn write_byte(fp: i64, val: i64) (r: i64)
{
    let buf: i64 = 0
    mut buf = malloc(1)
    __byte_store(buf, 0, val & 255)
    mut r = fwrite(buf, 1, 1, fp)
    free(buf)
}

fn write_str(fp: i64, s: i64) (r: i64)
{
    let len: i64 = 0
    mut len = 0
    let c: i32 = 0
    mut c = __byte_load(s, 0)
    while c != 0 {
        mut len = len + 1
        mut c = __byte_load(s + len, 0)
    }
    let buf: i64 = 0
    mut buf = malloc(16)
    let i: i64 = 0
    mut i = 0
    while i < len {
        __byte_store(buf, i, __byte_load(s, i))
        mut i = i + 1
    }
    mut i = len
    while i < 16 {
        __byte_store(buf, i, 0)
        mut i = i + 1
    }
    mut r = fwrite(buf, 1, 16, fp)
    free(buf)
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

fn init_parser() (r: i64)
{
    mut g_tok_type = malloc(16384)
    mut g_tok_val = malloc(16384)
    mut g_ast_kind = malloc(65536)
    mut g_ast_val = malloc(65536)
    mut g_ast_a = malloc(65536)
    mut g_ast_b = malloc(65536)
    mut g_ast_c = malloc(65536)
    mut g_ast_count = 0
    mut g_str_pool = malloc(65536)
    mut g_str_pos = 0
    mut g_tok_count = 0
    mut g_tok_idx = 0
    mut g_indent = 0
    mut g_first_fn = 0
    mut r = 0
}

fn init_codegen() (r: i64)
{
    mut g_code = malloc(65536)
    mut g_code_pos = 0
    mut g_var_name = malloc(4096)
    mut g_var_off = malloc(4096)
    mut g_var_count = 0
    mut g_fn_name = malloc(4096)
    mut g_fn_off = malloc(4096)
    mut g_fn_count = 0
    mut g_patch_pos = malloc(4096)
    mut g_patch_name = malloc(4096)
    mut g_patch_count = 0
    mut r = 0
}
fn do_parse(arg1_ptr: i64) (r: i64)
{
    let fp: i64 = 0
    mut fp = fopen(arg1_ptr, "r")
    if fp == 0 {
        puts("fopen failed")
        mut r = 1
    } else {
        mut g_src = malloc(65536)
        mut g_size = fread(g_src, 1, 65535, fp)
        fclose(fp)
        mut r = init_parser()
        mut r = lex()
        mut g_tok_idx = 0
        mut r = parse_program()
        puts("PARSE DONE")
    }
    mut r = 0
}

fn do_codegen(argv_ptr: i64) (r: i64)
{
    let arg2_ptr: i64 = 0
    mut r = init_codegen()
    mut r = gen_all_funcs()
    puts("GEN DONE")
    mut r = patch_calls()
    mut arg2_ptr = __mem_load(argv_ptr + 16)
    mut r = write_macho(arg2_ptr, g_code_pos)
    mut r = 0
}

fn run_compiler(argv_ptr: i64) (r: i64)
{
    let arg1_ptr: i64 = 0
    mut arg1_ptr = __mem_load(argv_ptr + 8)
    mut r = do_parse(arg1_ptr)
    mut r = do_codegen(argv_ptr)
    mut r = 0
}

fn main(argc: i32, argv: i64) (r: i32)
{
    mut r = run_compiler(argv)
}
