// Recursive fibonacci: fib(10) = 55
fn fib(n: i32) (r: i32)
{
    if n < 2 {
        mut r = n
    } else {
        let a: i32 = 0
        mut a = fib(n - 1)
        let b: i32 = 0
        mut b = fib(n - 2)
        mut r = a + b
    }
}

fn main() (r: i32)
{
    mut r = fib(10)
}
