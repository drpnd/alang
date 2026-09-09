// Test break in loop: count to 5
fn main() (r: i32)
{
    let i: i32 = 0
    mut i = 0
    loop {
        if i == 5 {
            break
        }
        mut i = i + 1
    }
    mut r = i
}
