// GCD using Euclidean algorithm: gcd(48, 18) = 6
fn gcd(a: i32, b: i32) (r: i32)
{
    while b != 0 {
        let t: i32 = 0
        mut t = b
        mut b = a % b
        mut a = t
    }
    mut r = a
}

fn main() (r: i32)
{
    mut r = gcd(48, 18)
}
