// Edge case: enum match where no case matches - should fall through
enum Opt {
    A,
    B,
    C
}

fn main() (r: i32)
{
    let x = C
    mut r = 42
    match x {
        A => mut r = 1,
        B => mut r = 2
    }
}
