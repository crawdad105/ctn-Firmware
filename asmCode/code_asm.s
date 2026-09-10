@;NOTE compile: arm-none-eabi-as asmCode/code_asm.s -o asmCode/code_asm.o
@;NOTE dump: arm-none-eabi-objdump -s asmCode/code_asm.o

@;NOTE full: arm-none-eabi-as asmCode/code_asm.s -o asmCode/code_asm.o; arm-none-eabi-objdump -s asmCode/code_asm.o

.arch armv7-a
.arm

.macro MOV32 reg, value
	LDR \reg, [pc, #0]
    ADD  pc, pc, #2		@; increment pc to be after the word
	.word \value
.endm

@; call an address
.macro CALL addr
    LDR  r12, [pc, #4] 	@; load word after the "add"
	BLX  r12			@; call (not jump)
    ADD  pc, pc, #2		@; increment pc to be after the word (i think #0 should work but im not touching it)
    .word \addr
.endm

@; jump
.macro JUMP addr
    LDR  r12, [pc, #0]
    BX   r12
    .word \addr
.endm

ldr     r3, [pc, #4]
add     r1, pc, #4
ADD     pc, pc, r3
.word (3 * 4)
.word 0x01
.word 0x02
.word 0x03

label_start:

push    { r1, r3 }
ldr     r1, [r1]

add     r0, r5, r5, lsl  #0x5
mov     r3, #0
add     r0, r4, r0, lsl  #0x2
mov     r2, #0x1        @; stack
add     r5, r5, #0x1
CALL 0x003adfdc 		@; call SetDefault_Item

pop     { r1, r3 }
add     r1, r1, #4

sub     r3, r3, #4
cmp     r3, #0
bne     label_start

jump 0x003f8f9c

.word 0xFFFFFFFF

@;; override code
@; jump to code cave (probably 0x00848000)
JUMP 0x00848000

@;; START code cave

push 	{lr}

@; original code
add     r0, r5, r5, lsl  #0x5
mov     r3, #0
add     r0, r4, r0, lsl  #0x2
mov     r2, #0x1        @; stack
MOV32 	r1, 0xA7		@; dynamite
add     r5, r5, #0x1
CALL 0x003adfdc 		@; call SetDefault_Item

@; new code
add     r0, r5, r5, lsl  #0x5
mov     r3, #0
add     r0, r4, r0, lsl  #0x2
mov     r2, #0x1        @; stack
MOV32 	r1, 0x13C5		@; holly hand grenade
add     r5, r5, #0x1
CALL 0x003adfdc 		@; call SetDefault_Item

@; run other code

pop 	{lr}

@; jump back after original SetDefault_Item
JUMP 0x003f7624

@;; END code cave