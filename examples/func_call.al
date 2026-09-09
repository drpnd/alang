// Test function call: add(3, 4) = 7
fn add(a: i32, b: i32) (r: i32)
{
    mut r = a + b
}

fn main() (r: i32)
{
    mut r = add(3, 4)
}
