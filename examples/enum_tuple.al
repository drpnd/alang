// Test enum with tuple variant: Option type
enum Option {
    Some(i32),
    None
}

fn main() (r: i32)
{
    let x = Some(42)
    mut r = 0
    match x {
        Some => mut r = 1,
        None => mut r = 0
    }
}
