// Edge case: long string operations with __str_eq and __str_len
fn main(argc: i32, argv: i64) (r: i32)
{
    let s1: i64 = 0
    mut s1 = "hello world"
    let s2: i64 = 0
    mut s2 = "hello world"
    let s3: i64 = 0
    mut s3 = "goodbye world"
    let len1: i64 = 0
    mut len1 = __str_len(s1)
    let len2: i64 = 0
    mut len2 = __str_len(s3)
    let eq1: i32 = 0
    mut eq1 = __str_eq(s1, s2)
    let eq2: i32 = 0
    mut eq2 = __str_eq(s1, s3)
    mut r = len1 + len2 + eq1 + eq2
}
