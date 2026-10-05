// End-to-end data flow pipeline test
// Reads i64 values from input file, doubles them, writes to output file
//
// Usage (with bootstrap compiler):
//   1. Write input data: use write_input.al to create /tmp/input.bin
//   2. Run pipeline: ./file_pipeline_test
//   3. Verify output: use read_output.al to read /tmp/output.bin
//
// Expected: input 0..9 -> output 0,2,4,6,8,10,12,14,16,18

fn double(x: i32) (r: i32)
{
    mut r = x + x
}

fn main() (r: i32)
{
    let infp: i64 = 0
    mut infp = fopen("/tmp/input.bin", "r")
    if infp == 0 {
        puts("input open failed")
        mut r = 1
    } else {
        let outfp: i64 = 0
        mut outfp = fopen("/tmp/output.bin", "w")
        if outfp == 0 {
            puts("output open failed")
            mut r = 1
        } else {
            let buf: i64 = 0
            mut buf = __malloc(8)
            let n: i64 = 0
            mut n = 1
            while n > 0 {
                mut n = fread(buf, 1, 8, infp)
                if n > 0 {
                    let val: i64 = 0
                    mut val = __mem_load(buf)
                    mut val = double(val)
                    __mem_store(buf, val)
                    mut r = fwrite(buf, 1, 8, outfp)
                }
            }
            mut r = fclose(infp)
            mut r = fclose(outfp)
            puts("pipeline done")
            mut r = 0
        }
    }
}
