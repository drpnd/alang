// Test continue: sum 0..6 but skip 2 and 4 -> 0+1+3+5 = 9
fn main() (r: i32)
{
    mut r = 0
    for x in 0..6 {
        if x == 2 {
            continue
        }
        if x == 4 {
            continue
        }
        mut r = r + x
    }
}
