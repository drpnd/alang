// Edge case: enum with many variants, match the last one
enum Status {
    Ok,
    NotFound,
    Permission,
    Timeout,
    Internal,
    Busy
}

fn main() (r: i32)
{
    let s = Busy
    mut r = 0
    match s {
        Ok => mut r = 0,
        NotFound => mut r = 1,
        Permission => mut r = 2,
        Timeout => mut r = 3,
        Internal => mut r = 4,
        Busy => mut r = 99
    }
}
