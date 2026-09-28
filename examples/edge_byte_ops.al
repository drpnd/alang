// Edge case: byte-level memory operations
fn main() (r: i32)
{
    let buf: i64 = 0
    mut buf = __malloc(64)
    __byte_store(buf, 0, 72)
    __byte_store(buf, 1, 105)
    __byte_store(buf, 2, 0)
    let b0: i64 = 0
    mut b0 = __byte_load(buf, 0)
    let b1: i64 = 0
    mut b1 = __byte_load(buf, 1)
    let b2: i64 = 0
    mut b2 = __byte_load(buf, 2)
    mut r = b0 + b1 + b2
}
