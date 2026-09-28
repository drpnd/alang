// Edge case: memory operations with __malloc, __mem_store, __mem_load
fn main() (r: i32)
{
    let buf: i64 = 0
    mut buf = __malloc(128)
    __mem_store(buf + 0, 100)
    __mem_store(buf + 8, 200)
    __mem_store(buf + 16, 300)
    let v0: i64 = 0
    mut v0 = __mem_load(buf + 0)
    let v1: i64 = 0
    mut v1 = __mem_load(buf + 8)
    let v2: i64 = 0
    mut v2 = __mem_load(buf + 16)
    mut r = v0 + v1 + v2
}
