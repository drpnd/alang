// Test for loop: sum 0..5 = 0+1+2+3+4 = 10
fn main() (r: i32)
{
    mut r = 0
    for x in 0..5 {
        mut r = r + x
    }
}
