// TCP data flow pipeline: read from TCP, transform, write to TCP
// Receives integers, doubles them, sends to another TCP port
fn double(x: i32) (r: i32)
{
    mut r = x + x
}

fn increment(x: i32) (r: i32)
{
    mut r = x + 1
}

graph main {
    source("tcp-listen:9090") |> increment |> double |> sink("stdout")
}
