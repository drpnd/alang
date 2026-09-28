// Edge case: complex while loop with break and continue
fn main() (r: i32)
{
    let i: i32 = 0
    mut i = 0
    let sum: i32 = 0
    mut sum = 0
    while i < 100 {
        mut i = i + 1
        if i == 5 {
            // skip 5
        } else {
            if i == 10 {
                break
            }
            mut sum = sum + i
        }
    }
    mut r = sum
}
