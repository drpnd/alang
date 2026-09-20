extern fn fopen(path: str, mode: str) (fp: i64)
extern fn fclose(fp: i64) (r: i32)
extern fn fread(buf: i64, size: i64, count: i64, fp: i64) (r: i64)
extern fn malloc(size: i64) (ptr: i64)
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

fn is_space(c: i32) (r: i32)
{
    if c == 32 {
        mut r = 1
    } else {
        if c == 10 {
            mut r = 1
        } else {
            mut r = 0
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
        while g_pos < g_size {
            while is_space(c) == 1 {
                mut g_pos = g_pos + 1
                mut c = __byte_load(g_src, g_pos)
            }
            if g_pos >= g_size {
            } else {
                if is_alpha(c) == 1 {
                    while is_alpha(c) == 1 {
                        putchar(c)
                        mut g_pos = g_pos + 1
                        mut c = __byte_load(g_src, g_pos)
                    }
                    putchar(10)
                } else {
                    putchar(c)
                    putchar(10)
                    mut g_pos = g_pos + 1
                    mut c = __byte_load(g_src, g_pos)
                }
            }
        }
        puts("DONE")
        mut r = 0
    }
}
