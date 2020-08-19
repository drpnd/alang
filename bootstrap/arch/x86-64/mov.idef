// MOV
88 /r       | MR    | r/m8,r8
89 /r       | MR    | r/m16,r16
89 /r       | MR    | r/m32,r32
W 89 /r     | MR    | r/m64,r64
8a /r       | RM    | r8,r/m8
8b /r       | RM    | r16,r/m16
8b /r       | RM    | r32,r/m32
W 8b /r     | RM    | r64,r/m64
8c /r       | MR    | r/m16,Sreg
W 8c /r     | MR    | r/m64,Sreg
8e /r       | RM    | Sreg,r/m16
W 8e        | RM    | Sreg,r/m64
a0          | FD    | al,moffs8         | Move byte at (seg:offset) to AL
W a0        | FD    | al                | Move byte at (offset) to AL
a1          | FD    | ax,moffs16        | Move word at (set:offset) to AX
a1          | FD    | eax,moffs32
W a1        | FD    | rax,moffs64
a2          | TD    | moffs8,al
W a2        | TD    | moffs8,al
a3          | TD    | moffs16,ax
a3          | TD    | moffs32,eax
W a3        | TD    | moffs64,rax
b0 rb ib    | OI    | r8,imm8
b8 rw iw    | OI    | r16,imm16
b8 rd id    | OI    | r32,imm32
W b8 rd io  | OI    | r64,imm64
c6 /0 ib    | MI    | r/m8,imm8
c7 /0 iw    | MI    | r/m16,imm16
c7 /0 id    | MI    | r/m32,imm32
W c7 /0 id  | MI    | r/m64,imm32
