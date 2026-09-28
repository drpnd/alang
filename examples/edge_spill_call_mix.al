// Edge case: spill with function calls interspersed
fn double(x: i32) (r: i32)
{
    mut r = x * 2
}

fn main() (r: i32)
{
    let a: i32 = double(5)
    let b: i32 = double(10)
    let c: i32 = double(15)
    let d: i32 = double(20)
    let e: i32 = double(25)
    let f: i32 = double(30)
    let g: i32 = double(35)
    let h: i32 = double(40)
    let i: i32 = double(45)
    let j: i32 = double(50)
    let k: i32 = double(55)
    let l: i32 = double(60)
    let m: i32 = double(65)
    let n: i32 = double(70)
    let o: i32 = double(75)
    let p: i32 = double(80)
    let q: i32 = double(85)
    let r2: i32 = double(90)
    let s: i32 = double(95)
    let t: i32 = double(100)
    mut r = a + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p + q + r2 + s + t
}
