// TCP client: connect to server, send data, receive response
fn main() (r: i32)
{
    let sfd: i64 = 0
    mut sfd = __socket_create(2, 1, 0)

    // Build sockaddr_in: 127.0.0.1:8080
    let addr: i64 = 0
    mut addr = __malloc(16)
    __mem_store(addr, 0)
    __mem_store(addr + 8, 0)
    let packed: i64 = 0
    mut packed = 2 | (0x901F << 16) | (0x0100007F << 32)
    __mem_store(addr, packed)

    mut r = __socket_connect(sfd, addr, 16)

    // Send "Hello"
    let msg: i64 = 0
    mut msg = __malloc(6)
    __byte_store(msg, 0, 72)
    __byte_store(msg, 1, 101)
    __byte_store(msg, 2, 108)
    __byte_store(msg, 3, 108)
    __byte_store(msg, 4, 111)
    __byte_store(msg, 5, 0)
    mut r = __socket_send(sfd, msg, 5, 0)

    // Receive response
    let buf: i64 = 0
    mut buf = __malloc(4096)
    let n: i64 = 0
    mut n = __socket_recv(sfd, buf, 4096, 0)
    if n > 0 {
        mut r = sys_write(1, buf, n)
    }

    __socket_close(sfd)
    mut r = 0
}

fn sys_write(fd: i64, buf: i64, len: i64) (r: i64)
{
    mut r = __syscall(1, fd, buf, len)
}
