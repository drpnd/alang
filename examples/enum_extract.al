// Test extracting data from tuple variant in match
enum Option {
    Some(i32),
    None
}

fn main() (r: i32)
{
    let x = Some(42)
    mut r = 0
    match x {
        Some(v) => mut r = v,
        None => mut r = 0
    }
}
