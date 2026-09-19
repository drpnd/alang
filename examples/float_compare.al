// Test float comparison: 3.14 < 6.28 → true → exit 1
fn less_than(a: f64, b: f64) (r: i32)
{
    if a < b {
        mut r = 1
    } else {
        mut r = 0
    }
}

fn main() (r: i32)
{
    let x: f64 = 3.14
    let y: f64 = 6.28
    mut r = less_than(x, y)
}
