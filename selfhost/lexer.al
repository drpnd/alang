// alang self-hosting compiler: Lexer
// Reads a source file and tokenizes it
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

fn is_alpha(c: i32) (r: i32)
{
    if c >= 65 {
        if c <= 90 {
            mut r = 1
        } else {
            if c >= 97 {
                if c <= 122 {
                    mut r = 1
                } else {
                    mut r = 0
                }
            } else {
                mut r = 0
            }
        }
    } else {
        if c == 95 {
            mut r = 1
        } else {
            mut r = 0
        }
    }
}

fn is_digit(c: i32) (r: i32)
{
    if c >= 48 {
        if c <= 57 {
            mut r = 1
        } else {
            mut r = 0
        }
    } else {
        mut r = 0
    }
}

fn is_space(c: i32) (r: i32)
{
    if c == 32 {
        mut r = 1
    } else {
        if c == 10 {
            mut r = 1
        } else {
            if c == 13 {
                mut r = 1
            } else {
                if c == 9 {
                    mut r = 1
                } else {
                    mut r = 0
                }
            }
        }
    }
}

fn peek_char(offset: i64) (r: i32)
{
    mut r = 0
    let pos: i64 = 0
    mut pos = g_pos + offset
    if pos < g_size {
        mut r = __byte_load(g_src, pos)
    }
}

fn next_char() (r: i32)
{
    mut r = 0
    mut g_pos = g_pos + 1
    if g_pos < g_size {
        mut r = __byte_load(g_src, g_pos)
    }
}

fn skip_whitespace(c: i32) (r: i32)
{
    mut r = c
    while is_space(r) == 1 {
        mut r = next_char()
    }
}

fn skip_line_comment() (r: i32)
{
    let c: i32 = 0
    mut c = next_char()
    while c != 10 {
        if g_pos >= g_size {
            mut c = 10
        } else {
            mut c = next_char()
        }
    }
    mut r = c
}

fn skip_block_comment() (r: i32)
{
    let c: i32 = 0
    mut c = next_char()
    while g_pos < g_size {
        if c == 42 {
            let next: i32 = 0
            mut next = peek_char(1)
            if next == 47 {
                mut g_pos = g_pos + 1
                mut r = next_char()
            } else {
                mut c = next_char()
            }
        } else {
            mut c = next_char()
        }
    }
    mut r = 0
}

fn lex_identifier(c: i32) (r: i32)
{
    puts("IDENT:")
    while is_alpha(c) == 1 {
        putchar(c)
        mut c = next_char()
    }
    while is_digit(c) == 1 {
        putchar(c)
        mut c = next_char()
    }
    putchar(10)
    mut r = c
}

fn lex_number(c: i32) (r: i32)
{
    puts("INT:")
    while is_digit(c) == 1 {
        putchar(c)
        mut c = next_char()
    }
    putchar(10)
    mut r = c
}

fn lex_string() (r: i32)
{
    puts("STR:")
    let c: i32 = 0
    mut c = next_char()
    while c != 34 {
        if g_pos >= g_size {
            mut c = 34
        } else {
            putchar(c)
            mut c = next_char()
        }
    }
    putchar(10)
    mut r = next_char()
}

fn lex_operator(c: i32) (r: i32)
{
    if c == 47 {
        let next: i32 = 0
        mut next = peek_char(1)
        if next == 47 {
            mut g_pos = g_pos + 1
            mut r = skip_line_comment()
        } else {
            if next == 42 {
                mut g_pos = g_pos + 1
                mut r = skip_block_comment()
            } else {
                puts("OP: /")
                mut r = next_char()
            }
        }
    } else {
        if c == 61 {
            let next: i32 = 0
            mut next = peek_char(1)
            if next == 61 {
                puts("OP: ==")
                mut g_pos = g_pos + 1
                mut r = next_char()
            } else {
                puts("OP: =")
                mut r = next_char()
            }
        } else {
            if c == 33 {
                let next: i32 = 0
                mut next = peek_char(1)
                if next == 61 {
                    puts("OP: !=")
                    mut g_pos = g_pos + 1
                    mut r = next_char()
                } else {
                    puts("OP: !")
                    mut r = next_char()
                }
            } else {
                if c == 60 {
                    let next: i32 = 0
                    mut next = peek_char(1)
                    if next == 61 {
                        puts("OP: <=")
                        mut g_pos = g_pos + 1
                        mut r = next_char()
                    } else {
                        puts("OP: <")
                        mut r = next_char()
                    }
                } else {
                    if c == 62 {
                        let next: i32 = 0
                        mut next = peek_char(1)
                        if next == 61 {
                            puts("OP: >=")
                            mut g_pos = g_pos + 1
                            mut r = next_char()
                        } else {
                            puts("OP: >")
                            mut r = next_char()
                        }
                    } else {
                        if c == 45 {
                            let next: i32 = 0
                            mut next = peek_char(1)
                            if next == 62 {
                                puts("OP: ->")
                                mut g_pos = g_pos + 1
                                mut r = next_char()
                            } else {
                                puts("OP: -")
                                mut r = next_char()
                            }
                        } else {
                            puts("OP:")
                            putchar(c)
                            putchar(10)
                            mut r = next_char()
                        }
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
        mut g_pos = 0
        let c: i32 = 0
        mut c = __byte_load(g_src, g_pos)
        mut c = skip_whitespace(c)
        while g_pos < g_size {
            if is_alpha(c) == 1 {
                mut c = lex_identifier(c)
            } else {
                if is_digit(c) == 1 {
                    mut c = lex_number(c)
                } else {
                    if c == 34 {
                        mut c = lex_string()
                    } else {
                        mut c = lex_operator(c)
                    }
                }
            }
            mut c = skip_whitespace(c)
        }
        puts("DONE")
        mut r = 0
    }
}
