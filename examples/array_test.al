// Test array: store and retrieve elements
fn main() (r: i32)
{
    let arr: i64 = 0
    mut arr = __alloca(3)

    let idx0: i32 = 0
    mut idx0 = 0
    let idx1: i32 = 0
    mut idx1 = 1
    let idx2: i32 = 0
    mut idx2 = 2

    mut arr[idx0] = 10
    mut arr[idx1] = 20
    mut arr[idx2] = 30

    let a: i32 = 0
    mut a = arr[idx0]
    let b: i32 = 0
    mut b = arr[idx1]
    let c: i32 = 0
    mut c = arr[idx2]

    mut r = a + b + c
}
