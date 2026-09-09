// Simple coroutine with yields — returns Ready (1)
coro gen() (r: i32)
{
    yield 1
    yield 2
}
