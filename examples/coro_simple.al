// Simple coroutine with yields — main returns 1 (Ready)
coro gen() (r: i32)
{
    yield 1
    yield 2
}

fn main() (r: i32)
{
    mut r = 1
}
