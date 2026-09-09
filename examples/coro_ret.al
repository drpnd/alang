// Coroutine that yields then returns — the final state returns Ready(1)
coro gen() (r: i32)
{
    yield 10
}

fn main() (r: i32)
{
    mut r = 7
}
