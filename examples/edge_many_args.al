// Edge case: function with many arguments (>6, requires stack passing)
fn sum8(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32, g: i32, h: i32) (r: i32)
{
    mut r = a + b + c + d + e + f + g + h
}

fn main() (r: i32)
{
    mut r = sum8(1, 2, 3, 4, 5, 6, 7, 8)
}
