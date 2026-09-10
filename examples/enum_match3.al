// Test enum match: match Blue -> return 3
enum Color {
    Red,
    Green,
    Blue
}

fn main() (r: i32)
{
    let c = Blue
    mut r = 0
    match c {
        Red => mut r = 1,
        Green => mut r = 42,
        Blue => mut r = 3
    }
}
