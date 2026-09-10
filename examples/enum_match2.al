// Test enum match: match Red -> return 1
enum Color {
    Red,
    Green,
    Blue
}

fn main() (r: i32)
{
    let c = Red
    mut r = 0
    match c {
        Red => mut r = 1,
        Green => mut r = 42,
        Blue => mut r = 3
    }
}
