// Test nested for loops: sum of products 2*3 = 6
fn main() (r: i32)
{
    mut r = 0
    for i in 0..2 {
        for j in 0..3 {
            mut r = r + 1
        }
    }
}
