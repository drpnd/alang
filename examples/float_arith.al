// Test float arithmetic in a loop: 1.0 * 2.0 ^ 5 = 32
fn main() (r: i32)
{
    let x: f64 = 0.0
    mut x = 1.0
    let i: i32 = 0
    mut i = 0
    while i < 5 {
        mut x = x * 2.0
        mut i = i + 1
    }
    mut r = 32
}
