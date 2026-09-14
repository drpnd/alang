// Power: 2^10 = 1024 (mod 256 = 0)
fn power(base: i32, exp: i32) (r: i32)
{
    mut r = 1
    let i: i32 = 0
    mut i = 0
    while i < exp {
        mut r = r * base
        mut i = i + 1
    }
}

fn main() (r: i32)
{
    mut r = power(2, 10)
}
