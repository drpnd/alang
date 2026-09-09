// Test struct: Point with x, y fields; set x=42, read x
struct Point {
    x: i32,
    y: i32
}

fn main() (r: i32)
{
    let p: Point = 0
    mut p.x = 42
    mut r = p.x
}
