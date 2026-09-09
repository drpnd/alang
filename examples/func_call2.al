// Test calling a function from another: double(add(3, 4)) = 14
fn add(a: i32, b: i32) (r: i32)
{
    mut r = a + b
}

fn double(x: i32) (r: i32)
{
    mut r = x + x
}

fn main() (r: i32)
{
    mut r = double(add(3, 4))
}
