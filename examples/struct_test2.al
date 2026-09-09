// Test struct with multiple fields: set x=10, y=20, return x+y=30
struct Point {
    x: i32,
    y: i32
}

fn main() (r: i32)
{
    let p: Point = 0
    mut p.x = 10
    mut p.y = 20
    mut r = p.x + p.y
}
