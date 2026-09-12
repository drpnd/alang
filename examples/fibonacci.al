// Fibonacci sequence: compute fib(10) = 55
// Uses iteration (while loop) instead of recursion
fn main() (r: i32)
{
    let a: i32 = 0
    let b: i32 = 1
    mut a = 0
    mut b = 1
    let i: i32 = 0
    mut i = 0
    while i < 10 {
        let temp: i32 = a + b
        mut temp = a + b
        mut a = b
        mut b = temp
        mut i = i + 1
    }
    mut r = a
}
