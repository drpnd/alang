package main

coroutine cr0000(x i32, y i32) (z i32)
{
    z := x + y
}

fn add(x i32, y i32) (z i32)
{
    a fp64 := 16.0
    b fp64 := .123
    c fp64 := 2.
    d i32 := 0x1234
    e i32 := 01234
    f i32 := 1234
    g string := "abcd\"\012\x0a"
    z := x + y
}

//
// Main coroutine
//
coroutine main() (r i32)
{
    r := 0
}
