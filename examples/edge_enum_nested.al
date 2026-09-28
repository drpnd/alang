// Edge case: enum with nested match
enum Shape {
    Circle(i32),
    Square(i32),
    Triangle(i32)
}

fn main() (r: i32)
{
    let s = Circle(10)
    let t = Square(20)
    mut r = 0
    match s {
        Circle(r2) => match t {
            Square(s2) => mut r = r2 + s2,
            Triangle(t2) => mut r = t2,
            Square => mut r = 0,
            Circle => mut r = 0
        },
        Square(v) => mut r = v,
        Triangle(v) => mut r = v,
        Square => mut r = 0,
        Circle => mut r = 0,
        Triangle => mut r = 0
    }
}
