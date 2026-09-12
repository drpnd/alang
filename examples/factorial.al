// Factorial: compute 5! = 120
fn factorial(n: i32) (r: i32)
{
    mut r = 1
    let i: i32 = 1
    mut i = 1
    while i <= n {
        mut r = r * i
        mut i = i + 1
    }
}

fn main() (r: i32)
{
    mut r = factorial(5)
}
