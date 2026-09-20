// alang self-hosting compiler: Lexer
// Reads a source file and tokenizes it

extern fn fopen(path: str, mode: str) (fp: i64)
extern fn fclose(fp: i64) (r: i32)
extern fn fread(buf: i64, size: i64, count: i64, fp: i64) (r: i64)
extern fn malloc(size: i64) (ptr: i64)
extern fn free(ptr: i64)
extern fn puts(s: str) (r: i32)
extern fn putchar(c: i32) (r: i32)

let g_size: i64 = 0
let g_pos: i64 = 0
let g_src: i64 = 0

// Check if character is whitespace
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

// Check if character is alphabetic or underscore
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

// Check if character is a digit
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

// Main: read file from argv[1], tokenize, print tokens
fn main(argc: i32, argv: i64) (r: i32)
{
    let argv_ptr: i64 = 0
    mut argv_ptr = argv
    let arg1_ptr: i64 = 0
    mut arg1_ptr = __mem_load(argv_ptr + 8)

    let fp: i64 = 0
    mut fp = fopen(arg1_ptr, "r")
    if fp == 0 {
        puts("Error: cannot open file")
        mut r = 1
    } else {
        mut g_src = malloc(65536)
        mut g_size = fread(g_src, 1, 65535, fp)
        __byte_store(g_src, g_size, 0)
        fclose(fp)

        
        let c: i32 = 0
        mut c = __byte_load(g_src, g_pos)

        while g_pos < g_size {
            // Skip whitespace and comments
            while is_space(c) == 1 {
                mut g_pos = g_pos + 1
                mut c = __byte_load(g_src, g_pos)
            }
            if pos >= size {
            } else {
                // Skip line comments //
                if c == 47 {
                    let next: i32 = 0
                    mut next = __byte_load(g_src, g_pos + 1)
                    if next == 47 {
                        while c != 10 {
                            if pos >= size {
                            } else {
                                mut g_pos = g_pos + 1
                                mut c = __byte_load(g_src, g_pos)
                            }
                        }
                    } else {
                        puts("OP: /")
                        mut g_pos = g_pos + 1
                        mut c = __byte_load(g_src, g_pos)
                    }
                } else {
                    // Identifiers and keywords
                    if is_alpha(c) == 1 {
                        let start: i64 = 0
                        mut start = g_pos
                        while is_alpha(c) == 1 {
                            mut g_pos = g_pos + 1
                            mut c = __byte_load(g_src, g_pos)
                        }
                        // Check if it's a number (alphanumeric)
                        while is_digit(c) == 1 {
                            mut g_pos = g_pos + 1
                            mut c = __byte_load(g_src, g_pos)
                        }
                        puts("IDENT:")
                        let i: i64 = 0
                        mut i = start
                        while i < g_pos {
                            putchar(__byte_load(g_src, i))
                            mut i = i + 1
                        }
                        putchar(10)
                    } else {
                        // Numbers
                        if is_digit(c) == 1 {
                            let start: i64 = 0
                            mut start = g_pos
                            while is_digit(c) == 1 {
                                mut g_pos = g_pos + 1
                                mut c = __byte_load(g_src, g_pos)
                            }
                            puts("INT:")
                            let i: i64 = 0
                            mut i = start
                            while i < g_pos {
                                putchar(__byte_load(g_src, i))
                                mut i = i + 1
                            }
                            putchar(10)
                        } else {
                            // Single-char tokens
                            if c == 40 {
                                puts("LPAREN")
                            } else {
                                if c == 41 {
                                    puts("RPAREN")
                                } else {
                                    if c == 123 {
                                        puts("LBRACE")
                                    } else {
                                        if c == 125 {
                                            puts("RBRACE")
                                        } else {
                                            if c == 59 {
                                                puts("SEMI")
                                            } else {
                                                if c == 58 {
                                                    puts("COLON")
                                                } else {
                                                    if c == 44 {
                                                        puts("COMMA")
                                                    } else {
                                                        puts("OP:")
                                                        putchar(c)
                                                        putchar(10)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            mut g_pos = g_pos + 1
                            mut c = __byte_load(g_src, g_pos)
                        }
                    }
                }
            }
        }

        free(g_src)
        mut r = 0
    }
}
