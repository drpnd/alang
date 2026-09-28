// Edge case: deeply nested if/else
fn main(argc: i32, argv: i64) (r: i32)
{
    let v: i32 = argc
    if v == 1 {
        mut r = 1
    } else {
        if v == 2 {
            mut r = 2
        } else {
            if v == 3 {
                mut r = 3
            } else {
                if v == 4 {
                    mut r = 4
                } else {
                    mut r = 99
                }
            }
        }
    }
}
