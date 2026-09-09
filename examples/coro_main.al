// Coroutine called from main — main returns 42
fn helper() (r: i32)
{
    mut r = 42
}

fn main() (r: i32)
{
    mut r = helper()
}
