// Example: map, filter, and reduce in data flow pipelines
//
// map(f)     — apply f to each value (same as plain function call)
// filter(p)  — keep only values where p(x) != 0
// reduce(f, init) — accumulate all values via f(acc, val), output once at end

fn double(x: i32) (r: i32)
{
    mut r = x + x
}

fn keep_large(x: i32) (r: i32)
{
    if x > 500 {
        mut r = 1
    } else {
        mut r = 0
    }
}

fn add(a: i32, b: i32) (r: i32)
{
    mut r = a + b
}

// Pipeline 1: map — double each value
// source |> map(double) |> sink("stdout")

// Pipeline 2: filter then map — keep values > 500, then double
// source |> filter(keep_large) |> double |> sink("stdout")

// Pipeline 3: reduce — sum all values
// source |> reduce(add, 0) |> sink("stdout")
