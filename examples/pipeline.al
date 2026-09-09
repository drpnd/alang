// Data flow graph: source |> map |> sink
fn double(x: i32) (r: i32)
{
    mut r = x + x
}

graph main {
    source("file:input") |> double |> sink("stdout")
}
