# assembler directives
.set noat      # allow manual use of $at
.set noreorder # don't insert nops after branches
.set gp=64

.section .start, "ax"

.global __start
__start:
la $t0, __bss_start
la $t1, __bss_end
#Loop through BSS in 8-byte increments for clearing
bss_clear:
#Clear 8 BSS bytes
sw $zero, 0($t0)
sw $zero, 4($t0)
#Advance to next 8-bytes until out of BSS bytes
addiu $t0, $t0, 8
sltu $at, $t0, $t1
bnez $at, bss_clear
nop
#Jump to boot code with valid stack
la $t2, nuBoot
la $sp, nuMainStack+0x2000
jr $t2
nop
