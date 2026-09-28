// alang self-hosting compiler: Lexer + Parser
// Reads a source file, tokenizes, parses, prints AST
// Syscall-based runtime (no libc dependency)
// Platform: 0=macOS, 1=Linux, 2=FreeBSD
// macOS aarch64: x16=syscall#, x0-x5=args, svc #0x80
// Linux aarch64: x8=syscall#, x0-x5=args, svc #0
// FreeBSD aarch64: x8=syscall#, x0-x5=args, svc #0
// __syscall(num, a0, a1, a2, a3, a4, a5) returns x0

let g_target_os: i64 = 0
let g_target_isa: i64 = 0
let g_output_elf: i64 = 0
let g_exec_elf: i64 = 0

// Syscall numbers (macOS/BSD)
let SC_READ: i64 = 3
let SC_WRITE: i64 = 4
let SC_OPEN: i64 = 5
let SC_CLOSE: i64 = 6
let SC_MMAP: i64 = 197
let SC_EXIT: i64 = 1

// Target syscall numbers (for codegen only, not compiler runtime)
let TSC_WRITE: i64 = 4
let TSC_MMAP: i64 = 197
let T_MAP_FLAGS: i64 = 4098

// O_RDONLY = 0, O_WRONLY = 1, O_RDWR = 2, O_CREAT = 0x200

// sys_write(fd, buf, len) -> bytes written
fn sys_write(fd: i64, buf: i64, len: i64) (r: i64)
{
    mut r = __syscall(SC_WRITE, fd, buf, len)
}

// sys_read(fd, buf, len) -> bytes read
fn sys_read(fd: i64, buf: i64, len: i64) (r: i64)
{
    mut r = __syscall(SC_READ, fd, buf, len)
}

// sys_open(path, flags, mode) -> fd
fn sys_open(path: i64, flags: i64, mode: i64) (r: i64)
{
    mut r = __syscall(SC_OPEN, path, flags, mode)
}

// sys_close(fd) -> 0 on success
fn sys_close(fd: i64) (r: i64)
{
    mut r = __syscall(SC_CLOSE, fd)
}

// sys_mmap(addr, size, prot, flags, fd, offset) -> ptr
fn sys_mmap(addr: i64, size: i64, prot: i64, flags: i64, fd: i64, offset: i64) (r: i64)
{
    mut r = __syscall(SC_MMAP, addr, size, prot, flags, fd, offset)
}

// sys_exit(code)
fn sys_exit(code: i64) (r: i64)
{
    mut r = __syscall(SC_EXIT, code)
}

// malloc replacement: use mmap to allocate memory
// PROT_READ|PROT_WRITE = 3, MAP_PRIVATE|MAP_ANON = 4098
fn malloc(size: i64) (ptr: i64)
{
    mut ptr = sys_mmap(0, size, 3, 4098, -1, 0)
}

// free is a no-op (short-lived process, OS reclaims on exit)
fn free(ptr: i64) (r: i64)
{
    mut r = 0
}

// fopen replacement: open(path, flags, mode) -> fd
// mode "r" = O_RDONLY (0), mode "w" = O_WRONLY|O_CREAT|O_TRUNC (0x601)
fn fopen(path: i64, mode: i64) (fp: i64)
{
    let flags: i64 = 0
    mut flags = 0
    // Check first byte of mode string: 'w' = 119
    if __byte_load(mode, 0) == 119 {
        mut flags = 1537  // O_WRONLY(1) | O_CREAT(0x200) | O_TRUNC(0x400) = 0x601
    }
    mut fp = sys_open(path, flags, 420)  // 420 = 0644 file permissions
}

// fclose replacement: close(fd)
fn fclose(fd: i64) (r: i32)
{
    let res: i64 = 0
    mut res = sys_close(fd)
    mut r = 0
}

// fread replacement: read(fd, buf, count) -> bytes read
fn fread(buf: i64, size: i64, count: i64, fp: i64) (r: i64)
{
    mut r = sys_read(fp, buf, count)
}

// fwrite replacement: write(fd, buf, count) -> bytes written
fn fwrite(buf: i64, size: i64, count: i64, fp: i64) (r: i64)
{
    mut r = sys_write(fp, buf, count)
}

// puts replacement: write(1, s, strlen(s)) to stdout
// Uses a simple strlen loop inline
fn puts(s: i64) (r: i32)
{
    let len: i64 = 0
    mut len = 0
    while __byte_load(s, len) != 0 {
        mut len = len + 1
    }
    let res: i64 = 0
    mut res = sys_write(1, s, len)
    mut r = 0
}

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
let g_glob_name: i64 = 0
let g_glob_off: i64 = 0
let g_glob_val: i64 = 0
let g_glob_count: i64 = 0

// Indent for printing
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

