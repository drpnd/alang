// Prime counting: count primes up to 30
fn is_prime(n: i32) (r: i32)
{
    mut r = 1
    if n < 2 {
        mut r = 0
    }
    let i: i32 = 2
    mut i = 2
    while i * i <= n {
        if n % i == 0 {
            mut r = 0
        }
        mut i = i + 1
    }
}

fn main() (r: i32)
{
    mut r = 0
    for n in 2..30 {
        let p: i32 = 0
        mut p = is_prime(n)
        if p == 1 {
            mut r = r + 1
        }
    }
}
