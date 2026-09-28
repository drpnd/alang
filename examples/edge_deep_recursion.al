// Edge case: deep recursion (10 levels)
fn count(n: i32) (r: i32)
{
    if n <= 0 {
        mut r = 0
    } else {
        let prev: i32 = 0
        mut prev = count(n - 1)
        mut r = prev + 1
    }
}

fn main() (r: i32)
{
    mut r = count(10)
}
