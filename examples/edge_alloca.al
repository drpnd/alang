// Edge case: stack allocation with __alloca
fn main() (r: i32)
{
    let buf: i64 = 0
    mut buf = __alloca(64)
    __mem_store(buf + 0, 42)
    __mem_store(buf + 8, 84)
    let v0: i64 = 0
    mut v0 = __mem_load(buf + 0)
    let v1: i64 = 0
    mut v1 = __mem_load(buf + 8)
    mut r = v0 + v1
}
