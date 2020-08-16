// ADD
04 ib       | I     | al,imm8
05 iw       | I     | ax,imm16
05 id       | I     | eax,imm32
W 05 id     | I     | rax,imm32
80 /0 ib    | MI    | r/m8,imm8
81 /0 iw    | MI    | r/m16,imm16
81 /0 id    | MI    | r/m32,imm32
W 81 /0 id  | MI    | r/m64,imm32
83 /0 ib    | MI    | r/m16,imm8
83 /0 ib    | MI    | r/m32,imm8
W 83 /9 ib  | MI    | r/m64,imm8
00 /r       | MR    | r/m8,r8
01 /r       | MR    | r/m16,r16
01 /r       | MR    | r/m32,r32
W 01 /r     | MR    | r/m64,r64
02 /r       | RM    | r8,r/m8
03 /r       | RM    | r16,r/m16
03 /r       | RM    | r32,r/m32
W 03 /r     | RM    | r64,r/m64
