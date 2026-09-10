// Test enum match: match Green -> return 42
enum Color {
    Red,
    Green,
    Blue
}

fn main() (r: i32)
{
    let c = Green
    mut r = 0
    match c {
        Red => mut r = 1,
        Green => mut r = 42,
        Blue => mut r = 3
    }
}
