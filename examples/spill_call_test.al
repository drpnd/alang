// Test register spilling with function calls: >25 SSA values with CALL
// Uses argc to prevent constant folding, calls add() with spilled values
fn add(a: i32, b: i32) (r: i32)
{
    mut r = a + b
}

fn main(argc: i32, argv: i64) (r: i32)
{
    let buf: i64 = 0
    mut buf = __malloc(512)
    let i: i32 = 0
    mut i = 0
    while i < 40 {
        __mem_store(buf + i * 8, argc + i)
        mut i = i + 1
    }
    let v0: i64 = __mem_load(buf + 0)
    let v1: i64 = __mem_load(buf + 8)
    let v2: i64 = __mem_load(buf + 16)
    let v3: i64 = __mem_load(buf + 24)
    let v4: i64 = __mem_load(buf + 32)
    let v5: i64 = __mem_load(buf + 40)
    let v6: i64 = __mem_load(buf + 48)
    let v7: i64 = __mem_load(buf + 56)
    let v8: i64 = __mem_load(buf + 64)
    let v9: i64 = __mem_load(buf + 72)
    let v10: i64 = __mem_load(buf + 80)
    let v11: i64 = __mem_load(buf + 88)
    let v12: i64 = __mem_load(buf + 96)
    let v13: i64 = __mem_load(buf + 104)
    let v14: i64 = __mem_load(buf + 112)
    let v15: i64 = __mem_load(buf + 120)
    let v16: i64 = __mem_load(buf + 128)
    let v17: i64 = __mem_load(buf + 136)
    let v18: i64 = __mem_load(buf + 144)
    let v19: i64 = __mem_load(buf + 152)
    let v20: i64 = __mem_load(buf + 160)
    let v21: i64 = __mem_load(buf + 168)
    let v22: i64 = __mem_load(buf + 176)
    let v23: i64 = __mem_load(buf + 184)
    let v24: i64 = __mem_load(buf + 192)
    let v25: i64 = __mem_load(buf + 200)
    let v26: i64 = __mem_load(buf + 208)
    let v27: i64 = __mem_load(buf + 216)
    let v28: i64 = __mem_load(buf + 224)
    let v29: i64 = __mem_load(buf + 232)
    let v30: i64 = __mem_load(buf + 240)
    let v31: i64 = __mem_load(buf + 248)
    let v32: i64 = __mem_load(buf + 256)
    let v33: i64 = __mem_load(buf + 264)
    let v34: i64 = __mem_load(buf + 272)
    let v35: i64 = __mem_load(buf + 280)
    let v36: i64 = __mem_load(buf + 288)
    let v37: i64 = __mem_load(buf + 296)
    let v38: i64 = __mem_load(buf + 304)
    let v39: i64 = __mem_load(buf + 312)
    let s1: i32 = add(v9, v10)
    let s2: i32 = add(v11, v12)
    let s3: i32 = add(v13, v14)
    mut r = v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 + v12 + v13 + v14 + v15 + v16 + v17 + v18 + v19 + v20 + v21 + v22 + v23 + v24 + v25 + v26 + v27 + v28 + v29 + v30 + v31 + v32 + v33 + v34 + v35 + v36 + v37 + v38 + v39 + s1 + s2 + s3
}
