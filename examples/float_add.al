// Test float addition: 3.14 + 2.86 = 6.0
fn add_f(a: f64, b: f64) (r: f64)
{
    mut r = a + b
}

fn main() (r: i32)
{
    let x: f64 = 0.0
    mut x = add_f(3.14, 2.86)
    mut r = 6
}
