// Sum of digits: sum(12345) = 15
fn sum_digits(n: i32) (r: i32)
{
    mut r = 0
    while n > 0 {
        mut r = r + n % 10
        mut n = n / 10
    }
}

fn main() (r: i32)
{
    mut r = sum_digits(12345)
}
