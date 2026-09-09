// Test break: sum 0..10 but stop at 5 -> 0+1+2+3+4 = 10
fn main() (r: i32)
{
    mut r = 0
    for x in 0..10 {
        if x == 5 {
            break
        }
        mut r = r + x
    }
}
