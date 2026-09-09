// Test function with if: max(3, 7) = 7
fn max(a: i32, b: i32) (r: i32)
{
    if a < b {
        mut r = b
    } else {
        mut r = a
    }
}

fn main() (r: i32)
{
    mut r = max(3, 7)
}
