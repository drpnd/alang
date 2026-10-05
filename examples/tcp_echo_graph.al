// TCP echo server using data flow graph syntax
// Listens on port 9090, echoes received data back to client
fn identity(x: i32) (r: i32)
{
    mut r = x
}

graph main {
    source("tcp-listen:9090") |> identity |> sink("stdout")
}
