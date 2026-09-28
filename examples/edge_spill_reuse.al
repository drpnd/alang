// Edge case: variable reuse after spilling (read after write to stack)
fn main() (r: i32)
{
    let a0: i32 = 10
    let a1: i32 = 20
    let a2: i32 = 30
    let a3: i32 = 40
    let a4: i32 = 50
    let a5: i32 = 60
    let a6: i32 = 70
    let a7: i32 = 80
    let a8: i32 = 90
    let a9: i32 = 100
    let a10: i32 = 110
    let a11: i32 = 120
    let a12: i32 = 130
    let a13: i32 = 140
    let a14: i32 = 150
    let a15: i32 = 160
    let a16: i32 = 170
    let a17: i32 = 180
    let a18: i32 = 190
    let a19: i32 = 200
    let a20: i32 = 210
    let a21: i32 = 220
    let a22: i32 = 230
    let a23: i32 = 240
    let a24: i32 = 250
    let a25: i32 = 260
    let a26: i32 = 270
    let a27: i32 = 280
    let a28: i32 = 290
    let a29: i32 = 300
    let a30: i32 = 310
    let a31: i32 = 320
    let a32: i32 = 330
    let a33: i32 = 340
    let a34: i32 = 350
    mut a0 = a0 + a1
    mut a1 = a2 + a3
    mut a2 = a4 + a5
    mut a3 = a6 + a7
    mut r = a0 + a1 + a2 + a3 + a8 + a9 + a10 + a11 + a12 + a13 + a14 + a15 + a16 + a17 + a18 + a19 + a20 + a21 + a22 + a23 + a24 + a25 + a26 + a27 + a28 + a29 + a30 + a31 + a32 + a33 + a34
}