fn is_ident_char(c: i32) (r: i32)
{
    mut r = 0
    if is_alpha(c) == 1 { mut r = 1 }
    if is_digit(c) == 1 { mut r = 1 }
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
    while is_ident_char(c) == 1 { mut c = next_ch() }
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
    let ishex: i64 = 0
    let cont: i64 = 0
    let dv: i64 = 0
    mut val = 0
    mut ishex = 0
    if c == 48 {
        mut c = next_ch()
        if c == 120 {
            mut ishex = 1
            mut c = next_ch()
        }
    }
    if ishex == 1 {
        mut cont = 1
        while cont == 1 {
            mut dv = 0
            if c >= 48 {
                if c <= 57 {
                    mut dv = c - 48
                }
            }
            if c >= 97 {
                if c <= 102 {
                    mut dv = c - 87
                }
            }
            if c >= 65 {
                if c <= 70 {
                    mut dv = c - 55
                }
            }
            if dv > 0 || c == 48 {
                mut val = val * 16 + dv
                mut c = next_ch()
            } else {
                mut cont = 0
            }
        }
    } else {
        while is_digit(c) == 1 {
            mut val = val * 10 + (c - 48)
            mut c = next_ch()
        }
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
        if c == 62 { if n == 62 { mut code = 15854 } }
        if c == 60 { if n == 60 { mut code = 15420 } }
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

// === Expression parser (precedence climbing) ===
// Prints expressions as they're parsed

fn store_node_fields(base: i64, kind: i64, val: i64, a: i64, b: i64, c: i64) (r: i64)
{
    __mem_store(g_ast_kind + base, kind)
    __mem_store(g_ast_val + base, val)
    __mem_store(g_ast_a + base, a)
    __mem_store(g_ast_b + base, b)
    __mem_store(g_ast_c + base, c)
    mut r = 0
}

fn emit_node(kind: i64, val: i64, a: i64, b: i64, c: i64) (r: i64)
{
    let base: i64 = 0
    mut base = g_ast_count * 8
    mut r = store_node_fields(base, kind, val, a, b, c)
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
                        let arg_last: i64 = 0
                        mut arg_last = emit_stmtlist(first_arg, 0)
                        mut first_arg = arg_last
                        while is_op(44) == 1 {
                            mut r = advance()
                            mut next_arg = parse_expr()
                            let arg_wrapper: i64 = 0
                            mut arg_wrapper = emit_stmtlist(next_arg, 0)
                            __mem_store(g_ast_b + arg_last * 8, arg_wrapper)
                            mut arg_last = arg_wrapper
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

fn parse_bit() (r: i64)
{
    let nd: i64 = 0
    let rhs: i64 = 0
    mut nd = parse_add()
    while is_op(38) == 1 {
        mut r = advance()
        mut rhs = parse_add()
        mut nd = emit_binop(38, nd, rhs)
    }
    while is_op(124) == 1 {
        mut r = advance()
        mut rhs = parse_add()
        mut nd = emit_binop(124, nd, rhs)
    }
    while is_op(94) == 1 {
        mut r = advance()
        mut rhs = parse_add()
        mut nd = emit_binop(94, nd, rhs)
    }
    while is_op(15854) == 1 {
        mut r = advance()
        mut rhs = parse_add()
        mut nd = emit_binop(15854, nd, rhs)
    }
    while is_op(15420) == 1 {
        mut r = advance()
        mut rhs = parse_add()
        mut nd = emit_binop(15420, nd, rhs)
    }
    mut r = nd
}
fn parse_cmp() (r: i64)
{
    let nd: i64 = 0
    let rhs: i64 = 0
    mut nd = parse_bit()
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

fn parse_eq_ne() (r: i64)
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

fn parse_logic_and() (r: i64)
{
    let nd: i64 = 0
    let rhs: i64 = 0
    mut nd = parse_eq_ne()
    while is_op(9798) == 1 {
        mut r = advance()
        mut rhs = parse_eq_ne()
        mut nd = emit_binop(9798, nd, rhs)
    }
    mut r = nd
}

fn parse_expr() (r: i64)
{
    let nd: i64 = 0
    let rhs: i64 = 0
    mut nd = parse_logic_and()
    while is_op(31870) == 1 {
        mut r = advance()
        mut rhs = parse_logic_and()
        mut nd = emit_binop(31870, nd, rhs)
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
        if cur_type() == 0 { mut r = 1 }
        if cur_type() == 1 {
            mut pname = cur_val()
            mut r = advance()
            if is_op(58) == 1 { mut r = advance() }
            mut pty = parse_type()
            let p: i64 = 0
            mut p = emit_param(pname, pty)
            let wrapper: i64 = 0
            mut wrapper = emit_stmtlist(p, 0)
            if first == 0 {
                mut first = wrapper
            } else {
                __mem_store(g_ast_b + last * 8, wrapper)
            }
            mut last = wrapper
        } else {
            if is_op(44) == 1 {
                mut r = advance()
            } else {
                if is_op(41) == 0 {
                    mut r = advance()
                }
            }
        }
    }
    if is_op(41) == 1 { mut r = advance() }
    mut r = first
}

fn parse_block() (r: i64)
{
    let first: i64 = 0
    let last: i64 = 0
    let s: i64 = 0
    let wrapper: i64 = 0
    if is_op(123) == 1 { mut r = advance() }
    while is_op(125) == 0 {
        if cur_type() == 0 {
            mut r = 1
        } else {
            mut s = parse_stmt()
            mut wrapper = emit_stmtlist(s, 0)
            if first == 0 {
                mut first = wrapper
            } else {
                __mem_store(g_ast_b + last * 8, wrapper)
            }
            mut last = wrapper
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
            mut r = parse_let()
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
    mut g_func_list = emit_stmtlist(fnode, g_func_list)
    mut r = fnode
}
fn parse_glob_decl() (r: i64)
{
    let gidx: i64 = 0
    mut r = advance()
    if cur_type() == 1 {
        mut gidx = glob_add(cur_val())
        mut r = advance()
    }
    if is_op(58) == 1 {
        mut r = advance()
        if cur_type() == 1 { mut r = parse_type() }
    }
    if is_op(61) == 1 {
        mut r = advance()
        if cur_type() == 2 {
            __mem_store(g_glob_val + gidx * 8, cur_val())
            mut r = advance()
        } else {
            if cur_type() != 0 { mut r = parse_expr() }
        }
    }
    mut r = 0
}

fn parse_program() (r: i64)
{
    while cur_type() != 0 {
        if is_kw(14) == 1 {
            mut r = advance()
            if is_kw(1) == 1 {
                mut r = advance()
                if cur_type() == 1 { mut r = advance() }
                if is_op(40) == 1 {
                    mut r = advance()
                    while is_op(41) == 0 {
                        if cur_type() == 0 { mut r = 1 } else { mut r = advance() }
                    }
                    if is_op(41) == 1 { mut r = advance() }
                }
                if is_op(40) == 1 {
                    mut r = advance()
                    while is_op(41) == 0 {
                        if cur_type() == 0 { mut r = 1 } else { mut r = advance() }
                    }
                    if is_op(41) == 1 { mut r = advance() }
                }
            }
        } else {
            if is_kw(1) == 1 {
                mut r = parse_fn()
            } else {
                if is_kw(2) == 1 {
                    mut r = parse_glob_decl()
                } else {
                    if is_kw(3) == 1 {
                        mut r = parse_glob_decl()
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
                                let en_tag: i64 = 0
                                let en_name: i64 = 0
                                let en_has_arg: i64 = 0
                                mut en_tag = 0
                                mut r = advance()
                                if cur_type() == 1 { mut r = advance() }
                                if is_op(123) == 1 { mut r = advance() }
                                while is_op(125) == 0 {
                                    if cur_type() == 0 { mut r = 1 } else {
                                        if cur_type() == 1 {
                                            mut en_name = cur_val()
                                            mut r = advance()
                                            mut en_has_arg = 0
                                            if is_op(40) == 1 {
                                                mut en_has_arg = 1
                                                mut r = advance()
                                                while is_op(41) == 0 {
                                                    if cur_type() == 0 { mut r = 1 } else { mut r = advance() }
                                                }
                                                if is_op(41) == 1 { mut r = advance() }
                                            }
                                            mut r = enum_add(en_name, en_tag, en_has_arg)
                                            mut en_tag = en_tag + 1
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
let g_call_name: i64 = 0
let g_str_const: i64 = 0
let g_str_const_count: i64 = 0
let g_adr_patch_pos: i64 = 0
let g_adr_patch_idx: i64 = 0
let g_adr_patch_count: i64 = 0
let g_loop_start: i64 = 0
let g_loop_end: i64 = 0
let g_saved_loop_start: i64 = 0
let g_saved_loop_end: i64 = 0
let g_break_pos: i64 = 0

// Function table (name string offset -> code offset)
let g_fn_name: i64 = 0
let g_fn_off: i64 = 0
let g_fn_count: i64 = 0

// Enum variant table (name -> tag, has_arg)
let g_enum_name: i64 = 0
let g_enum_tag: i64 = 0
let g_enum_has_arg: i64 = 0
let g_enum_count: i64 = 0

// Patch table for BL instructions (position -> function name)
let g_patch_pos: i64 = 0
let g_patch_name: i64 = 0
let g_patch_count: i64 = 0
let g_ext_name: i64 = 0
let g_ext_pos: i64 = 0
let g_ext_count: i64 = 0

// String table for Mach-O symbols

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


// x86-64 registers (indices for encoding)
let X86_RAX: i64 = 0
let X86_RCX: i64 = 1
let X86_RDX: i64 = 2
let X86_RBX: i64 = 3
let X86_RSP: i64 = 4
let X86_RBP: i64 = 5
let X86_RSI: i64 = 6
let X86_RDI: i64 = 7
let X86_R8: i64 = 8
let X86_R9: i64 = 9
let X86_R10: i64 = 10
let X86_R11: i64 = 11
let X86_R12: i64 = 12
let X86_R13: i64 = 13
let X86_R14: i64 = 14
let X86_R15: i64 = 15

// === x86-64 byte emitter and helpers ===

fn emit_byte(val: i64) (r: i64)
{
    __byte_store(g_code, g_code_pos, val & 255)
    mut g_code_pos = g_code_pos + 1
    mut r = 0
}

fn emit_rex(w: i64, r: i64, x: i64, b: i64) (r: i64)
{
    let rex: i64 = 0x40
    mut rex = 0x40 | ((w & 1) << 3) | ((r & 1) << 2) | ((x & 1) << 1) | (b & 1)
    if rex != 0x40 {
        mut r = emit_byte(rex)
    }
    mut r = 0
}

fn emit_modrm(mod_val: i64, reg: i64, rm: i64) (r: i64)
{
    mut r = emit_byte(((mod_val & 3) << 6) | ((reg & 7) << 3) | (rm & 7))
}

// === x86-64 instruction encoders ===

// MOV reg64, imm64
fn x86_mov_imm(rd: i64, imm: i64) (r: i64)
{
    mut r = emit_rex(1, 0, 0, (rd >> 3) & 1)
    mut r = emit_byte(0xB8 + (rd & 7))
    mut r = emit_byte(imm & 255)
    mut r = emit_byte((imm >> 8) & 255)
    mut r = emit_byte((imm >> 16) & 255)
    mut r = emit_byte((imm >> 24) & 255)
    mut r = emit_byte((imm >> 32) & 255)
    mut r = emit_byte((imm >> 40) & 255)
    mut r = emit_byte((imm >> 48) & 255)
    mut r = emit_byte((imm >> 56) & 255)
}

// MOV reg64, reg64
fn x86_mov_reg(dst: i64, src: i64) (r: i64)
{
    mut r = emit_rex(1, (src >> 3) & 1, 0, (dst >> 3) & 1)
    mut r = emit_byte(0x89)
    mut r = emit_modrm(3, src & 7, dst & 7)
}

// ADD reg64, reg64
fn x86_add_reg(dst: i64, src: i64) (r: i64)
{
    mut r = emit_rex(1, (src >> 3) & 1, 0, (dst >> 3) & 1)
    mut r = emit_byte(0x01)
    mut r = emit_modrm(3, src & 7, dst & 7)
}

// SUB reg64, reg64
fn x86_sub_reg(dst: i64, src: i64) (r: i64)
{
    mut r = emit_rex(1, (src >> 3) & 1, 0, (dst >> 3) & 1)
    mut r = emit_byte(0x29)
    mut r = emit_modrm(3, src & 7, dst & 7)
}

// IMUL reg64, reg64
fn x86_mul_reg(dst: i64, src: i64) (r: i64)
{
    mut r = emit_rex(1, (dst >> 3) & 1, 0, (src >> 3) & 1)
    mut r = emit_byte(0x0F)
    mut r = emit_byte(0xAF)
    mut r = emit_modrm(3, dst & 7, src & 7)
}

// AND reg64, reg64
fn x86_and_reg(dst: i64, src: i64) (r: i64)
{
    mut r = emit_rex(1, (src >> 3) & 1, 0, (dst >> 3) & 1)
    mut r = emit_byte(0x21)
    mut r = emit_modrm(3, src & 7, dst & 7)
}

// OR reg64, reg64
fn x86_or_reg(dst: i64, src: i64) (r: i64)
{
    mut r = emit_rex(1, (src >> 3) & 1, 0, (dst >> 3) & 1)
    mut r = emit_byte(0x09)
    mut r = emit_modrm(3, src & 7, dst & 7)
}

// XOR reg64, reg64
fn x86_xor_reg(dst: i64, src: i64) (r: i64)
{
    mut r = emit_rex(1, (src >> 3) & 1, 0, (dst >> 3) & 1)
    mut r = emit_byte(0x31)
    mut r = emit_modrm(3, src & 7, dst & 7)
}

// SHL reg64, imm8
fn x86_shl_imm(dst: i64, count: i64) (r: i64)
{
    mut r = emit_rex(1, 0, 0, (dst >> 3) & 1)
    mut r = emit_byte(0xC1)
    mut r = emit_modrm(3, 4, dst & 7)
    mut r = emit_byte(count & 255)
}

// SHR reg64, imm8
fn x86_shr_imm(dst: i64, count: i64) (r: i64)
{
    mut r = emit_rex(1, 0, 0, (dst >> 3) & 1)
    mut r = emit_byte(0xC1)
    mut r = emit_modrm(3, 5, dst & 7)
    mut r = emit_byte(count & 255)
}

// CMP reg64, reg64
fn x86_cmp_reg(a: i64, b: i64) (r: i64)
{
    mut r = emit_rex(1, (b >> 3) & 1, 0, (a >> 3) & 1)
    mut r = emit_byte(0x39)
    mut r = emit_modrm(3, b & 7, a & 7)
}

// SETcc reg8
fn x86_setcc(cond: i64, dst: i64) (r: i64)
{
    mut r = emit_rex(0, 0, 0, (dst >> 3) & 1)
    mut r = emit_byte(0x0F)
    mut r = emit_byte(0x90 + (cond & 15))
    mut r = emit_modrm(3, 0, dst & 7)
}

// MOVZX reg64, reg8
fn x86_movzx_64(dst: i64, src: i64) (r: i64)
{
    mut r = emit_rex(1, (dst >> 3) & 1, 0, (src >> 3) & 1)
    mut r = emit_byte(0x0F)
    mut r = emit_byte(0xB6)
    mut r = emit_modrm(3, dst & 7, src & 7)
}

// Jcc rel32
fn x86_jcc(cond: i64, offset: i64) (r: i64)
{
    mut r = emit_byte(0x0F)
    mut r = emit_byte(0x80 + (cond & 15))
    mut r = emit_byte(offset & 255)
    mut r = emit_byte((offset >> 8) & 255)
    mut r = emit_byte((offset >> 16) & 255)
    mut r = emit_byte((offset >> 24) & 255)
}

// JMP rel32
fn x86_jmp(offset: i64) (r: i64)
{
    mut r = emit_byte(0xE9)
    mut r = emit_byte(offset & 255)
    mut r = emit_byte((offset >> 8) & 255)
    mut r = emit_byte((offset >> 16) & 255)
    mut r = emit_byte((offset >> 24) & 255)
}

// CALL rel32
fn x86_call(offset: i64) (r: i64)
{
    mut r = emit_byte(0xE8)
    mut r = emit_byte(offset & 255)
    mut r = emit_byte((offset >> 8) & 255)
    mut r = emit_byte((offset >> 16) & 255)
    mut r = emit_byte((offset >> 24) & 255)
}

// RET
fn x86_ret() (r: i64)
{
    mut r = emit_byte(0xC3)
}

// PUSH reg64
fn x86_push_reg(reg: i64) (r: i64)
{
    if reg >= 8 {
        mut r = emit_byte(0x41)
    }
    mut r = emit_byte(0x50 + (reg & 7))
}

// POP reg64
fn x86_pop_reg(reg: i64) (r: i64)
{
    if reg >= 8 {
        mut r = emit_byte(0x41)
    }
    mut r = emit_byte(0x58 + (reg & 7))
}

// MOV reg64, [base64 + disp32]
fn x86_load_reg(dst: i64, base: i64, disp: i64) (r: i64)
{
    mut r = emit_rex(1, (dst >> 3) & 1, 0, (base >> 3) & 1)
    mut r = emit_byte(0x8B)
    if (base & 7) == 4 {
        mut r = emit_modrm(2, dst & 7, 4)
        mut r = emit_byte(0x24)
    } else {
        mut r = emit_modrm(2, dst & 7, base & 7)
    }
    mut r = emit_byte(disp & 255)
    mut r = emit_byte((disp >> 8) & 255)
    mut r = emit_byte((disp >> 16) & 255)
    mut r = emit_byte((disp >> 24) & 255)
}

// MOV [base64 + disp32], reg64
fn x86_store_reg(src: i64, base: i64, disp: i64) (r: i64)
{
    mut r = emit_rex(1, (src >> 3) & 1, 0, (base >> 3) & 1)
    mut r = emit_byte(0x89)
    if (base & 7) == 4 {
        mut r = emit_modrm(2, src & 7, 4)
        mut r = emit_byte(0x24)
    } else {
        mut r = emit_modrm(2, src & 7, base & 7)
    }
    mut r = emit_byte(disp & 255)
    mut r = emit_byte((disp >> 8) & 255)
    mut r = emit_byte((disp >> 16) & 255)
    mut r = emit_byte((disp >> 24) & 255)
}

// MOVZX reg64, byte [base64 + disp32]
fn x86_load8_reg(dst: i64, base: i64, disp: i64) (r: i64)
{
    mut r = emit_rex(1, (dst >> 3) & 1, 0, (base >> 3) & 1)
    mut r = emit_byte(0x0F)
    mut r = emit_byte(0xB6)
    if (base & 7) == 4 {
        mut r = emit_modrm(2, dst & 7, 4)
        mut r = emit_byte(0x24)
    } else {
        mut r = emit_modrm(2, dst & 7, base & 7)
    }
    mut r = emit_byte(disp & 255)
    mut r = emit_byte((disp >> 8) & 255)
    mut r = emit_byte((disp >> 16) & 255)
    mut r = emit_byte((disp >> 24) & 255)
}

// MOV byte [base64 + disp32], reg8
fn x86_store8_reg(src: i64, base: i64, disp: i64) (r: i64)
{
    mut r = emit_rex(0, (src >> 3) & 1, 0, (base >> 3) & 1)
    mut r = emit_byte(0x88)
    if (base & 7) == 4 {
        mut r = emit_modrm(2, src & 7, 4)
        mut r = emit_byte(0x24)
    } else {
        mut r = emit_modrm(2, src & 7, base & 7)
    }
    mut r = emit_byte(disp & 255)
    mut r = emit_byte((disp >> 8) & 255)
    mut r = emit_byte((disp >> 16) & 255)
    mut r = emit_byte((disp >> 24) & 255)
}

// ADD reg64, imm32 (sign-extended)
fn x86_add_imm(dst: i64, imm: i64) (r: i64)
{
    mut r = emit_rex(1, 0, 0, (dst >> 3) & 1)
    mut r = emit_byte(0x81)
    mut r = emit_modrm(3, 0, dst & 7)
    mut r = emit_byte(imm & 255)
    mut r = emit_byte((imm >> 8) & 255)
    mut r = emit_byte((imm >> 16) & 255)
    mut r = emit_byte((imm >> 24) & 255)
}

// SUB reg64, imm32 (sign-extended)
fn x86_sub_imm(dst: i64, imm: i64) (r: i64)
{
    mut r = emit_rex(1, 0, 0, (dst >> 3) & 1)
    mut r = emit_byte(0x81)
    mut r = emit_modrm(3, 5, dst & 7)
    mut r = emit_byte(imm & 255)
    mut r = emit_byte((imm >> 8) & 255)
    mut r = emit_byte((imm >> 16) & 255)
    mut r = emit_byte((imm >> 24) & 255)
}

// NEG reg64
fn x86_neg_reg(dst: i64) (r: i64)
{
    mut r = emit_rex(1, 0, 0, (dst >> 3) & 1)
    mut r = emit_byte(0xF7)
    mut r = emit_modrm(3, 3, dst & 7)
}

// NOT reg64
fn x86_not_reg(dst: i64) (r: i64)
{
    mut r = emit_rex(1, 0, 0, (dst >> 3) & 1)
    mut r = emit_byte(0xF7)
    mut r = emit_modrm(3, 2, dst & 7)
}

// CDQ
fn x86_cdq() (r: i64)
{
    mut r = emit_byte(0x99)
}

// IDIV reg64
fn x86_idiv_reg(src: i64) (r: i64)
{
    mut r = emit_rex(1, 0, 0, (src >> 3) & 1)
    mut r = emit_byte(0xF7)
    mut r = emit_modrm(3, 7, src & 7)
}

// SYSCALL
fn x86_syscall() (r: i64)
{
    mut r = emit_byte(0x0F)
    mut r = emit_byte(0x05)
}

// NOP
fn x86_nop() (r: i64)
{
    mut r = emit_byte(0x90)
}


// === aarch64 instruction encoders ===

// MOVZ Xd, #imm16 (64-bit)
// Map logical aarch64 register numbers to x86-64 physical registers
// 0->RAX(0), 1->RCX(1), 2->RDX(2), 3->RBX(3), 4->RSP(4), 5->RBP(5)
// 17->R10(10) scratch, 19->RBX(3) callee-saved, 29->RBP(5) FP, 31->RSP(4)
fn x86_reg(logical: i64) (r: i64)
{
    if logical == 0 { mut r = 0 }
    else { if logical == 1 { mut r = 1 }
    else { if logical == 2 { mut r = 2 }
    else { if logical == 3 { mut r = 3 }
    else { if logical == 4 { mut r = 6 }
    else { if logical == 5 { mut r = 7 }
    else { if logical == 6 { mut r = 8 }
    else { if logical == 7 { mut r = 9 }
    else { if logical == 17 { mut r = 10 }
    else { if logical == 19 { mut r = 3 }
    else { if logical == 29 { mut r = 5 }
    else { if logical == 30 { mut r = 0 }
    else { if logical == 31 { mut r = 4 }
    else { mut r = logical } } } } } } } } } } } } }
}

fn gen_movk(rd: i64, imm16: i64, shift: i64) (r: i64)
{
    if g_target_isa == 1 {
        let shifted: i64 = 0
        mut shifted = (imm16 & 65535) << shift
        mut r = x86_mov_imm(10, shifted)
        mut r = x86_or_reg(x86_reg(rd), 10)
    } else {
        let hw: i64 = 0
        mut hw = shift / 16
        mut r = emit32(0xF2800000 | ((imm16 & 65535) << 5) | ((hw & 3) << 21) | (rd & 31))
    }
}

fn gen_movz(rd: i64, imm16: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_mov_imm(x86_reg(rd), imm16)
    } else {
        mut r = emit32(0xD2800000 | ((imm16 & 65535) << 5) | (rd & 31))
    }
}

// ADD Xd, Xn, Xm (64-bit register)
fn gen_add(rd: i64, rn: i64, rm: i64) (r: i64)
{
    if g_target_isa == 1 {
        if rd == rn {
            mut r = x86_add_reg(x86_reg(rd), x86_reg(rm))
        } else {
            if rd == rm {
                mut r = x86_add_reg(x86_reg(rd), x86_reg(rn))
            } else {
                mut r = x86_mov_reg(x86_reg(rd), x86_reg(rn))
                mut r = x86_add_reg(x86_reg(rd), x86_reg(rm))
            }
        }
    } else {
        mut r = emit32(0x8B000000 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
    }
}

// SUB Xd, Xn, Xm
fn gen_sub(rd: i64, rn: i64, rm: i64) (r: i64)
{
    if g_target_isa == 1 {
        if rd == rn {
            mut r = x86_sub_reg(x86_reg(rd), x86_reg(rm))
        } else {
            if rd == rm {
                mut r = x86_mov_reg(11, x86_reg(rm))
                mut r = x86_mov_reg(x86_reg(rd), x86_reg(rn))
                mut r = x86_sub_reg(x86_reg(rd), 11)
            } else {
                mut r = x86_mov_reg(x86_reg(rd), x86_reg(rn))
                mut r = x86_sub_reg(x86_reg(rd), x86_reg(rm))
            }
        }
    } else {
        mut r = emit32(0xCB000000 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
    }
}

// MUL Xd, Xn, Xm
fn gen_mul(rd: i64, rn: i64, rm: i64) (r: i64)
{
    if g_target_isa == 1 {
        if rd == rn {
            mut r = x86_mul_reg(x86_reg(rd), x86_reg(rm))
        } else {
            if rd == rm {
                mut r = x86_mul_reg(x86_reg(rd), x86_reg(rn))
            } else {
                mut r = x86_mov_reg(x86_reg(rd), x86_reg(rn))
                mut r = x86_mul_reg(x86_reg(rd), x86_reg(rm))
            }
        }
    } else {
        mut r = emit32(0x9B007C00 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
    }
}

// AND Xd, Xn, Xm (64-bit register)
fn gen_and(rd: i64, rn: i64, rm: i64) (r: i64)
{
    if g_target_isa == 1 {
        if rd == rn {
            mut r = x86_and_reg(x86_reg(rd), x86_reg(rm))
        } else {
            if rd == rm {
                mut r = x86_and_reg(x86_reg(rd), x86_reg(rn))
            } else {
                mut r = x86_mov_reg(x86_reg(rd), x86_reg(rn))
                mut r = x86_and_reg(x86_reg(rd), x86_reg(rm))
            }
        }
    } else {
        mut r = emit32(0x8A000000 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
    }
}

// ORR Xd, Xn, Xm (64-bit register)
fn gen_or(rd: i64, rn: i64, rm: i64) (r: i64)
{
    if g_target_isa == 1 {
        if rd == rn {
            mut r = x86_or_reg(x86_reg(rd), x86_reg(rm))
        } else {
            if rd == rm {
                mut r = x86_or_reg(x86_reg(rd), x86_reg(rn))
            } else {
                mut r = x86_mov_reg(x86_reg(rd), x86_reg(rn))
                mut r = x86_or_reg(x86_reg(rd), x86_reg(rm))
            }
        }
    } else {
        mut r = emit32(0xAA000000 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
    }
}

// EOR Xd, Xn, Xm (64-bit register)
fn gen_xor(rd: i64, rn: i64, rm: i64) (r: i64)
{
    if g_target_isa == 1 {
        if rd == rn {
            mut r = x86_xor_reg(x86_reg(rd), x86_reg(rm))
        } else {
            if rd == rm {
                mut r = x86_xor_reg(x86_reg(rd), x86_reg(rn))
            } else {
                mut r = x86_mov_reg(x86_reg(rd), x86_reg(rn))
                mut r = x86_xor_reg(x86_reg(rd), x86_reg(rm))
            }
        }
    } else {
        mut r = emit32(0xCA000000 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
    }
}

// LSR Xd, Xn, Xm (64-bit register shift right)
fn gen_lsr(rd: i64, rn: i64, rm: i64) (r: i64)
{
    if g_target_isa == 1 {
        let rdp: i64 = 0
        mut rdp = x86_reg(rd)
        if rd != rn {
            mut r = x86_mov_reg(rdp, x86_reg(rn))
        }
        if rm != 1 {
            mut r = x86_mov_reg(1, x86_reg(rm))
        }
        mut r = emit_rex(1, 0, 0, (rdp >> 3) & 1)
        mut r = emit_byte(0xD3)
        mut r = emit_modrm(3, 5, rdp & 7)
    } else {
        mut r = emit32(0x9AC02400 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
    }
}

// LSL Xd, Xn, Xm (64-bit register shift left)
fn gen_lsl(rd: i64, rn: i64, rm: i64) (r: i64)
{
    if g_target_isa == 1 {
        let rdp: i64 = 0
        mut rdp = x86_reg(rd)
        if rd != rn {
            mut r = x86_mov_reg(rdp, x86_reg(rn))
        }
        if rm != 1 {
            mut r = x86_mov_reg(1, x86_reg(rm))
        }
        mut r = emit_rex(1, 0, 0, (rdp >> 3) & 1)
        mut r = emit_byte(0xD3)
        mut r = emit_modrm(3, 4, rdp & 7)
    } else {
        mut r = emit32(0x9AC02000 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
    }
}

// MOV Xd, Xm (ORR Xd, XZR, Xm)
fn gen_mov(rd: i64, rm: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_mov_reg(x86_reg(rd), x86_reg(rm))
    } else {
        mut r = emit32(0xAA0003E0 | ((rm & 31) << 16) | (rd & 31))
    }
}

// RET
fn gen_ret() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_ret()
    } else {
        mut r = emit32(0xD65F03C0)
    }
}

// ADD Xd, Xn, #imm12
fn gen_add_imm(rd: i64, rn: i64, imm12: i64) (r: i64)
{
    if g_target_isa == 1 {
        if rd != rn {
            mut r = x86_mov_reg(x86_reg(rd), x86_reg(rn))
        }
        mut r = x86_add_imm(x86_reg(rd), imm12)
    } else {
        mut r = emit32(0x91000000 | ((imm12 & 4095) << 10) | ((rn & 31) << 5) | (rd & 31))
    }
}

// SUB Xd, Xn, #imm12
fn gen_sub_imm(rd: i64, rn: i64, imm12: i64) (r: i64)
{
    if g_target_isa == 1 {
        if rd != rn {
            mut r = x86_mov_reg(x86_reg(rd), x86_reg(rn))
        }
        mut r = x86_sub_imm(x86_reg(rd), imm12)
    } else {
        mut r = emit32(0xD1000000 | ((imm12 & 4095) << 10) | ((rn & 31) << 5) | (rd & 31))
    }
}

// STR Xt, [Xn, #imm12*8]
fn gen_str(rt: i64, rn: i64, imm12: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_store_reg(x86_reg(rt), x86_reg(rn), imm12 * 8)
    } else {
        mut r = emit32(0xF9000000 | ((imm12 & 4095) << 10) | ((rn & 31) << 5) | (rt & 31))
    }
}

// LDR Xt, [Xn, #imm12*8]
fn gen_ldr(rt: i64, rn: i64, imm12: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_load_reg(x86_reg(rt), x86_reg(rn), imm12 * 8)
    } else {
        mut r = emit32(0xF9400000 | ((imm12 & 4095) << 10) | ((rn & 31) << 5) | (rt & 31))
    }
}

// STUR Xt, [Xn, #imm9] (signed offset)
// Handles three cases for the signed 9-bit immediate:
//   1. imm9 in [-256, 255]: emit a single STUR instruction.
//   2. imm9 > 255 (large positive): load offset into X17 (scratch / IP1)
//      via MOVZ+MOVK, ADD to base register, then STR from computed addr.
//      X17 is safe to clobber (caller-saved, not preserved across calls).
//   3. imm9 < -256 (large negative): negate to absolute value, load into
//      X17 via MOVZ+MOVK, SUB from base register, then STR.
fn gen_stur(rt: i64, rn: i64, imm9: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_store_reg(x86_reg(rt), x86_reg(rn), imm9)
    } else {
    if imm9 >= -256 {
        if imm9 <= 255 {
            mut r = emit32(0xF8000000 | ((imm9 & 511) << 12) | ((rn & 31) << 5) | (rt & 31))
        } else {
            mut r = gen_movz(17, imm9 & 65535)
            if (imm9 >> 16) != 0 {
                mut r = gen_movk(17, (imm9 >> 16) & 65535, 16)
            }
            mut r = emit32(0x8B110000 | ((rn & 31) << 5) | (17 & 31))
            mut r = emit32(0xF9000000 | ((17 & 31) << 5) | (rt & 31))
        }
    } else {
        let absval: i64 = 0
        mut absval = 0 - imm9
        mut r = gen_movz(17, absval & 65535)
        if (absval >> 16) != 0 {
            mut r = gen_movk(17, (absval >> 16) & 65535, 16)
        }
        mut r = emit32(0xCB110000 | ((rn & 31) << 5) | (17 & 31))
        mut r = emit32(0xF9000000 | ((17 & 31) << 5) | (rt & 31))
    }
    }
}

// LDUR Xt, [Xn, #imm9] (signed offset)
// Same three-case strategy as gen_stur but for loads:
//   1. imm9 in [-256, 255]: emit a single LDUR instruction.
//   2. imm9 > 255: load offset into X17 (scratch / IP1) via MOVZ+MOVK,
//      ADD to base, then LDR from computed address.
//   3. imm9 < -256: load absolute value into X17 via MOVZ+MOVK,
//      SUB from base, then LDR.
fn gen_ldur(rt: i64, rn: i64, imm9: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_load_reg(x86_reg(rt), x86_reg(rn), imm9)
    } else {
    if imm9 >= -256 {
        if imm9 <= 255 {
            mut r = emit32(0xF8400000 | ((imm9 & 511) << 12) | ((rn & 31) << 5) | (rt & 31))
        } else {
            mut r = gen_movz(17, imm9 & 65535)
            if (imm9 >> 16) != 0 {
                mut r = gen_movk(17, (imm9 >> 16) & 65535, 16)
            }
            mut r = emit32(0x8B110000 | ((rn & 31) << 5) | (17 & 31))
            mut r = emit32(0xF9400000 | ((17 & 31) << 5) | (rt & 31))
        }
    } else {
        let absval: i64 = 0
        mut absval = 0 - imm9
        mut r = gen_movz(17, absval & 65535)
        if (absval >> 16) != 0 {
            mut r = gen_movk(17, (absval >> 16) & 65535, 16)
        }
        mut r = emit32(0xCB110000 | ((rn & 31) << 5) | (17 & 31))
        mut r = emit32(0xF9400000 | ((17 & 31) << 5) | (rt & 31))
    }
    }
}

fn gen_ldrb_reg(rt: i64, rn: i64, rm: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_mov_reg(11, x86_reg(rn))
        mut r = x86_add_reg(11, x86_reg(rm))
        mut r = x86_load8_reg(x86_reg(rt), 11, 0)
    } else {
        mut r = emit32(0x38606800 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rt & 31))
    }
}

fn gen_strb_reg(rt: i64, rn: i64, rm: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_mov_reg(11, x86_reg(rn))
        mut r = x86_add_reg(11, x86_reg(rm))
        mut r = x86_store8_reg(x86_reg(rt), 11, 0)
    } else {
        mut r = emit32(0x38206800 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rt & 31))
    }
}

fn gen_ldr_reg(rt: i64, rn: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_load_reg(x86_reg(rt), x86_reg(rn), 0)
    } else {
        mut r = emit32(0xF9400000 | ((rn & 31) << 5) | (rt & 31))
    }
}

fn gen_str_reg(rt: i64, rn: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_store_reg(x86_reg(rt), x86_reg(rn), 0)
    } else {
        mut r = emit32(0xF9000000 | ((rn & 31) << 5) | (rt & 31))
    }
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
    if g_target_isa == 1 {
        if rm == 31 {
            mut r = x86_or_reg(x86_reg(rn), x86_reg(rn))
        } else {
            if rn == 31 {
                mut r = x86_or_reg(x86_reg(rm), x86_reg(rm))
            } else {
                mut r = x86_cmp_reg(x86_reg(rn), x86_reg(rm))
            }
        }
    } else {
        mut r = emit32(0xEB00001F | ((rm & 31) << 16) | ((rn & 31) << 5))
    }
}

// B.cond offset (condition codes: 0=EQ, 1=NE, 10=GE, 11=LT, 12=GT, 13=LE)
fn gen_bcond(cond: i64, offset: i64) (r: i64)
{
    if g_target_isa == 1 {
        let x86_cond: i64 = 0
        if cond == 0 { mut x86_cond = 4 }
        if cond == 1 { mut x86_cond = 5 }
        if cond == 10 { mut x86_cond = 13 }
        if cond == 11 { mut x86_cond = 12 }
        if cond == 12 { mut x86_cond = 15 }
        if cond == 13 { mut x86_cond = 14 }
        mut r = x86_jcc(x86_cond, offset - 6)
    } else {
        let off19: i64 = 0
        mut off19 = (offset >> 2) & 524287
        mut r = emit32(0x54000000 | (off19 << 5) | (cond & 15))
    }
}

// B offset (unconditional)
fn gen_b(offset: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_jmp(offset - 5)
    } else {
        let off26: i64 = 0
        mut off26 = (offset >> 2) & 67108863
        mut r = emit32(0x14000000 | off26)
    }
}

// BL offset
fn gen_bl(offset: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_call(offset - 5)
    } else {
        let off26: i64 = 0
        mut off26 = (offset >> 2) & 67108863
        mut r = emit32(0x94000000 | off26)
    }
}

// SDIV Xd, Xn, Xm
fn gen_sdiv(rd: i64, rn: i64, rm: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_mov_reg(11, x86_reg(rm))
        mut r = x86_mov_reg(0, x86_reg(rn))
        mut r = x86_cdq()
        mut r = x86_idiv_reg(11)
        mut r = x86_mov_reg(x86_reg(rd), 0)
    } else {
        mut r = emit32(0x9AC00C00 | ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31))
    }
}

// MSUB Xd, Xn, Xm, X0 (for modulo: result = X0 - Xn * Xm)
fn gen_msub(rd: i64, rn: i64, rm: i64, ra: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_mov_reg(11, x86_reg(rn))
        mut r = x86_mul_reg(11, x86_reg(rm))
        mut r = x86_mov_reg(x86_reg(rd), x86_reg(ra))
        mut r = x86_sub_reg(x86_reg(rd), 11)
    } else {
        mut r = emit32(0x9B008000 | ((rm & 31) << 16) | ((ra & 31) << 10) | ((rn & 31) << 5) | (rd & 31))
    }
}

// Push X0 to stack (STR X0, [SP, #-16]!)
fn gen_push() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_push_reg(0)
    } else {
        mut r = emit32(0xF81F0FE0)
    }
}

// Pop to X1 (LDR X1, [SP], #16)
fn gen_pop_x1() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_pop_reg(1)
    } else {
        mut r = emit32(0xF84107E1)
    }
}

// === Variable table ===
fn var_lookup(name: i64) (r: i64)
{
    let i: i64 = 0
    let h: i64 = 0
    mut h = str_hash(name)
    mut i = 0
    mut r = -1
    while i < g_var_count {
        if __mem_load(g_var_name + i * 8) == h {
            mut r = __mem_load(g_var_off + i * 8)
        }
        mut i = i + 1
    }
}

fn var_store(h: i64, offset: i64) (r: i64)
{
    __mem_store(g_var_name + g_var_count * 8, h)
    __mem_store(g_var_off + g_var_count * 8, offset)
    mut g_var_count = g_var_count + 1
    mut r = 0
}

fn var_add(name: i64, offset: i64) (r: i64)
{
    let h: i64 = 0
    mut h = str_hash(name)
    mut r = var_store(h, offset)
}

fn glob_add(name: i64) (r: i64)
{
    __mem_store(g_glob_name + g_glob_count * 8, name)
    __mem_store(g_glob_off + g_glob_count * 8, g_glob_count)
    __mem_store(g_glob_val + g_glob_count * 8, 0)
    mut g_glob_count = g_glob_count + 1
    mut r = g_glob_count - 1
}

fn glob_lookup(name: i64) (r: i64)
{
    let i: i64 = 0
    mut r = -1
    mut i = 0
    while i < g_glob_count {
        if my_str_eq(__mem_load(g_glob_name + i * 8), name) == 1 {
            mut r = __mem_load(g_glob_off + i * 8)
        }
        mut i = i + 1
    }
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

fn fn_store(h: i64, offset: i64) (r: i64)
{
    __mem_store(g_fn_name + g_fn_count * 8, h)
    __mem_store(g_fn_off + g_fn_count * 8, offset)
    mut g_fn_count = g_fn_count + 1
    mut r = 0
}

fn fn_add(name: i64, offset: i64) (r: i64)
{
    mut r = fn_store(name, offset)
}

fn fn_lookup(name: i64) (r: i64)
{
    let i: i64 = 0
    let stored: i64 = 0
    let result: i64 = 0
    mut i = 0
    while i < g_fn_count {
        mut stored = __mem_load(g_fn_name + i * 8)
        if my_str_eq(stored, name) == 1 {
            mut result = __mem_load(g_fn_off + i * 8)
        }
        mut i = i + 1
    }
    mut r = result
}


fn enum_add(name: i64, tag: i64, has_arg: i64) (r: i64)
{
    __mem_store(g_enum_name + g_enum_count * 8, name)
    __mem_store(g_enum_tag + g_enum_count * 8, tag)
    __mem_store(g_enum_has_arg + g_enum_count * 8, has_arg)
    mut g_enum_count = g_enum_count + 1
    mut r = 0
}

fn enum_lookup(name: i64) (r: i64)
{
    let i: i64 = 0
    let found: i64 = 0
    mut i = 0
    mut found = 0
    mut r = -1
    while i < g_enum_count {
        if my_str_eq(__mem_load(g_enum_name + i * 8), name) == 1 {
            mut found = 1
            mut r = __mem_load(g_enum_tag + i * 8)
        }
        mut i = i + 1
    }
}

fn enum_has_arg_lookup(name: i64) (r: i64)
{
    let i: i64 = 0
    mut r = 0
    mut i = 0
    while i < g_enum_count {
        if my_str_eq(__mem_load(g_enum_name + i * 8), name) == 1 {
            mut r = __mem_load(g_enum_has_arg + i * 8)
        }
        mut i = i + 1
    }
}

fn patch_one(ppos: i64, pname: i64) (r: i64)
{
    let foff: i64 = 0
    let rel: i64 = 0
    let off26: i64 = 0
    mut foff = fn_lookup(pname)
    if foff > 0 {
        mut rel = foff - ppos
        if g_target_isa == 1 {
            mut rel = rel - 5
            mut r = __byte_store(g_code, ppos + 1, rel & 255)
            mut r = __byte_store(g_code, ppos + 2, (rel >> 8) & 255)
            mut r = __byte_store(g_code, ppos + 3, (rel >> 16) & 255)
            mut r = __byte_store(g_code, ppos + 4, (rel >> 24) & 255)
        } else {
            mut off26 = (rel >> 2) & 67108863
            mut r = emit32_at(ppos, 0x94000000 | off26)
        }
    } else {
        mut r = ext_add(pname, ppos)
    }
    mut r = 0
}

fn ext_find(name: i64) (r: i64)
{
    let i: i64 = 0
    mut r = -1
    mut i = 0
    while i < g_ext_count {
        if my_str_eq(__mem_load(g_ext_name + i * 8), name) == 1 {
            mut r = i
        }
        mut i = i + 1
    }
}

fn ext_add(name: i64, pos: i64) (r: i64)
{
    let idx: i64 = 0
    mut idx = ext_find(name)
    if idx < 0 {
        mut idx = g_ext_count
        __mem_store(g_ext_name + g_ext_count * 8, name)
        mut g_ext_count = g_ext_count + 1
    }
    __mem_store(g_ext_pos + idx * 8, pos)
    mut r = idx
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
fn gen_cmp_lt() (r: i64) { if g_target_isa == 1 { mut r = x86_setcc(12, 0) mut r = x86_movzx_64(0, 0) } else { mut r = emit32(2594154464) } mut r = 0 }
fn gen_cmp_gt() (r: i64) { if g_target_isa == 1 { mut r = x86_setcc(15, 0) mut r = x86_movzx_64(0, 0) } else { mut r = emit32(2594166752) } mut r = 0 }
fn gen_cmp_le() (r: i64) { if g_target_isa == 1 { mut r = x86_setcc(14, 0) mut r = x86_movzx_64(0, 0) } else { mut r = emit32(2594162656) } mut r = 0 }
fn gen_cmp_ge() (r: i64) { if g_target_isa == 1 { mut r = x86_setcc(13, 0) mut r = x86_movzx_64(0, 0) } else { mut r = emit32(2594158560) } mut r = 0 }
fn gen_cmp_eq2() (r: i64) { if g_target_isa == 1 { mut r = x86_setcc(4, 0) mut r = x86_movzx_64(0, 0) } else { mut r = emit32(2594117600) } mut r = 0 }
fn gen_cmp_ne() (r: i64) { if g_target_isa == 1 { mut r = x86_setcc(5, 0) mut r = x86_movzx_64(0, 0) } else { mut r = emit32(2594113504) } mut r = 0 }

fn gen_mod() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_mov_reg(11, x86_reg(0))
        mut r = x86_mov_reg(0, x86_reg(1))
        mut r = x86_cdq()
        mut r = x86_idiv_reg(11)
        mut r = x86_mov_reg(x86_reg(0), 2)
    } else {
        mut r = gen_sdiv(2, 1, 0)
        mut r = gen_msub(0, 2, 0, 1)
    }
}

fn gen_arith_op(op: i64) (r: i64)
{
    if op == 43 {
        mut r = gen_add(0, 1, 0)
    } else {
        if op == 45 {
            mut r = gen_sub(0, 1, 0)
        } else {
            if op == 42 {
                mut r = gen_mul(0, 1, 0)
            } else {
                if op == 47 {
                    mut r = gen_sdiv(0, 1, 0)
                } else {
                    if op == 37 {
                        mut r = gen_mod()
                    } else {
                        if op == 38 {
                            mut r = gen_and(0, 1, 0)
                        } else {
                            if op == 124 {
                                mut r = gen_or(0, 1, 0)
                            } else {
                                if op == 94 {
                                    mut r = gen_xor(0, 1, 0)
                                } else {
                                    if op == 15854 {
                                        mut r = gen_lsr(0, 1, 0)
                                    } else {
                                        if op == 15420 {
                                            mut r = gen_lsl(0, 1, 0)
                                        } else {
                                            if op == 9798 {
                                                mut r = gen_and(0, 1, 0)
                                            } else {
                                                if op == 31870 {
                                                    mut r = gen_or(0, 1, 0)
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
        }
    }
    mut r = 0
}

fn gen_binop(op: i64) (r: i64)
{
    if op == 15677 {
        mut r = gen_cmp(1, 0)
        mut r = gen_cmp_eq2()
    } else {
        if op == 8645 {
            mut r = gen_cmp(1, 0)
            mut r = gen_cmp_ne()
        } else {
            if op == 60 {
                mut r = gen_cmp(1, 0)
                mut r = gen_cmp_lt()
            } else {
                if op == 62 {
                    mut r = gen_cmp(1, 0)
                    mut r = gen_cmp_gt()
                } else {
                    if op == 15485 {
                        mut r = gen_cmp(1, 0)
                        mut r = gen_cmp_le()
                    } else {
                        if op == 15997 {
                            mut r = gen_cmp(1, 0)
                            mut r = gen_cmp_ge()
                        } else {
                            mut r = gen_arith_op(op)
                        }
                    }
                }
            }
        }
    }
    mut r = 0
}
fn gen_glob_load(off: i64) (r: i64)
{
    mut r = gen_ldr(0, 19, off)
    mut r = 0
}

fn gen_glob_store(off: i64) (r: i64)
{
    mut r = gen_str(0, 19, off)
    mut r = 0
}

fn gen_expr_glob(v: i64) (r: i64)
{
    let goff: i64 = 0
    mut goff = glob_lookup(v)
    if goff >= 0 {
        mut r = gen_glob_load(goff)
    } else {
        mut r = gen_movz(0, 0)
    }
}

fn gen_expr_ident(v: i64) (r: i64)
{
    let off: i64 = 0
    let etag: i64 = 0
    mut etag = enum_lookup(v)
    if etag >= 0 {
        mut r = gen_enum_alloc()
        mut r = gen_movz(1, etag)
        mut r = gen_stur(1, 0, 0)
    } else {
        mut off = var_lookup(v)
        if off >= 0 {
            mut r = gen_ldur(0, 29, 0 - (off + 1) * 8)
        } else {
            mut r = gen_expr_glob(v)
        }
    }
    mut r = 0
}

fn gen_assign_glob(name: i64) (r: i64)
{
    let goff: i64 = 0
    mut goff = glob_lookup(name)
    if goff >= 0 {
        mut r = gen_glob_store(goff)
    }
    mut r = 0
}

fn gen_expr_assign(a: i64, b: i64) (r: i64)
{
    let off: i64 = 0
    mut r = gen_expr(b)
    mut off = var_lookup(__mem_load(g_ast_val + a * 8))
    if off >= 0 {
        mut r = gen_stur(0, 29, 0 - (off + 1) * 8)
    } else {
        mut r = gen_assign_glob(__mem_load(g_ast_val + a * 8))
    }
    mut r = 0
}

fn gen_expr_binop(t1: i64, t2: i64, op: i64) (r: i64)
{
    mut r = gen_expr(t1)
    mut r = gen_push()
    mut r = gen_expr(t2)
    mut r = gen_pop_x1()
    mut r = gen_binop(op)
    mut r = 0
}

fn str_const_add(s: i64) (r: i64)
{
    let i: i64 = 0
    mut i = 0
    while i < g_str_const_count {
        if __mem_load(g_str_const + i * 8) == s {
            mut r = g_glob_count + i
        }
        mut i = i + 1
    }
    mut r = g_glob_count + g_str_const_count
    __mem_store(g_str_const + g_str_const_count * 8, s)
    mut g_str_const_count = g_str_const_count + 1
}

fn str_adr_patch_add(pos: i64, idx: i64) (r: i64)
{
    __mem_store(g_adr_patch_pos + g_adr_patch_count * 8, pos)
    __mem_store(g_adr_patch_idx + g_adr_patch_count * 8, idx)
    mut g_adr_patch_count = g_adr_patch_count + 1
    mut r = 0
}

fn gen_expr_str(v: i64) (r: i64)
{
    let idx: i64 = 0
    let adr_pos: i64 = 0
    mut idx = str_const_add(v)
    mut adr_pos = g_code_pos
    if g_target_isa == 1 {
        mut r = emit_byte(0x48)
        mut r = emit_byte(0x8D)
        mut r = emit_byte(0x05)
        mut r = emit_byte(0)
        mut r = emit_byte(0)
        mut r = emit_byte(0)
        mut r = emit_byte(0)
    } else {
        mut r = emit32(0x10000000)
    }
    mut r = str_adr_patch_add(adr_pos, idx)
    mut r = 0
}

fn gen_expr_unop(op: i64, a: i64) (r: i64)
{
    mut r = gen_expr(a)
    if op == 45 {
        mut r = gen_sub(0, 31, 0)
    }
    mut r = 0
}

fn gen_expr_dispatch(k: i64, v: i64, a: i64, b: i64) (r: i64)
{
    if k == 1 {
        mut r = gen_movz(0, v & 65535)
        if (v >> 16) != 0 {
            mut r = gen_movk(0, (v >> 16) & 65535, 16)
        }
        if (v >> 32) != 0 {
            mut r = gen_movk(0, (v >> 32) & 65535, 32)
        }
        if (v >> 48) != 0 {
            mut r = gen_movk(0, (v >> 48) & 65535, 48)
        }
    } else {
        if k == 2 {
            mut r = gen_expr_str(v)
        } else {
            if k == 3 {
                mut r = gen_expr_ident(v)
            } else {
                if k == 5 {
                    mut r = gen_expr_binop(a, b, v)
                } else {
                    if k == 4 {
                        mut r = gen_call(v, a)
                    } else {
                        if k == 6 {
                            mut r = gen_expr_unop(v, a)
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
        }
    }
}

fn gen_expr(nd: i64) (r: i64)
{
    let k: i64 = 0
    let v: i64 = 0
    let a: i64 = 0
    let b: i64 = 0
    mut k = ast_field(nd, g_ast_kind)
    mut v = ast_field(nd, g_ast_val)
    mut a = ast_field(nd, g_ast_a)
    mut b = ast_field(nd, g_ast_b)
    mut r = gen_expr_dispatch(k, v, a, b)
    mut r = 0
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
                    } else {
                        if i == 4 {
                            mut r = gen_pop_x4()
                        } else {
                            if i == 5 {
                                mut r = gen_pop_x5()
                            } else {
                                mut r = gen_pop_discard()
                            }
                        }
                    }
                }
            }
        }
    }
    mut r = 0
}

fn check_bytes(name: i64, b0: i64, b1: i64, b2: i64, b3: i64) (r: i64)
{
    mut r = 0
    if __byte_load(name, 0) == b0 {
        if __byte_load(name, 1) == b1 {
            if __byte_load(name, 2) == b2 {
                if __byte_load(name, 3) == b3 {
                    mut r = 1
                }
            }
        }
    }
}

fn is_builtin_name(name: i64) (r: i64)
{
    mut r = 0
    if __byte_load(name, 0) == 95 {
        if __byte_load(name, 1) == 95 {
            let b2: i64 = 0
            mut b2 = __byte_load(name, 2)
            if b2 == 98 {
                mut r = check_bytes(name, 95, 95, 98, 121)
            } else {
                if b2 == 109 {
                    if __byte_load(name, 3) == 101 {
                        mut r = check_bytes(name, 95, 95, 109, 101)
                    } else {
                        if __byte_load(name, 3) == 97 {
                            mut r = check_bytes(name, 95, 95, 109, 97)
                        }
                    }
                } else {
                    if b2 == 97 {
                        if __byte_load(name, 3) == 108 {
                            mut r = check_bytes(name, 95, 95, 97, 108)
                        }
                    } else {
                        if b2 == 115 {
                            if __byte_load(name, 3) == 116 {
                                mut r = check_bytes(name, 95, 95, 115, 116)
                            } else {
                                if __byte_load(name, 3) == 121 {
                                    mut r = check_bytes(name, 95, 95, 115, 121)
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        if __byte_load(name, 0) == 112 {
            if __byte_load(name, 1) == 114 {
                if __byte_load(name, 2) == 105 {
                    if __byte_load(name, 3) == 110 {
                        if __byte_load(name, 4) == 116 {
                            if __byte_load(name, 5) == 0 {
                                mut r = 1
                            } else {
                                if __byte_load(name, 5) == 108 {
                                    if __byte_load(name, 6) == 110 {
                                        mut r = 1
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

fn gen_eval_one(arg_nd: i64) (r: i64)
{
    let k: i64 = 0
    let actual: i64 = 0
    let next_arg: i64 = 0
    mut k = __mem_load(g_ast_kind + arg_nd * 8)
    mut actual = arg_nd
    mut next_arg = 0
    if k == 20 {
        mut actual = __mem_load(g_ast_a + arg_nd * 8)
        mut next_arg = __mem_load(g_ast_b + arg_nd * 8)
    }
    mut r = gen_expr(actual)
    mut r = gen_push()
    mut r = next_arg
}

fn gen_eval_args(first_arg: i64) (r: i64)
{
    let count: i64 = 0
    let cur: i64 = 0
    mut count = 0
    mut cur = first_arg
    while cur > 0 {
        mut cur = gen_eval_one(cur)
        mut count = count + 1
    }
    mut r = count
}

fn gen_str_eq_swap() (r: i64)
{
    mut r = gen_mov(2, 0)
    mut r = gen_mov(0, 1)
    mut r = gen_mov(1, 2)
    mut r = 0
}

fn gen_str_eq_cmp() (r: i64)
{
    mut r = gen_ldrb_reg(3, 0, 4)
    mut r = gen_ldrb_reg(5, 1, 4)
    mut r = gen_cmp(3, 5)
    mut r = 0
}

fn gen_str_eq_branch() (r: i64)
{
    let ne_pos: i64 = 0
    let eq_pos: i64 = 0
    mut ne_pos = g_code_pos
    mut r = gen_bcond(1, 0)
    mut r = gen_cmp(3, 31)
    mut eq_pos = g_code_pos
    mut r = gen_bcond(0, 0)
    mut r = patch_bcond(ne_pos, g_code_pos - ne_pos)
    mut r = 0
}

fn gen_str_eq_inc(loop_pos: i64) (r: i64)
{
    mut r = gen_add_imm(4, 4, 1)
    mut r = gen_b(loop_pos - g_code_pos)
    mut r = 0
}

fn gen_str_eq_ne() (r: i64)
{
    mut r = gen_movz(0, 0)
    mut r = 0
}

fn gen_str_eq_inline() (r: i64)
{
    let loop_pos: i64 = 0
    let ne_branch_pos: i64 = 0
    let eq_branch_pos: i64 = 0
    let skip_branch_pos: i64 = 0
    // Pop args: s2 -> X1, s1 -> X0
    mut r = gen_pop_x1()
    mut r = gen_pop_x0()
    // Save caller-saved regs
    mut r = gen_caller_save()
    // Reload args from saved area
    mut r = gen_ldr(0, 31, 2)
    mut r = gen_ldr(1, 31, 3)
    mut r = gen_movz(4, 0)
    mut r = gen_str_eq_swap()
    mut loop_pos = g_code_pos
    mut r = gen_str_eq_cmp()
    // B.NE not_equal (forward, will be patched)
    mut ne_branch_pos = g_code_pos
    mut r = gen_bcond(1, 0)
    // Check if byte is zero (end of string)
    mut r = gen_cmp(3, 31)
    // B.EQ equal (forward, will be patched)
    mut eq_branch_pos = g_code_pos
    mut r = gen_bcond(0, 0)
    // Increment counter and loop back
    mut r = gen_str_eq_inc(loop_pos)
    // equal: return 1
    mut r = patch_bcond(eq_branch_pos, g_code_pos - eq_branch_pos)
    mut r = gen_movz(0, 1)
    // B done (skip not_equal case)
    mut skip_branch_pos = g_code_pos
    mut r = gen_b(0)
    // not_equal: patch B.NE to here, return 0
    mut r = patch_bcond(ne_branch_pos, g_code_pos - ne_branch_pos)
    mut r = gen_str_eq_ne()
    // done: patch skip branch to here
    mut r = patch_b(skip_branch_pos, g_code_pos - skip_branch_pos)
    // Save return value and restore caller-saved regs
    mut r = gen_save_retval()
    mut r = gen_caller_restore()
    mut r = gen_load_retval()
    mut r = gen_add_sp()
    mut r = 0
}

fn gen_byte_load_builtin() (r: i64)
{
    mut r = gen_mov(2, 0)
    mut r = gen_mov(0, 1)
    mut r = gen_mov(1, 2)
    mut r = gen_ldrb_reg(0, 0, 1)
}

fn gen_byte_store_builtin() (r: i64)
{
    mut r = gen_mov(3, 0)
    mut r = gen_mov(0, 2)
    mut r = gen_mov(2, 3)
    mut r = gen_strb_reg(0, 2, 1)
}

fn gen_mem_load_builtin() (r: i64)
{
    mut r = gen_ldr_reg(0, 0)
}

fn gen_mem_store_builtin() (r: i64)
{
    mut r = gen_mov(2, 0)
    mut r = gen_mov(0, 1)
    mut r = gen_mov(1, 2)
    mut r = gen_str_reg(0, 1)
}

fn gen_byte_builtin() (r: i64)
{
    if __byte_load(g_call_name, 7) == 108 {
        mut r = gen_pop_x1()
        mut r = gen_pop_x0()
        mut r = gen_byte_load_builtin()
    } else {
        mut r = gen_pop_x2()
        mut r = gen_pop_x1()
        mut r = gen_pop_x0()
        mut r = gen_byte_store_builtin()
    }
}

fn gen_mem_builtin() (r: i64)
{
    if __byte_load(g_call_name, 6) == 108 {
        mut r = gen_pop_x0()
        mut r = gen_mem_load_builtin()
    } else {
        mut r = gen_pop_x1()
        mut r = gen_pop_x0()
        mut r = gen_mem_store_builtin()
    }
}


fn gen_syscall_builtin(arg_count: i64) (r: i64)
{
    let n: i64 = 0
    mut n = arg_count
    if g_target_isa == 1 {
        if n > 6 { mut r = x86_pop_reg(x86_reg(5)); mut n = n - 1 }
        if n > 5 { mut r = x86_pop_reg(x86_reg(4)); mut n = n - 1 }
        if n > 4 { mut r = x86_pop_reg(x86_reg(3)); mut n = n - 1 }
        if n > 3 { mut r = x86_pop_reg(x86_reg(2)); mut n = n - 1 }
        if n > 2 { mut r = x86_pop_reg(x86_reg(1)); mut n = n - 1 }
        if n > 1 { mut r = x86_pop_reg(x86_reg(0)); mut n = n - 1 }
        mut r = x86_pop_reg(x86_reg(6))
        mut r = gen_caller_save()
        if arg_count > 1 { mut r = x86_load_reg(7, 4, 16) }
        if arg_count > 2 { mut r = x86_load_reg(6, 4, 24) }
        if arg_count > 3 { mut r = x86_load_reg(2, 4, 32) }
        if arg_count > 4 { mut r = x86_load_reg(10, 4, 40) }
        if arg_count > 5 { mut r = x86_load_reg(8, 4, 48) }
        if arg_count > 6 { mut r = x86_load_reg(9, 4, 56) }
        mut r = x86_load_reg(0, 4, 64)
        mut r = x86_syscall()
    } else {
        if n > 6 { mut r = emit32(0xF84107E5); mut n = n - 1 }
        if n > 5 { mut r = emit32(0xF84107E4); mut n = n - 1 }
        if n > 4 { mut r = emit32(0xF84107E3); mut n = n - 1 }
        if n > 3 { mut r = emit32(0xF84107E2); mut n = n - 1 }
        if n > 2 { mut r = emit32(0xF84107E1); mut n = n - 1 }
        if n > 1 { mut r = emit32(0xF84107E0); mut n = n - 1 }
        mut r = emit32(0xF84107E6)
        mut r = gen_caller_save()
        if arg_count > 1 { mut r = gen_ldr(0, 31, 2) }
        if arg_count > 2 { mut r = gen_ldr(1, 31, 3) }
        if arg_count > 3 { mut r = gen_ldr(2, 31, 4) }
        if arg_count > 4 { mut r = gen_ldr(3, 31, 5) }
        if arg_count > 5 { mut r = gen_ldr(4, 31, 6) }
        if arg_count > 6 { mut r = gen_ldr(5, 31, 7) }
        mut r = gen_ldr(6, 31, 8)
        if g_target_os == 0 {
            mut r = gen_mov(16, 6)
            mut r = emit32(0xD4001001)
        } else {
            mut r = gen_mov(8, 6)
            mut r = emit32(0xD4000001)
        }
    }
    mut r = gen_call_finish()
    mut r = 0
}

fn gen_print_int_builtin(is_println: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_pop_reg(0)
        mut r = x86_sub_imm(4, 32)
        mut r = x86_mov_reg(6, 4)
        mut r = x86_add_imm(6, 31)
        mut r = x86_mov_imm(1, 0)
        mut r = x86_store8_reg(1, 6, 0)
        mut r = x86_or_reg(0, 0)
        let zero_pos: i64 = 0
        mut zero_pos = g_code_pos
        mut r = x86_jcc(4, 0)
        mut r = x86_mov_imm(1, 0)
        mut r = x86_or_reg(0, 0)
        let neg_pos: i64 = 0
        mut neg_pos = g_code_pos
        mut r = x86_jcc(13, 0)
        mut r = x86_neg_reg(0)
        mut r = x86_mov_imm(1, 1)
        let pos_label: i64 = 0
        mut pos_label = g_code_pos
        mut r = patch_bcond(neg_pos, pos_label - neg_pos)
        let loop_pos: i64 = 0
        mut loop_pos = g_code_pos
        mut r = x86_mov_imm(11, 10)
        mut r = x86_cdq()
        mut r = x86_idiv_reg(11)
        mut r = x86_add_imm(2, 48)
        mut r = x86_sub_imm(6, 1)
        mut r = x86_store8_reg(2, 6, 0)
        mut r = x86_or_reg(0, 0)
        let cbnz_pos: i64 = 0
        mut cbnz_pos = g_code_pos
        mut r = x86_jcc(5, 0)
        mut r = patch_bcond(cbnz_pos, loop_pos - cbnz_pos)
        mut r = x86_or_reg(1, 1)
        let skip_minus: i64 = 0
        mut skip_minus = g_code_pos
        mut r = x86_jcc(4, 0)
        mut r = x86_sub_imm(6, 1)
        mut r = x86_mov_imm(2, 45)
        mut r = x86_store8_reg(2, 6, 0)
        let write_label: i64 = 0
        mut write_label = g_code_pos
        mut r = patch_bcond(skip_minus, write_label - skip_minus)
        let done_branch: i64 = 0
        mut done_branch = g_code_pos
        mut r = x86_jmp(0)
        let zero_label: i64 = 0
        mut zero_label = g_code_pos
        mut r = patch_bcond(zero_pos, zero_label - zero_pos)
        mut r = x86_mov_imm(2, 48)
        mut r = x86_sub_imm(6, 1)
        mut r = x86_store8_reg(2, 6, 0)
        let done_label: i64 = 0
        mut done_label = g_code_pos
        mut r = patch_b(done_branch, done_label - done_branch)
        mut r = x86_mov_reg(2, 4)
        mut r = x86_add_imm(2, 32)
        mut r = x86_sub_reg(2, 6)
        mut r = x86_mov_imm(0, TSC_WRITE)
        mut r = x86_mov_imm(7, 1)
        mut r = x86_syscall()
        if is_println == 1 {
            mut r = x86_sub_imm(4, 16)
            mut r = x86_mov_imm(0, 10)
            mut r = x86_store8_reg(0, 4, 0)
            mut r = x86_mov_imm(0, TSC_WRITE)
            mut r = x86_mov_imm(7, 1)
            mut r = x86_mov_reg(6, 4)
            mut r = x86_mov_imm(2, 1)
            mut r = x86_syscall()
            mut r = x86_add_imm(4, 16)
        }
        mut r = x86_add_imm(4, 32)
    } else {
        mut r = emit32(0xF84107E0)
        mut r = emit32(0xD10083FF)
        mut r = emit32(0x91007BE1)
        mut r = emit32(0x39007BFF)
        mut r = gen_cmp(0, 31)
        let zero_pos2: i64 = 0
        mut zero_pos2 = g_code_pos
        mut r = gen_bcond(0, 0)
        mut r = gen_movz(2, 0)
        mut r = gen_cmp(0, 31)
        let neg_pos2: i64 = 0
        mut neg_pos2 = g_code_pos
        mut r = gen_bcond(10, 0)
        mut r = emit32(0xCB0003E0)
        mut r = gen_movz(2, 1)
        let pos_label2: i64 = 0
        mut pos_label2 = g_code_pos
        mut r = patch_bcond(neg_pos2, pos_label2 - neg_pos2)
        let loop_pos2: i64 = 0
        mut loop_pos2 = g_code_pos
        mut r = gen_movz(3, 10)
        mut r = emit32(0x9AC30804)
        mut r = emit32(0x9B038085)
        mut r = gen_add_imm(5, 5, 48)
        mut r = gen_sub_imm(1, 1, 1)
        mut r = emit32(0x39000025)
        mut r = gen_mov(0, 4)
        mut r = gen_cmp(0, 31)
        let cbnz_pos2: i64 = 0
        mut cbnz_pos2 = g_code_pos
        mut r = gen_bcond(1, 0)
        mut r = patch_bcond(cbnz_pos2, loop_pos2 - cbnz_pos2)
        mut r = gen_cmp(2, 31)
        let skip_minus2: i64 = 0
        mut skip_minus2 = g_code_pos
        mut r = gen_bcond(0, 0)
        mut r = gen_sub_imm(1, 1, 1)
        mut r = gen_movz(5, 45)
        mut r = emit32(0x39000025)
        let write_label2: i64 = 0
        mut write_label2 = g_code_pos
        mut r = patch_bcond(skip_minus2, write_label2 - skip_minus2)
        let done_branch2: i64 = 0
        mut done_branch2 = g_code_pos
        mut r = gen_b(0)
        let zero_label2: i64 = 0
        mut zero_label2 = g_code_pos
        mut r = patch_bcond(zero_pos2, zero_label2 - zero_pos2)
        mut r = gen_movz(5, 48)
        mut r = emit32(0x39006C65)
        mut r = emit32(0x910073E1)
        let done_label2: i64 = 0
        mut done_label2 = g_code_pos
        mut r = patch_b(done_branch2, done_label2 - done_branch2)
        mut r = emit32(0x910083E3)
        mut r = emit32(0xCB010062)
        mut r = gen_mov(4, 1)
        mut r = gen_mov(5, 2)
        mut r = gen_movz(0, 1)
        mut r = gen_mov(1, 4)
        mut r = gen_mov(2, 5)
        mut r = gen_movz(16, TSC_WRITE)
        if g_target_os == 0 {
            mut r = emit32(0xD4001001)
        } else {
            mut r = gen_mov(8, 16)
            mut r = emit32(0xD4000001)
        }
        if is_println == 1 {
            mut r = emit32(0xD10043FF)
            mut r = gen_movz(0, 10)
            mut r = gen_strb_reg(0, 31, 31)
            mut r = gen_movz(0, 1)
            mut r = emit32(0x910003E1)
            mut r = gen_movz(2, 1)
            mut r = gen_movz(16, TSC_WRITE)
            if g_target_os == 0 {
                mut r = emit32(0xD4001001)
            } else {
                mut r = gen_mov(8, 16)
                mut r = emit32(0xD4000001)
            }
            mut r = emit32(0x910043FF)
        }
        mut r = emit32(0x910083FF)
    }
    mut r = 0
}

fn gen_print_builtin(arg_count: i64) (r: i64)
{
    let is_println: i64 = 0
    mut is_println = 0
    if __byte_load(g_call_name, 5) == 108 { mut is_println = 1 }
    if g_target_isa == 1 {
        mut r = x86_pop_reg(0)
        mut r = x86_mov_reg(6, 0)
        mut r = x86_mov_imm(2, 0)
        let loop_pos: i64 = 0
        mut loop_pos = g_code_pos
        mut r = x86_load8_reg(1, 6, 0)
        mut r = x86_or_reg(1, 1)
        let skip_pos: i64 = 0
        mut skip_pos = g_code_pos
        mut r = x86_jcc(4, 0)
        mut r = x86_add_imm(6, 1)
        mut r = x86_add_imm(2, 1)
        mut r = gen_b(loop_pos - g_code_pos)
        mut r = patch_bcond(skip_pos, g_code_pos - skip_pos)
        mut r = x86_sub_reg(6, 2)
        mut r = x86_mov_imm(0, TSC_WRITE)
        mut r = x86_mov_imm(7, 1)
        mut r = x86_syscall()
        if is_println == 1 {
            mut r = x86_sub_imm(4, 16)
            mut r = x86_mov_imm(0, 10)
            mut r = x86_store8_reg(0, 4, 0)
            mut r = x86_mov_imm(0, TSC_WRITE)
            mut r = x86_mov_imm(7, 1)
            mut r = x86_mov_reg(6, 4)
            mut r = x86_mov_imm(2, 1)
            mut r = x86_syscall()
            mut r = x86_add_imm(4, 16)
        }
    } else {
        mut r = emit32(0xF84107E0)
        mut r = gen_movz(1, 0)
        let loop_pos2: i64 = 0
        mut loop_pos2 = g_code_pos
        mut r = gen_ldrb_reg(2, 0, 1)
        mut r = gen_cmp(2, 31)
        let skip_pos2: i64 = 0
        mut skip_pos2 = g_code_pos
        mut r = gen_bcond(0, 0)
        mut r = gen_add_imm(1, 1, 1)
        mut r = gen_b(loop_pos2 - g_code_pos)
        mut r = patch_bcond(skip_pos2, g_code_pos - skip_pos2)
        mut r = gen_mov(3, 0)
        mut r = gen_mov(4, 1)
        mut r = gen_movz(0, 1)
        mut r = gen_mov(1, 3)
        mut r = gen_mov(2, 4)
        mut r = gen_movz(16, TSC_WRITE)
        if g_target_os == 0 {
            mut r = emit32(0xD4001001)
        } else {
            mut r = gen_mov(8, 16)
            mut r = emit32(0xD4000001)
        }
        if is_println == 1 {
            mut r = emit32(0xD10043FF)
            mut r = gen_movz(0, 10)
            mut r = gen_strb_reg(0, 31, 31)
            mut r = gen_movz(0, 1)
            mut r = emit32(0x910003E1)
            mut r = gen_movz(2, 1)
            mut r = gen_movz(16, TSC_WRITE)
            if g_target_os == 0 {
                mut r = emit32(0xD4001001)
            } else {
                mut r = gen_mov(8, 16)
                mut r = emit32(0xD4000001)
            }
            mut r = emit32(0x910043FF)
        }
    }
    mut r = 0
}

// __malloc(size): allocate memory via mmap syscall.
// Pops size to X0, then sets up mmap args:
//   X0=0 (addr), X1=size (copied from X0), X2=3 (PROT_RW),
//   X3=4098 (MAP_PRIVATE|MAP_ANON), X4=-1 (fd), X5=0 (offset).
// The gen_mov(1, 0) is critical: mmap expects size in X1, but we
// popped it into X0, so we must copy X0->X1 before zeroing X0.
// Syscall: X16=197 (macOS) or X8=222 (Linux).
fn gen_malloc_builtin(arg_count: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = gen_pop_x0()
        mut r = gen_caller_save()
        mut r = x86_load_reg(7, 4, 16)
        mut r = x86_mov_reg(6, 7)
        mut r = x86_mov_imm(7, 0)
        mut r = x86_mov_imm(2, 3)
        mut r = x86_mov_imm(10, T_MAP_FLAGS)
        mut r = x86_mov_imm(8, 0 - 1)
        mut r = x86_mov_imm(9, 0)
        mut r = x86_mov_imm(0, TSC_MMAP)
        mut r = x86_syscall()
    } else {
        mut r = gen_pop_x0()
        mut r = gen_mov(1, 0)
        mut r = gen_movz(0, 0)
        mut r = gen_movz(2, 3)
        mut r = gen_movz(3, T_MAP_FLAGS)
        mut r = emit32(0x92800004)
        mut r = gen_movz(5, 0)
        if g_target_os == 0 {
            mut r = gen_movz(16, TSC_MMAP)
            mut r = emit32(0xD4001001)
        } else {
            mut r = gen_movz(8, 222)
            mut r = emit32(0xD4000001)
        }
    }
}

fn gen_alloca_builtin(arg_count: i64) (r: i64)
{
    if g_target_isa == 1 {
        mut r = gen_pop_x0()
        mut r = x86_add_imm(x86_reg(0), 15)
        mut r = x86_mov_imm(11, 0 - 16)
        mut r = x86_and_reg(x86_reg(0), 11)
        mut r = x86_sub_reg(4, 4, x86_reg(0))
        mut r = x86_mov_reg(x86_reg(0), 4)
    } else {
        mut r = gen_pop_x0()
        mut r = gen_add_imm(0, 0, 15)
        mut r = emit32(0x92800001)
        mut r = emit32(0x8A010000)
        mut r = emit32(0xCB0003FF)
        mut r = emit32(0x910003E0)
    }
}

// __str_len(s): compute string length by scanning for null byte.
// Pops string ptr to X0, then runs a tight loop:
//   X1 = 0 (counter)
//   loop: LDRB W2, [X0]  ; load current byte
//         CBZ W2, done   ; if 0, string ends
//         ADD X0, X0, #1 ; advance pointer
//         ADD X1, X1, #1 ; increment counter
//         B loop
//   done: MOV X0, X1     ; return length
fn gen_str_len_builtin() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_pop_reg(0)
        mut r = x86_mov_imm(1, 0)
        let loop_pos: i64 = 0
        mut loop_pos = g_code_pos
        mut r = x86_load8_reg(2, 0, 0)
        mut r = x86_cmp_reg(2, 2)
        let skip_pos: i64 = 0
        mut skip_pos = g_code_pos
        mut r = x86_jcc(4, 0)
        mut r = x86_add_imm(x86_reg(0), 1)
        mut r = x86_add_imm(1, 1)
mut r = gen_b(loop_pos - g_code_pos)
        mut r = patch_bcond(skip_pos, g_code_pos - skip_pos)
        mut r = x86_mov_reg(0, 1)
    } else {
        mut r = emit32(0xF84107E0)
        mut r = gen_movz(1, 0)
        let loop_pos2: i64 = 0
        mut loop_pos2 = g_code_pos
        mut r = emit32(0x38400002)
        mut r = emit32(0x34000082)
        mut r = gen_add_imm(0, 0, 1)
        mut r = gen_add_imm(1, 1, 1)
        mut r = gen_b(loop_pos2 - g_code_pos)
        mut r = gen_mov(0, 1)
    }
    mut r = 0
}

fn gen_call_builtin(name: i64, arg_count: i64) (r: i64)
{
    let b0: i64 = 0
    mut b0 = __byte_load(g_call_name, 0)
    if b0 == 112 {
        mut r = 0
    } else {
        let b2: i64 = 0
        mut b2 = __byte_load(g_call_name, 2)
        if b2 == 98 {
            if __byte_load(g_call_name, 3) == 121 {
                mut r = gen_byte_builtin()
            }
        } else {
            if b2 == 109 {
                if __byte_load(g_call_name, 3) == 101 {
                    mut r = gen_mem_builtin()
                } else {
                    if __byte_load(g_call_name, 3) == 97 {
                        mut r = gen_malloc_builtin(arg_count)
                    }
                }
            } else {
                if b2 == 97 {
                    if __byte_load(g_call_name, 3) == 108 {
                        mut r = gen_alloca_builtin(arg_count)
                    }
                } else {
                    if b2 == 115 {
                        if __byte_load(g_call_name, 3) == 116 {
                            if __byte_load(g_call_name, 6) == 108 {
                                mut r = gen_str_len_builtin()
                            } else {
                                mut r = gen_str_eq_inline()
                            }
                        } else {
                            if __byte_load(g_call_name, 3) == 121 {
                                mut r = gen_syscall_builtin(arg_count)
                            }
                        }
                    }
                }
            }
        }
    }
    mut r = 0
}

fn gen_caller_save2() (r: i64)
{
    if g_target_isa == 1 {
        mut r = 0
    } else {
        mut r = emit32(2835687400)
        mut r = emit32(2835754986)
        mut r = emit32(2835822572)
        mut r = emit32(2835890158)
    }
    mut r = 0
}

// Save caller-saved registers before a syscall or function call.
// Allocates 160 bytes on stack (SUB SP, SP, #0xA0) then stores
// X0-X7 via STP pairs, plus X8-X15 via gen_caller_save2().
// This preserves argument and temporary registers that may hold
// live values across the call. X16-X17 (IP0/IP1) are not saved
// as they are scratch/PLT registers. X19+ are callee-saved.
fn gen_caller_save() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_sub_imm(4, 160)
        mut r = x86_store_reg(0, 4, 16)
        mut r = x86_store_reg(1, 4, 24)
        mut r = x86_store_reg(2, 4, 32)
        mut r = x86_store_reg(3, 4, 40)
        mut r = x86_store_reg(6, 4, 48)
        mut r = x86_store_reg(7, 4, 56)
        mut r = x86_store_reg(8, 4, 64)
        mut r = x86_store_reg(9, 4, 72)
    } else {
        mut r = emit32(0xD10283FF)
        mut r = emit32(2835417056)
        mut r = emit32(2835484642)
        mut r = emit32(2835552228)
        mut r = emit32(2835619814)
        mut r = gen_caller_save2()
    }
    mut r = 0
}

fn gen_save_retval() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_store_reg(0, 4, 8)
    } else {
        mut r = emit32(0xF90007E0)
    }
    mut r = 0
}

fn gen_caller_restore2() (r: i64)
{
    if g_target_isa == 1 {
        mut r = 0
    } else {
        mut r = emit32(2839881704)
        mut r = emit32(2839949290)
        mut r = emit32(2840016876)
        mut r = emit32(2840084462)
    }
    mut r = 0
}

// Restore caller-saved registers after a syscall or function call.
// Loads X0-X7 via LDP pairs from the 160-byte save area, then
// loads X8-X15 via gen_caller_restore2(). Mirror of gen_caller_save.
// Note: X0 is typically overwritten with the return value afterwards
// via gen_load_retval().
fn gen_caller_restore() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_load_reg(0, 4, 16)
        mut r = x86_load_reg(1, 4, 24)
        mut r = x86_load_reg(2, 4, 32)
        mut r = x86_load_reg(3, 4, 40)
        mut r = x86_load_reg(6, 4, 48)
        mut r = x86_load_reg(7, 4, 56)
        mut r = x86_load_reg(8, 4, 64)
        mut r = x86_load_reg(9, 4, 72)
    } else {
        mut r = emit32(2839611360)
        mut r = emit32(2839678946)
        mut r = emit32(2839746532)
        mut r = emit32(2839814118)
        mut r = gen_caller_restore2()
    }
    mut r = 0
}
fn gen_add_sp() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_add_imm(4, 160)
    } else {
        mut r = emit32(0x910283FF)
    }
    mut r = 0
}

fn gen_load_retval() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_load_reg(0, 4, 8)
    } else {
        mut r = emit32(0xF94007E0)
    }
    mut r = 0
}

fn gen_extern_call() (r: i64)
{
    __mem_store(g_patch_pos + g_patch_count * 8, g_code_pos)
    __mem_store(g_patch_name + g_patch_count * 8, g_call_name)
    mut g_patch_count = g_patch_count + 1
    if g_target_isa == 1 {
        mut r = emit_byte(0xE8)
        mut r = emit_byte(0)
        mut r = emit_byte(0)
        mut r = emit_byte(0)
        mut r = emit_byte(0)
    } else {
        mut r = gen_bl(0)
    }
}

fn gen_direct_call(fn_off: i64) (r: i64)
{
    let rel: i64 = 0
    mut rel = fn_off - g_code_pos
    mut r = gen_bl(rel)
}

fn gen_call_finish() (r: i64)
{
    mut r = gen_save_retval()
    mut r = gen_caller_restore()
    mut r = gen_load_retval()
    mut r = gen_add_sp()
    mut r = 0
}

fn gen_call_normal2(name: i64, arg_count: i64) (r: i64)
{
    let fn_off: i64 = 0
    mut r = gen_pop_args(arg_count)
    mut r = gen_caller_save()
    mut fn_off = fn_lookup(name)
    if fn_off > 0 {
        mut r = gen_direct_call(fn_off)
    } else {
        mut g_call_name = name
        mut r = gen_extern_call()
    }
    mut r = gen_call_finish()
}

// Allocate a 16-byte enum value on the heap via mmap.
// 1. Save caller-saved regs (X0-X5 may hold live values).
// 2. mmap(0, 16, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0).
// 3. Save return value, restore caller-saved regs, reload return value,
//    and deallocate the 160-byte save area (gen_add_sp).
// The save/restore preserves live registers since mmap clobbers X0-X5.
fn gen_enum_alloc() (r: i64)
{
    if g_target_isa == 1 {
        mut r = gen_caller_save()
        mut r = x86_mov_imm(7, 0)
        mut r = x86_mov_imm(6, 16)
        mut r = x86_mov_imm(2, 3)
        mut r = x86_mov_imm(10, T_MAP_FLAGS)
        mut r = x86_mov_imm(8, 0 - 1)
        mut r = x86_mov_imm(9, 0)
        mut r = x86_mov_imm(0, TSC_MMAP)
        mut r = x86_syscall()
        mut r = gen_save_retval()
        mut r = gen_caller_restore()
        mut r = gen_load_retval()
        mut r = gen_add_sp()
    } else {
        mut r = gen_caller_save()
        mut r = gen_movz(0, 16)
        mut r = gen_mov(1, 0)
        mut r = gen_movz(0, 0)
        mut r = gen_movz(2, 3)
        mut r = gen_movz(3, T_MAP_FLAGS)
        mut r = emit32(0x92800004)
        mut r = gen_movz(5, 0)
        if g_target_os == 0 {
            mut r = gen_movz(16, TSC_MMAP)
            mut r = emit32(0xD4001001)
        } else {
            mut r = gen_movz(8, 222)
            mut r = emit32(0xD4000001)
        }
        mut r = gen_save_retval()
        mut r = gen_caller_restore()
        mut r = gen_load_retval()
        mut r = gen_add_sp()
    }
}

// Generate code for an enum constructor call (e.g. Some(value)).
// Layout: [tag at +0] [arg at +8], 16 bytes total.
// 1. If variant has an arg: pop arg to X0, push it back (survive mmap).
// 2. gen_enum_alloc() -> X0 = pointer to 16-byte block.
// 3. If has_arg: pop saved arg to X1, STR X1 at [X0, #+8].
// 4. Load tag into X1, STR X1 at [X0, #+0].
fn gen_enum_constructor(name: i64, arg_count: i64) (r: i64)
{
    let tag: i64 = 0
    let has_arg: i64 = 0
    mut tag = enum_lookup(name)
    mut has_arg = enum_has_arg_lookup(name)
    if has_arg == 1 {
        mut r = gen_pop_x0()
        mut r = gen_push()
    }
    mut r = gen_enum_alloc()
    if has_arg == 1 {
        mut r = gen_pop_x1()
        mut r = gen_stur(1, 0, 8)
    } else {
        if arg_count > 0 {
            mut r = gen_pop_discard()
        }
    }
    mut r = gen_movz(1, tag)
    mut r = gen_stur(1, 0, 0)
    mut r = 0
}

fn gen_call(name: i64, first_arg: i64) (r: i64)
{
    let arg_count: i64 = 0
    let saved_name: i64 = 0
    let arg_kind: i64 = 0
    let arg_nd: i64 = 0
    mut saved_name = name
    mut g_call_name = name
    mut arg_count = gen_eval_args(first_arg)
    mut g_call_name = saved_name

    if enum_lookup(g_call_name) >= 0 {
        mut r = gen_enum_constructor(g_call_name, arg_count)
    } else {
    if is_builtin_name(g_call_name) == 1 {
        if __byte_load(g_call_name, 0) == 112 {
            // print/println: check if argument is string (k=2) or integer
            mut arg_kind = 0
            mut arg_nd = first_arg
            if arg_nd > 0 {
                mut arg_kind = __mem_load(g_ast_kind + arg_nd * 8)
                if arg_kind == 20 {
                    mut arg_nd = __mem_load(g_ast_a + arg_nd * 8)
                    mut arg_kind = __mem_load(g_ast_kind + arg_nd * 8)
                }
            }
            if arg_kind == 2 {
                mut r = gen_print_builtin(arg_count)
            } else {
                let is_pln: i64 = 0
                mut is_pln = 0
                if __byte_load(g_call_name, 5) == 108 { mut is_pln = 1 }
                mut r = gen_print_int_builtin(is_pln)
            }
        } else {
            mut r = gen_call_builtin(g_call_name, arg_count)
        }
    } else {
        mut r = gen_call_normal2(g_call_name, arg_count)
    }
    }
    mut r = 0
}
fn gen_pop_x0() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_pop_reg(0)
    } else {
        mut r = emit32(0xF84107E0)
    }
}

// Pop to X2
fn gen_pop_x2() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_pop_reg(x86_reg(2))
    } else {
        mut r = emit32(0xF84107E2)
    }
}

// Pop to X3
fn gen_pop_x3() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_pop_reg(x86_reg(3))
    } else {
        mut r = emit32(0xF84107E3)
    }
}

// Pop to X4
fn gen_pop_x4() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_pop_reg(x86_reg(4))
    } else {
        mut r = emit32(0xF84107E4)
    }
}

// Pop to X5
fn gen_pop_x5() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_pop_reg(x86_reg(5))
    } else {
        mut r = emit32(0xF84107E5)
    }
}

// Pop and discard (to XZR)
fn gen_pop_discard() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_add_imm(4, 8)
    } else {
        mut r = emit32(0xF84107FF)
    }
}

// === Statement code generator ===

fn gen_let_stmt(v: i64, b: i64, c: i64) (r: i64)
{
    let off: i64 = 0
    let goff: i64 = 0
    mut goff = glob_lookup(v)
    if goff >= 0 {
        if b > 0 {
            mut r = gen_expr(b)
        } else {
            mut r = gen_movz(0, 0)
        }
        mut r = gen_glob_store(goff)
    } else {
        mut off = var_lookup(v)
        if off < 0 {
            mut off = g_var_count
            mut r = var_add(v, off)
        }
        if b > 0 {
            mut r = gen_expr(b)
        } else {
            mut r = gen_movz(0, 0)
        }
        mut r = gen_stur(0, 29, 0 - (off + 1) * 8)
    }
    mut r = 0
}

fn gen_assign_stmt(a: i64, b: i64) (r: i64)
{
    let off: i64 = 0
    mut r = gen_expr(b)
    mut off = var_lookup(__mem_load(g_ast_val + a * 8))
    if off >= 0 {
        mut r = gen_stur(0, 29, 0 - (off + 1) * 8)
    } else {
        let goff: i64 = 0
        mut goff = glob_lookup(__mem_load(g_ast_val + a * 8))
        if goff >= 0 {
            mut r = gen_glob_store(goff)
        }
    }
    mut r = 0
}

fn gen_return_stmt(a: i64) (r: i64)
{
    if a > 0 {
        mut r = gen_expr(a)
    } else {
        mut r = gen_movz(0, 0)
    }
    mut r = gen_epilogue()
    mut r = 0
}

// Generate code for a match expression.
// 1. Evaluate scrutinee -> X0, push to stack.
// 2. For each case:
//    a. Load scrutinee ptr from stack, deref tag at [ptr, #+0].
//    b. Compare tag with expected enum variant tag; B.NE to next case.
//    c. If variant has arg and bind var: load arg from [ptr, #+8],
//       store in local variable slot.
//    d. Generate case body code.
//    e. B to done label (patched after all cases).
// 3. Patch all done-branches to point past the match.
// 4. Pop and discard the scrutinee.
fn gen_match(nd: i64) (r: i64)
{
    let scrut: i64 = 0
    let cases: i64 = 0
    let c: i64 = 0
    let pat: i64 = 0
    let bind: i64 = 0
    let body: i64 = 0
    let tag: i64 = 0
    let has_arg: i64 = 0
    let done_patches: i64 = 0
    let patch_count: i64 = 0
    let i: i64 = 0
    mut done_patches = malloc(256)
    mut patch_count = 0
    mut scrut = ast_field(nd, g_ast_a)
    mut r = gen_expr(scrut)
    mut r = gen_push()
    mut cases = ast_field(nd, g_ast_b)
    while cases > 0 {
        let k: i64 = 0
        mut k = __mem_load(g_ast_kind + cases * 8)
        if k == 20 {
            mut c = __mem_load(g_ast_a + cases * 8)
            mut cases = __mem_load(g_ast_b + cases * 8)
        } else {
            mut c = cases
            mut cases = 0
        }
        if c > 0 {
            mut pat = ast_field(c, g_ast_val)
            mut bind = ast_field(c, g_ast_a)
            mut body = ast_field(c, g_ast_b)
            mut tag = enum_lookup(pat)
            mut has_arg = enum_has_arg_lookup(pat)
            mut r = gen_ldr(1, 31, 0)
            mut r = gen_ldr(1, 1, 0)
            if tag >= 0 {
                mut r = gen_movz(2, tag)
                mut r = gen_cmp(1, 2)
                let neq_pos: i64 = 0
                mut neq_pos = g_code_pos
                mut r = gen_bcond(1, 0)
                if has_arg == 1 {
                    if bind > 0 {
                        mut r = gen_ldr(2, 31, 0)
                        mut r = gen_ldr(0, 2, 1)
                        let off: i64 = 0
                        mut off = var_lookup(bind)
                        if off < 0 {
                            mut off = g_var_count
                            mut r = var_add(bind, off)
                        }
                        mut r = gen_stur(0, 29, 0 - (off + 1) * 8)
                    }
                }
                mut r = gen_stmt(body)
                __mem_store(done_patches + patch_count * 8, g_code_pos)
                mut patch_count = patch_count + 1
                mut r = gen_b(0)
                mut r = patch_bcond(neq_pos, g_code_pos - neq_pos)
            }
        }
    }
    mut i = 0
    while i < patch_count {
        let ppos: i64 = 0
        mut ppos = __mem_load(done_patches + i * 8)
        mut r = patch_b(ppos, g_code_pos - ppos)
        mut i = i + 1
    }
    mut r = gen_pop_discard()
    mut r = 0
}

fn gen_stmt2(nd: i64, k: i64) (r: i64)
{
    if k == 17 {
        mut r = gen_match(nd)
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
                if k == 15 {
                    mut g_break_pos = g_code_pos
                    mut r = gen_b(0)
                } else {
                    if k == 16 {
                        mut r = gen_b(g_loop_start - g_code_pos)
                    } else {
                        if k == 4 {
                            mut r = gen_call(ast_field(nd, g_ast_val), ast_field(nd, g_ast_a))
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

fn gen_stmt(nd: i64) (r: i64)
{
    let k: i64 = 0
    mut k = ast_field(nd, g_ast_kind)
    if k == 10 {
        mut r = gen_let_stmt(ast_field(nd, g_ast_val), ast_field(nd, g_ast_b), ast_field(nd, g_ast_c))
    } else {
        if k == 9 {
            mut r = gen_assign_stmt(ast_field(nd, g_ast_a), ast_field(nd, g_ast_b))
        } else {
            if k == 14 {
                mut r = gen_return_stmt(ast_field(nd, g_ast_a))
            } else {
                mut r = gen_stmt2(nd, k)
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

fn gen_else_patch(beq_pos: i64) (r: i64)
{
    mut r = patch_bcond(beq_pos, g_code_pos - beq_pos)
    mut r = 0
}

fn gen_end_patch(bend_pos: i64) (r: i64)
{
    mut r = patch_b(bend_pos, g_code_pos - bend_pos)
    mut r = 0
}

fn gen_if_with_else(beq_pos: i64, else_blk: i64) (r: i64)
{
    let bend_pos: i64 = 0
    mut bend_pos = g_code_pos
    mut r = gen_b(0)
    mut r = gen_else_patch(beq_pos)
    mut r = gen_block(else_blk)
    mut r = gen_end_patch(bend_pos)
    mut r = 0
}

fn gen_if_else(beq_pos: i64, else_blk: i64) (r: i64)
{
    if else_blk > 0 {
        mut r = gen_if_with_else(beq_pos, else_blk)
    } else {
        mut r = gen_else_patch(beq_pos)
    }
    mut r = 0
}

fn gen_if_cond(nd: i64) (r: i64)
{
    mut r = gen_expr(ast_field(nd, g_ast_a))
    mut r = gen_cmp(0, 31)
    mut r = 0
}

fn gen_if(nd: i64) (r: i64)
{
    let beq_pos: i64 = 0
    mut r = gen_if_cond(nd)
    mut beq_pos = g_code_pos
    mut r = gen_bcond(0, 8)
    mut r = gen_block(ast_field(nd, g_ast_b))
    mut r = gen_if_else(beq_pos, ast_field(nd, g_ast_c))
    mut r = 0
}

fn gen_while_back(loop_start: i64) (r: i64)
{
    mut r = gen_b(loop_start - g_code_pos)
    mut r = 0
}

fn gen_while_cond(nd: i64) (r: i64)
{
    mut r = gen_expr(ast_field(nd, g_ast_a))
    mut r = gen_cmp(0, 31)
    mut r = 0
}

fn gen_while_body(nd: i64, loop_start: i64) (r: i64)
{
    mut r = gen_block(ast_field(nd, g_ast_b))
    mut r = gen_while_back(loop_start)
    mut r = 0
}

fn gen_while(nd: i64) (r: i64)
{
    let loop_start: i64 = 0
    let beq_pos: i64 = 0
    let saved_break: i64 = 0
    mut loop_start = g_code_pos
    mut g_saved_loop_start = g_loop_start
    mut g_saved_loop_end = g_loop_end
    mut saved_break = g_break_pos
    mut g_loop_start = loop_start
    mut g_break_pos = 0
    mut r = gen_while_cond(nd)
    mut beq_pos = g_code_pos
    mut r = gen_bcond(0, 0)
    mut r = gen_while_body(nd, loop_start)
    mut g_loop_end = g_code_pos
    if g_break_pos > 0 {
        mut r = patch_b(g_break_pos, g_code_pos - g_break_pos)
    }
    mut r = gen_else_patch(beq_pos)
    mut g_loop_start = g_saved_loop_start
    mut g_loop_end = g_saved_loop_end
    mut g_break_pos = saved_break
    mut r = 0
}

// Patch a B.cond instruction at pos with new offset
fn patch_bcond(pos: i64, offset: i64) (r: i64)
{
    if g_target_isa == 1 {
        let rel: i64 = 0
        mut rel = offset - 6
        mut r = __byte_store(g_code, pos + 2, rel & 255)
        mut r = __byte_store(g_code, pos + 3, (rel >> 8) & 255)
        mut r = __byte_store(g_code, pos + 4, (rel >> 16) & 255)
        mut r = __byte_store(g_code, pos + 5, (rel >> 24) & 255)
    } else {
        let off19: i64 = 0
        let old_cond: i64 = 0
        mut off19 = (offset >> 2) & 524287
        mut old_cond = __byte_load(g_code, pos) & 15
        mut r = emit32_at(pos, 0x54000000 | (off19 << 5) | old_cond)
    }
    mut r = 0
}

// Patch a B instruction at pos with new offset
fn patch_b(pos: i64, offset: i64) (r: i64)
{
    if g_target_isa == 1 {
        let rel: i64 = 0
        mut rel = offset - 5
        mut r = __byte_store(g_code, pos + 1, rel & 255)
        mut r = __byte_store(g_code, pos + 2, (rel >> 8) & 255)
        mut r = __byte_store(g_code, pos + 3, (rel >> 16) & 255)
        mut r = __byte_store(g_code, pos + 4, (rel >> 24) & 255)
    } else {
        let off26: i64 = 0
        mut off26 = (offset >> 2) & 67108863
        mut r = emit32_at(pos, 0x14000000 | off26)
    }
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
    mut reg = g_var_count
    mut p = params
    while p > 0 {
        mut pname = extract_name(p)
        if pname > 0 {
            mut r = var_add(pname, reg)
            mut r = gen_stur(reg, 29, 0 - (reg + 1) * 8)
            mut reg = reg + 1
        }
        mut p = extract_next(p)
    }
    mut r = 0
}

fn gen_rets(rets: i64) (r: i64)
{
    let p: i64 = 0
    let pname: i64 = 0
    let off: i64 = 0
    mut p = rets
    while p > 0 {
        mut pname = extract_name(p)
        if pname > 0 {
            mut off = g_var_count
            mut r = var_add(pname, off)
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
            if off >= 0 {
                mut r = gen_ldur(0, 29, 0 - (off + 1) * 8)
            }
        }
    }
    mut r = 0
}
fn ast_field(nd: i64, field: i64) (r: i64)
{
    mut r = __mem_load(field + nd * 8)
}

// Function epilogue: restore frame and return.
// 1. ADD SP, SP, #2048  (deallocate the 2048-byte stack frame).
// 2. LDP X29, X30, [SP], #16  (restore FP and LR, post-index).
// 3. RET.
fn gen_epilogue() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_add_imm(4, 2048)
        mut r = x86_pop_reg(5)
        mut r = x86_ret()
    } else {
        mut r = gen_add_imm(31, 31, 2048)
        mut r = gen_ldp_post(29, 30, 31, 2)
        mut r = gen_ret()
    }
    mut r = 0
}

// Function prologue: set up a 2048-byte stack frame.
// 1. STP X29, X30, [SP, #-16]!  (save FP and LR, pre-index).
// 2. MOV X29, SP  (set frame pointer).
// 3. SUB SP, SP, #2048  (allocate 2048 bytes for locals/spills).
// The 2048-byte frame accommodates up to 256 i64 locals.
fn gen_prologue() (r: i64)
{
    if g_target_isa == 1 {
        mut r = x86_push_reg(5)
        mut r = x86_mov_reg(5, 4)
        mut r = x86_sub_imm(4, 2048)
    } else {
        mut r = gen_stp_pre(29, 30, 31, 65534)
        mut r = gen_add_imm(29, 31, 0)
        mut r = gen_sub_imm(31, 31, 2048)
    }
    mut r = 0
}

fn gen_main_init() (r: i64)
{
    if g_target_isa == 1 {
        // x86-64: mmap syscall with args in RDI/RSI/RDX/R10/R8/R9
        mut r = x86_mov_imm(7, 0)          // RDI = 0 (addr)
        mut r = x86_mov_imm(6, 4096)       // RSI = 4096
        mut r = x86_mov_imm(11, 1)
        mut r = x86_shl_imm(11, 16)        // R11 = 0x10000
        mut r = x86_or_reg(6, 11)          // RSI = 69632
        mut r = x86_mov_imm(2, 3)          // RDX = 3 (PROT_RW)
        mut r = x86_mov_imm(10, T_MAP_FLAGS) // R10 = MAP flags
        mut r = x86_mov_imm(8, 0 - 1)      // R8 = -1 (fd)
        mut r = x86_mov_imm(9, 0)          // R9 = 0 (offset)
        mut r = x86_mov_imm(0, TSC_MMAP)   // RAX = mmap syscall #
        mut r = x86_syscall()
        mut r = x86_mov_reg(3, 0)          // RBX = RAX (global base ptr)
    } else {
        mut r = gen_movz(0, 0)
        mut r = gen_movz(1, 4096)
        mut r = gen_movk(1, 1, 16)
        mut r = gen_movz(2, 3)
        mut r = gen_movz(3, T_MAP_FLAGS)
        mut r = emit32(0x92800004)
        mut r = gen_movz(5, 0)
        if g_target_os == 0 {
            mut r = gen_movz(16, TSC_MMAP)
            mut r = emit32(0xD4001001)
        } else {
            mut r = gen_movz(8, 222)
            mut r = emit32(0xD4000001)
        }
        mut r = gen_mov(19, 0)
    }
    mut r = 0
}


fn gen_glob_init() (r: i64)
{
    let i: i64 = 0
    mut i = 0
    while i < g_glob_count {
        let val: i64 = 0
        mut val = __mem_load(g_glob_val + i * 8)
        if val != 0 {
            mut r = gen_movz(0, val & 65535)
            if (val >> 16) != 0 {
                mut r = gen_movk(0, (val >> 16) & 65535, 16)
            }
            if (val >> 32) != 0 {
                mut r = gen_movk(0, (val >> 32) & 65535, 32)
            }
            if (val >> 48) != 0 {
                mut r = gen_movk(0, (val >> 48) & 65535, 48)
            }
            mut r = gen_str(0, 19, i)
        }
        mut i = i + 1
    }
    mut r = 0
}

fn gen_str_const_init() (r: i64)
{
    let i: i64 = 0
    mut i = 0
    while i < g_str_const_count {
        let soff: i64 = 0
        let saddr: i64 = 0
        mut soff = g_glob_count + i
        mut saddr = __mem_load(g_str_const + i * 8)
        mut r = gen_movz(0, saddr & 65535)
        mut r = gen_movk(0, (saddr >> 16) & 65535, 16)
        mut r = gen_movk(0, (saddr >> 32) & 65535, 32)
        mut r = gen_str(0, 19, soff)
        mut i = i + 1
    }
    mut r = 0
}

fn gen_func_body(name: i64, params: i64, rets: i64, body: i64, is_main: i64) (r: i64)
{
    let is_main_fn: i64 = 0
    mut r = fn_add(name, g_code_pos)
    mut g_var_count = 0
    mut r = gen_prologue()
    mut r = gen_params(params)
    mut r = gen_rets(rets)
    mut is_main_fn = my_str_eq(name, "main")
    if is_main_fn == 1 {
            mut r = gen_main_init()
            mut r = gen_glob_init()
        }
    mut r = gen_block(body)
    mut r = gen_retval(rets)
    if is_main_fn == 1 {
        if g_exec_elf == 1 {
            if g_target_isa == 1 {
                mut r = x86_push_reg(0)
                mut r = x86_pop_reg(7)
                if g_target_os == 1 {
                    mut r = x86_mov_imm(0, 60)
                } else {
                    mut r = x86_mov_imm(0, 1)
                }
                mut r = x86_syscall()
            } else {
                if g_target_os == 1 {
                    mut r = gen_movz(8, 93)
                    mut r = emit32(0xD4000001)
                } else {
                    mut r = gen_movz(16, 1)
                    mut r = emit32(0xD4001001)
                }
            }
        }
    }
    mut r = gen_epilogue()
    mut r = 0
}

fn gen_func(nd: i64) (r: i64)
{
    let name: i64 = 0
    let params: i64 = 0
    let rets: i64 = 0
    let body: i64 = 0
    mut name = ast_field(nd, g_ast_val)
    mut params = ast_field(nd, g_ast_a)
    mut rets = ast_field(nd, g_ast_b)
    mut body = ast_field(nd, g_ast_c)
    mut r = gen_func_body(name, params, rets, body, 0)
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
    mut r = write32(fp, 4277009103)
    mut r = write32(fp, 16777228)
    mut r = write32(fp, 0)
    mut r = write32(fp, 1)
    mut r = write32(fp, ncmds)
    mut r = write32(fp, sizeofcmds)
    mut r = write32(fp, 0)
    mut r = write32(fp, 0)
    mut r = 0
}

fn write_segment2(fp: i64, total: i64, text_off: i64) (r: i64)
{
    mut r = write64(fp, total)
    mut r = write64(fp, text_off)
    mut r = write64(fp, total)
    mut r = write32(fp, 7)
    mut r = write32(fp, 7)
    mut r = write32(fp, 1)
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
    mut r = write_segment2(fp, total, text_off)
    mut r = 0
}

fn write_section3(fp: i64, reloc_off: i64, nreloc: i64) (r: i64)
{
    mut r = write32(fp, reloc_off)
    mut r = write32(fp, nreloc)
    mut r = write32(fp, 0x80000400)
    mut r = write32(fp, 0)
    mut r = write32(fp, 0)
    mut r = write32(fp, 0)
    mut r = 0
}

fn write_section2(fp: i64, text_off: i64, code_size: i64, reloc_off: i64, nreloc: i64) (r: i64)
{
    mut r = write_str(fp, "__text")
    mut r = write_str(fp, "__TEXT")
    mut r = write64(fp, 0)
    mut r = write64(fp, code_size)
    mut r = write32(fp, text_off)
    mut r = write32(fp, 4)
    mut r = write_section3(fp, reloc_off, nreloc)
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
fn write_ext_nlist(fp: i64) (r: i64)
{
    let i: i64 = 0
    let stroff: i64 = 0
    let pname: i64 = 0
    let len: i64 = 0
    mut stroff = 17
    mut i = 0
    while i < g_ext_count {
        mut r = write32(fp, stroff)
        mut r = write_byte(fp, 0x01)
        mut r = write_byte(fp, 0)
        mut r = write16(fp, 0)
        mut r = write64(fp, 0)
        mut pname = __mem_load(g_ext_name + i * 8)
        mut len = 0
        if pname > 0 {
            while __byte_load(pname + len, 0) != 0 {
                mut len = len + 1
            }
        }
        mut stroff = stroff + len + 2
        mut i = i + 1
    }
    mut r = 0
}

fn write_ext_names(fp: i64) (r: i64)
{
    let i: i64 = 0
    let j: i64 = 0
    let pname: i64 = 0
    mut i = 0
    while i < g_ext_count {
        mut pname = __mem_load(g_ext_name + i * 8)
        if pname > 0 {
            mut r = write_byte(fp, 95)
            mut j = 0
            while __byte_load(pname + j, 0) != 0 {
                mut r = write_byte(fp, __byte_load(pname + j, 0))
                mut j = j + 1
            }
            mut r = write_byte(fp, 0)
        }
        mut i = i + 1
    }
    mut r = 0
}

fn count_ext_str() (r: i64)
{
    let total: i64 = 0
    let i: i64 = 0
    let pname: i64 = 0
    let len: i64 = 0
    mut total = 17
    mut i = 0
    while i < g_ext_count {
        mut pname = __mem_load(g_ext_name + i * 8)
        if pname > 0 {
            mut len = 0
            while __byte_load(pname + len, 0) != 0 {
                mut len = len + 1
            }
            mut total = total + len + 2
        }
        mut i = i + 1
    }
    mut r = total
}

fn patch_str_adrs(code_size: i64) (r: i64)
{
    let i: i64 = 0
    let adr_pos: i64 = 0
    let str_idx: i64 = 0
    let str_off: i64 = 0
    let rel: i64 = 0
    let off_hi: i64 = 0
    let off_lo: i64 = 0
    mut i = 0
    while i < g_adr_patch_count {
        mut adr_pos = __mem_load(g_adr_patch_pos + i * 8)
        mut str_idx = __mem_load(g_adr_patch_idx + i * 8)
        mut str_off = code_size + str_str_off(str_idx)
        if g_target_isa == 1 {
            mut rel = str_off - adr_pos - 7
            mut r = __byte_store(g_code, adr_pos + 3, rel & 255)
            mut r = __byte_store(g_code, adr_pos + 4, (rel >> 8) & 255)
            mut r = __byte_store(g_code, adr_pos + 5, (rel >> 16) & 255)
            mut r = __byte_store(g_code, adr_pos + 6, (rel >> 24) & 255)
        } else {
            mut rel = str_off - adr_pos
            mut off_hi = (rel >> 2) & 262143
            mut off_lo = rel & 3
            mut r = emit32_at(adr_pos, 0x10000000 | (off_lo << 29) | (off_hi << 5))
        }
        mut i = i + 1
    }
    mut r = 0
}

fn str_str_off(idx: i64) (r: i64)
{
    let i: i64 = 0
    let off: i64 = 0
    let s: i64 = 0
    let c: i64 = 0
    let j: i64 = 0
    let real_idx: i64 = 0
    mut real_idx = idx - g_glob_count
    mut i = 0
    while i < real_idx {
        mut s = __mem_load(g_str_const + i * 8)
        mut j = 0
        mut c = __byte_load(s, 0)
        while c != 0 {
            mut off = off + 1
            mut j = j + 1
            mut c = __byte_load(s + j, 0)
        }
        mut off = off + 1
        mut i = i + 1
    }
    mut r = off
}

fn count_str_data() (r: i64)
{
    let i: i64 = 0
    let total: i64 = 0
    let s: i64 = 0
    let c: i64 = 0
    let j: i64 = 0
    mut i = 0
    while i < g_str_const_count {
        mut s = __mem_load(g_str_const + i * 8)
        mut j = 0
        mut c = __byte_load(s, 0)
        while c != 0 {
            mut total = total + 1
            mut j = j + 1
            mut c = __byte_load(s + j, 0)
        }
        mut total = total + 1
        mut i = i + 1
    }
    mut r = total
}

fn write_str_data(fp: i64) (r: i64)
{
    let i: i64 = 0
    let s: i64 = 0
    let j: i64 = 0
    let c: i64 = 0
    let buf: i64 = 0
    mut buf = malloc(1)
    mut i = 0
    while i < g_str_const_count {
        mut s = __mem_load(g_str_const + i * 8)
        mut j = 0
        mut c = __byte_load(s, 0)
        while c != 0 {
            __byte_store(buf, 0, c)
            mut r = fwrite(buf, 1, 1, fp)
            mut j = j + 1
            mut c = __byte_load(s + j, 0)
        }
        __byte_store(buf, 0, 0)
        mut r = fwrite(buf, 1, 1, fp)
        mut i = i + 1
    }
    free(buf)
    mut r = 0
}

fn write_ext_relocs(fp: i64) (r: i64)
{
    let i: i64 = 0
    let pos: i64 = 0
    mut i = 0
    while i < g_ext_count {
        mut pos = __mem_load(g_ext_pos + i * 8)
        if g_target_isa == 1 {
            mut r = __byte_store(g_code, pos, 0xE8)
            mut r = __byte_store(g_code, pos + 1, 0)
            mut r = __byte_store(g_code, pos + 2, 0)
            mut r = __byte_store(g_code, pos + 3, 0)
            mut r = __byte_store(g_code, pos + 4, 0)
        } else {
            mut r = emit32_at(pos, 0x94000000)
        }
        mut r = write32(fp, pos)
        if g_target_isa == 1 {
            mut r = write32(fp, ((i + 1) & 16777215) | (1 << 24) | (2 << 25) | (1 << 27) | (2 << 28))
        } else {
            mut r = write32(fp, ((i + 1) & 16777215) | (1 << 24) | (2 << 25) | (1 << 27) | (2 << 28))
        }
        mut i = i + 1
    }
    mut r = 0
}

fn write_str_raw(fp: i64, s: i64) (r: i64)
{
    let len: i64 = 0
    mut len = 0
    while __byte_load(s, len) != 0 {
        mut len = len + 1
    }
    let buf: i64 = 0
    mut buf = malloc(len)
    let i: i64 = 0
    mut i = 0
    while i < len {
        __byte_store(buf, i, __byte_load(s, i))
        mut i = i + 1
    }
    mut r = fwrite(buf, 1, len, fp)
    free(buf)
}

fn write_elf_exec(path: i64, code_size: i64) (r: i64)
{
    let str_data_size: i64 = 0
    let fp: i64 = 0
    let load_addr: i64 = 0
    let entry: i64 = 0
    let code_off: i64 = 0
    let total_size: i64 = 0
    mut fp = fopen(path, "w")
    if fp == 0 {
        puts("Cannot open output file")
        mut r = 1
    } else {
        mut str_data_size = count_str_data()
        mut load_addr = 4194304
        mut code_off = 120
        mut entry = load_addr + code_off
        mut total_size = code_size + str_data_size
        mut r = patch_str_adrs(code_size)
        mut r = write32(fp, 0x464C457F)
        mut r = write_byte(fp, 2)
        mut r = write_byte(fp, 1)
        mut r = write_byte(fp, 1)
        mut r = write_byte(fp, 0)
        mut r = write32(fp, 0)
        mut r = write32(fp, 0)
        mut r = write16(fp, 2)
        if g_target_isa == 1 {
            mut r = write16(fp, 62)
        } else {
            mut r = write16(fp, 183)
        }
        mut r = write32(fp, 1)
        mut r = write64(fp, entry)
        mut r = write64(fp, 64)
        mut r = write64(fp, 0)
        mut r = write32(fp, 0)
        mut r = write16(fp, 64)
        mut r = write16(fp, 56)
        mut r = write16(fp, 1)
        mut r = write16(fp, 0)
        mut r = write16(fp, 0)
        mut r = write16(fp, 0)
        // PT_LOAD program header
        mut r = write32(fp, 1)
        mut r = write32(fp, 5)
        mut r = write64(fp, code_off)
        mut r = write64(fp, load_addr + code_off)
        mut r = write64(fp, load_addr + code_off)
        mut r = write64(fp, total_size)
        mut r = write64(fp, total_size + 65536)
        mut r = write64(fp, 4096)
        // Write code + string data
        mut r = write_code_bytes(fp, code_size)
        mut r = write_str_data(fp)
        mut r = fclose(fp)
        puts("ELF exec written")
        mut r = 0
    }
}

fn write_elf(path: i64, code_size: i64) (r: i64)
{
    let str_data_size: i64 = 0
    let fp: i64 = 0
    let code_off: i64 = 0
    let reloc_off: i64 = 0
    let sym_off: i64 = 0
    let str_off: i64 = 0
    let shstr_off: i64 = 0
    let i: i64 = 0
    let nreloc: i64 = 0
    let nsyms: i64 = 0
    let str_size: i64 = 0
    let shstr_size: i64 = 0
    mut fp = fopen(path, "w")
    if fp == 0 {
        puts("Cannot open output file")
        mut r = 1
    } else {
        mut str_data_size = count_str_data()
        mut nreloc = g_ext_count
        mut nsyms = 2 + g_ext_count
        mut str_size = 6
        mut i = 0
        while i < g_ext_count {
            let name: i64 = 0
            let len: i64 = 0
            mut name = __mem_load(g_ext_name + i * 8)
            mut len = 0
            while __byte_load(name, len) != 0 {
                mut len = len + 1
            }
            mut str_size = str_size + len + 1
            mut i = i + 1
        }
        mut shstr_size = 33
        // Layout: ELF header(64) + .text + .data(strs) + relocs + symtab + strtab + shstrtab + section headers
        mut code_off = 64
        mut reloc_off = code_off + code_size + str_data_size
        mut sym_off = reloc_off + nreloc * 16
        mut str_off = sym_off + nsyms * 24
        mut shstr_off = str_off + str_size
        let shdr_off: i64 = 0
        mut shdr_off = shstr_off + shstr_size
        // ELF header (64 bytes)
        mut r = write32(fp, 0x464C457F)  // e_ident[0:3] magic
        mut r = write_byte(fp, 2)        // EI_CLASS = ELFCLASS64
        mut r = write_byte(fp, 1)        // EI_DATA = ELFDATA2LSB
        mut r = write_byte(fp, 1)        // EI_VERSION = EV_CURRENT
        mut r = write_byte(fp, 0)        // EI_OSABI = ELFOSABI_NONE
        mut r = write32(fp, 0)           // EI_ABIVERSION + padding
        mut r = write32(fp, 0)           // padding
        mut r = write16(fp, 1)           // e_type = ET_REL
        if g_target_isa == 1 {
            mut r = write16(fp, 62)
        } else {
            mut r = write16(fp, 183)
        }
        mut r = write32(fp, 1)           // e_version = EV_CURRENT
        mut r = write64(fp, 0)           // e_entry
        mut r = write64(fp, 0)           // e_phoff
        mut r = write64(fp, shdr_off)    // e_shoff
        mut r = write32(fp, 0)           // e_flags
        mut r = write16(fp, 64)          // e_ehsize
        mut r = write16(fp, 0)           // e_phentsize
        mut r = write16(fp, 0)           // e_phnum
        mut r = write16(fp, 64)          // e_shentsize
        mut r = write16(fp, 5)           // e_shnum (null, .text, .symtab, .strtab, .shstrtab)
        mut r = write16(fp, 4)           // e_shstrndx = 4
        // Patch string addresses
        mut r = patch_str_adrs(code_size)
        // Write code + string data
        mut r = write_code_bytes(fp, code_size)
        mut r = write_str_data(fp)
        // Relocations (ELF64 Rela entries, 24 bytes each)
        // Actually use ELF64 Rel (16 bytes: r_offset + r_info)
        mut i = 0
        while i < g_ext_count {
            let pos: i64 = 0
            let sym_idx: i64 = 0
            mut pos = __mem_load(g_ext_pos + i * 8)
            mut sym_idx = i + 1
            mut r = write64(fp, pos)
            if g_target_isa == 1 {
                mut r = write64(fp, (sym_idx << 32) | 4)
            } else {
                mut r = write64(fp, (sym_idx << 32) | 274)
            }
            mut i = i + 1
        }
        // Symbol table (ELF64 Sym, 24 bytes each)
        // Null symbol
        mut i = 0
        while i < 24 {
            mut r = write_byte(fp, 0)
            mut i = i + 1
        }
        // main symbol (STB_GLOBAL, STT_FUNC)
        mut r = write32(fp, 1)           // st_name (offset in strtab)
        mut r = write_byte(fp, 18)       // st_info = STB_GLOBAL(1) << 4 | STT_FUNC(2)
        mut r = write_byte(fp, 0)        // st_other
        mut r = write16(fp, 1)           // st_shndx = .text section (1)
        mut r = write64(fp, find_main()) // st_value
        mut r = write64(fp, 0)           // st_size
        // Extern symbols (STB_GLOBAL, STT_NOTYPE, SHN_UNDEF)
        mut i = 0
        while i < g_ext_count {
            let name_off: i64 = 0
            let j: i64 = 0
            let name: i64 = 0
            mut name_off = 6
            mut j = 0
            while j < i {
                mut name = __mem_load(g_ext_name + j * 8)
                let k: i64 = 0
                mut k = 0
                while __byte_load(name, k) != 0 {
                    mut name_off = name_off + 1
                    mut k = k + 1
                }
                mut name_off = name_off + 1
                mut j = j + 1
            }
            mut r = write32(fp, name_off)
            mut r = write_byte(fp, 16)   // st_info = STB_GLOBAL(1) << 4 | STT_NOTYPE(0)
            mut r = write_byte(fp, 0)
            mut r = write16(fp, 0)       // st_shndx = SHN_UNDEF
            mut r = write64(fp, 0)
            mut r = write64(fp, 0)
            mut i = i + 1
        }
        // String table
        mut r = write_byte(fp, 0)
        mut r = write_str_raw(fp, "main")
        mut r = write_byte(fp, 0)
        mut i = 0
        while i < g_ext_count {
            let name: i64 = 0
            mut name = __mem_load(g_ext_name + i * 8)
            mut r = write_str_raw(fp, name)
            mut r = write_byte(fp, 0)
            mut i = i + 1
        }
        // Section header string table
        mut r = write_byte(fp, 0)
        mut r = write_str_raw(fp, ".text")
        mut r = write_byte(fp, 0)
        mut r = write_str_raw(fp, ".symtab")
        mut r = write_byte(fp, 0)
        mut r = write_str_raw(fp, ".strtab")
        mut r = write_byte(fp, 0)
        mut r = write_str_raw(fp, ".shstrtab")
        mut r = write_byte(fp, 0)
        // Section headers (56 bytes each, 5 sections)
        // Section 0: null
        mut i = 0
        while i < 64 {
            mut r = write_byte(fp, 0)
            mut i = i + 1
        }
        // Section 1: .text
        mut r = write32(fp, 1)           // sh_name (offset 1 in shstrtab)
        mut r = write32(fp, 1)           // sh_type = SHT_PROGBITS
        mut r = write64(fp, 6)           // sh_flags = SHF_ALLOC | SHF_EXECINSTR
        mut r = write64(fp, 0)           // sh_addr
        mut r = write64(fp, code_off)    // sh_offset
        mut r = write64(fp, code_size + str_data_size)  // sh_size
        mut r = write32(fp, 0)           // sh_link
        mut r = write32(fp, 0)           // sh_info
        mut r = write64(fp, 16)          // sh_addralign
        mut r = write64(fp, 0)           // sh_entsize
        // Section 2: .symtab
        mut r = write32(fp, 7)           // sh_name (offset 7 in shstrtab)
        mut r = write32(fp, 2)           // sh_type = SHT_SYMTAB
        mut r = write64(fp, 0)           // sh_flags
        mut r = write64(fp, 0)           // sh_addr
        mut r = write64(fp, sym_off)     // sh_offset
        mut r = write64(fp, nsyms * 24)  // sh_size
        mut r = write32(fp, 3)           // sh_link = .strtab section index
        mut r = write32(fp, 1)           // sh_info = index of first non-local symbol
        mut r = write64(fp, 8)           // sh_addralign
        mut r = write64(fp, 24)          // sh_entsize
        // Section 3: .strtab
        mut r = write32(fp, 15)          // sh_name (offset 15 in shstrtab)
        mut r = write32(fp, 3)           // sh_type = SHT_STRTAB
        mut r = write64(fp, 0)           // sh_flags
        mut r = write64(fp, 0)           // sh_addr
        mut r = write64(fp, str_off)     // sh_offset
        mut r = write64(fp, str_size)    // sh_size
        mut r = write32(fp, 0)           // sh_link
        mut r = write32(fp, 0)           // sh_info
        mut r = write64(fp, 1)           // sh_addralign
        mut r = write64(fp, 0)           // sh_entsize
        // Section 4: .shstrtab
        mut r = write32(fp, 23)          // sh_name (offset 23 in shstrtab)
        mut r = write32(fp, 3)           // sh_type = SHT_STRTAB
        mut r = write64(fp, 0)           // sh_flags
        mut r = write64(fp, 0)           // sh_addr
        mut r = write64(fp, shstr_off)   // sh_offset
        mut r = write64(fp, shstr_size)  // sh_size
        mut r = write32(fp, 0)           // sh_link
        mut r = write32(fp, 0)           // sh_info
        mut r = write64(fp, 1)           // sh_addralign
        mut r = write64(fp, 0)           // sh_entsize
        mut r = fclose(fp)
        puts("ELF written")
        mut r = 0
    }
}

fn write_macho(path: i64, code_size: i64) (r: i64)
{
    let fp: i64 = 0
    let text_off: i64 = 0
    let sym_off: i64 = 0
    let str_off: i64 = 0
    let str_size: i64 = 0
    let reloc_off: i64 = 0
    mut fp = fopen(path, "w")
    if fp == 0 {
        puts("Cannot open output file")
        mut r = 1
    } else {
        let str_data_size: i64 = 0
        mut text_off = 32 + 152 + 16 + 24
        mut str_data_size = count_str_data()
        mut reloc_off = text_off + code_size + str_data_size
        mut sym_off = reloc_off + g_ext_count * 8
        mut str_off = sym_off + (1 + g_ext_count) * 16
        mut str_size = count_ext_str()
        mut r = write_header(fp, 3, 152 + 16 + 24)
        mut r = write_segment(fp, text_off, code_size + str_data_size)
        mut r = write_section2(fp, text_off, code_size + str_data_size, reloc_off, g_ext_count)
        mut r = write_version(fp)
        mut r = write_symtab_header2(fp, sym_off, 1 + g_ext_count, str_off, str_size)
        mut r = patch_str_adrs(code_size)
        mut r = write_code_bytes(fp, code_size)
        mut r = write_str_data(fp)
        mut r = write_ext_relocs(fp)
        mut r = write_nlist(fp, 0)
        mut r = write_ext_nlist(fp)
        mut r = write_main_sym(fp)
        mut r = write_ext_names(fp)
        mut r = fclose(fp)
        puts("Mach-O written")
        mut r = 0
    }
}
fn find_main() (r: i64)
{
    mut r = fn_lookup("main")
}

fn write_symtab_header2(fp: i64, sym_off: i64, nsyms: i64, str_off: i64, str_size: i64) (r: i64)
{
    mut r = write32(fp, 2)
    mut r = write32(fp, 24)
    mut r = write32(fp, sym_off)
    mut r = write32(fp, nsyms)
    mut r = write32(fp, str_off)
    mut r = write32(fp, str_size)
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
    mut r = write32(fp, 1)
    mut r = write_byte(fp, 0x0F)
    mut r = write_byte(fp, 1)
    mut r = write16(fp, 0)
    mut r = write64(fp, code_off + find_main())
    mut r = 0
}

fn write_main_sym(fp: i64) (r: i64)
{
    mut r = write_byte(fp, 0)
    mut r = write_str(fp, "_main")
    mut r = 0
}
fn write32_bytes(buf: i64, val: i64) (r: i64)
{
    __byte_store(buf, 0, val & 255)
    __byte_store(buf, 1, (val >> 8) & 255)
    __byte_store(buf, 2, (val >> 16) & 255)
    __byte_store(buf, 3, (val >> 24) & 255)
    mut r = 0
}

fn write32(fp: i64, val: i64) (r: i64)
{
    let buf: i64 = 0
    mut buf = malloc(4)
    mut r = write32_bytes(buf, val)
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

fn init_parser1() (r: i64)
{
    mut g_tok_type = malloc(262144)
    mut g_tok_val = malloc(262144)
    mut g_ast_kind = malloc(524288)
    mut g_ast_val = malloc(524288)
    mut g_glob_name = malloc(4096)
    mut g_glob_off = malloc(4096)
    mut g_glob_val = malloc(4096)
    mut r = 0
}

fn init_parser2() (r: i64)
{
    mut g_ast_a = malloc(524288)
    mut g_ast_b = malloc(524288)
    mut g_ast_c = malloc(524288)
    mut g_str_pool = malloc(524288)
    mut g_enum_name = malloc(4096)
    mut g_enum_tag = malloc(4096)
    mut g_enum_has_arg = malloc(4096)
    mut r = 0
}

fn init_parser() (r: i64)
{
    mut r = init_parser1()
    mut r = init_parser2()
    mut g_ast_count = 0
    mut r = emit_node(0, 0, 0, 0, 0)
    mut g_str_pos = 0
    mut g_tok_count = 0
    mut g_tok_idx = 0
    mut r = 0
}

fn init_codegen1() (r: i64)
{
    mut g_code = malloc(524288)
    mut g_var_name = malloc(4096)
    mut g_var_off = malloc(4096)
    mut g_fn_name = malloc(4096)
    mut g_fn_off = malloc(4096)
    mut r = 0
}

fn init_codegen2() (r: i64)
{
    mut g_patch_pos = malloc(65536)
    mut g_patch_name = malloc(65536)
    mut g_ext_name = malloc(65536)
    mut g_ext_pos = malloc(65536)
    mut g_str_const = malloc(65536)
    mut g_adr_patch_pos = malloc(65536)
    mut g_adr_patch_idx = malloc(65536)
    mut r = 0
}

fn init_codegen() (r: i64)
{
    mut r = init_codegen1()
    mut r = init_codegen2()
    mut g_code_pos = 0
    mut g_var_count = 0
    mut g_fn_count = 0
    mut g_patch_count = 0
    mut g_ext_count = 0
    mut g_str_const_count = 0
    mut g_adr_patch_count = 0
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
        mut g_src = malloc(524288)
        mut g_size = fread(g_src, 1, 524287, fp)
        fclose(fp)
        mut r = init_parser()
        mut r = lex()
        mut g_tok_idx = 0
        mut r = parse_program()
        puts("PARSE DONE")
        mut r = 0
    }
}

fn do_codegen(argv_ptr: i64) (r: i64)
{
    let arg2_ptr: i64 = 0
    mut r = init_codegen()
    mut r = gen_all_funcs()
    puts("GEN DONE")
    mut r = patch_calls()
    mut arg2_ptr = __mem_load(argv_ptr + 16)
    if g_exec_elf == 1 {
        mut r = write_elf_exec(arg2_ptr, g_code_pos)
    } else {
    if g_output_elf == 1 {
        mut r = write_elf(arg2_ptr, g_code_pos)
    } else {
        mut r = write_macho(arg2_ptr, g_code_pos)
    }
    }
    mut r = 0
}

fn run_compiler(argv_ptr: i64) (r: i64)
{
    let arg1_ptr: i64 = 0
    let status: i64 = 0
    let i: i64 = 0
    let arg_ptr: i64 = 0
    mut i = 1
    mut arg1_ptr = 0
    while i < 100 {
        mut arg_ptr = __mem_load(argv_ptr + i * 8)
        if arg_ptr == 0 { mut i = 100 }
        if arg_ptr != 0 {
            if __byte_load(arg_ptr, 0) == 45 {
                if __byte_load(arg_ptr, 2) == 116 {
                    if __byte_load(arg_ptr, 9) == 108 { mut g_target_os = 1 }
                    if __byte_load(arg_ptr, 9) == 102 { mut g_target_os = 2 }
                }
                if __byte_load(arg_ptr, 2) == 120 { mut g_target_isa = 1 }
                if __byte_load(arg_ptr, 2) == 101 {
                    if __byte_load(arg_ptr, 5) == 99 { mut g_exec_elf = 1 }
                    mut g_output_elf = 1
                }
            } else {
                if arg1_ptr == 0 { mut arg1_ptr = arg_ptr }
            }
        }
        mut i = i + 1
    }
    if g_target_os == 1 {
        mut TSC_WRITE = 64
        mut TSC_MMAP = 222
        mut T_MAP_FLAGS = 34
    }
    if g_target_os == 2 {
        mut TSC_WRITE = 4
        mut TSC_MMAP = 477
        mut T_MAP_FLAGS = 4110
    }
    if g_target_isa == 1 {
        if g_target_os == 1 {
            mut TSC_WRITE = 1
            mut TSC_MMAP = 9
        }
    }
    mut status = do_parse(arg1_ptr)
    if status == 0 {
        mut r = do_codegen(argv_ptr)
    } else {
        mut r = status
    }
    mut r = 0
}

fn main(argc: i32, argv: i64) (r: i32)
{
    mut r = run_compiler(argv)
}
