// Collatz sequence: count steps from n=27 to reach 1 = 111
fn collatz(n: i32) (r: i32)
{
    mut r = 0
    while n != 1 {
        if n % 2 == 0 {
            mut n = n / 2
        } else {
            mut n = n * 3 + 1
        }
        mut r = r + 1
    }
}

fn main() (r: i32)
{
    mut r = collatz(27)
}
