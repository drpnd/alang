// Test early return: abs_val(-5) = 5
fn abs_val(n: i32) (r: i32)
{
    if n < 0 {
        mut r = 0 - n
        return r
    }
    mut r = n
}

fn main() (r: i32)
{
    mut r = abs_val(-5)
}
