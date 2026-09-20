// Test string builtins: __str_eq, __str_len
fn main(argc: i32, argv: i64) (r: i32)
{
    let argv_ptr: i64 = 0
    mut argv_ptr = argv
    let arg1: i64 = 0
    mut arg1 = __mem_load(argv_ptr + 8)
    let len: i64 = 0
    mut len = __str_len("hello")
    let eq: i32 = 0
    mut eq = __str_eq("hello", "hello")
    let ne: i32 = 0
    mut ne = __str_eq("hello", "world")
    mut r = len + eq + ne
}
