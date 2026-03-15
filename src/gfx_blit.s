#include "gfx.h"

@ i Think i'm better than gcc. Go my hand-written assmebly
@ Fuck gcc is 0.06% faster. Fuck. I just wasted my time.
/*

.global blit_tile

.section ".iwram", "ax", %progbits
.arm
.align 2
blit_tile:
    push {r4-r10}

    @ uint ry = y & 7;
    @ ry = r3
    and r3, r1, #7

    @ u32 *row = &gfx_text_bmp_buf[(y >> 3) * GFX_TEXT_BMP_COLS + (x >> 3)].data[ry];
    @ row = r4
    lsr r4, r1, #3
    mov r5, #GFX_TEXT_BMP_COLS
    mul r4, r5
    add r4, r4, r0, lsr #3
    lsl r4, r4, #5
    ldr r5, =gfx_text_bmp_buf
    add r4, r5
    add r4, r4, r3, lsl #2

    @ const u32 *src_row = src_tile->data;
    @ src_row = r2
    @ no-op

    @ uint shf = (x & 7) << 2;
    @ shf = r5
    and r5, r0, #7
    lsls r5, r5, #2

    @ r10 = row stride
    mov r10, #((GFX_TEXT_BMP_COLS - 1) * 8)
    lsl r10, #2 

    @ r6 = loop countdown
    mov r6, #8

    beq .no_shf @ branch if zero

    @ r0 = 32 - shf
    rsb r0, r5, #32

.yes_shf:
    @ u32 src = *(src_row++);
    @ r7 = src
    ldr r7, [r2], #4

    @ *row |= src << shf;
    @ r8 = src << shf
    @ r9 = *row
    @ lsl r8, r7, r5
    ldr r9, [r4]
    orr r9, r7, lsl r5
    str r9, [r4]

    @ *(row + 8) |= src >> (32 - shf);
    @ r8 = src >> (32 - shf)
    @ r9 = *(row + 8)
    @ rsb r8, r5, #32
    @ lsr r8, r7, r8
    ldr r9, [r4, #32]
    orr r9, r9, r7, lsr r0
    str r9, [r4, #32]

    @ ++row;
    add r4, #4

    @ if (++ry == 8)
    @   row += (GFX_TEXT_BMP_COLS - 1) * 8;
    add r3, r3, #1
    cmp r3, #8
    addeq r4, r4, r10  @ add if zero?

    subs r6, r6, #1
    bne .yes_shf @ branch if not zero
    pop {r4-r10}
    bx lr

.no_shf:
    @ u32 src = *(src_row++);
    @ r7 = src
    ldr r7, [r2], #4

    @ *row |= src
    @ r9 = *row
    ldr r9, [r4]
    orr r9, r7
    str r9, [r4], #4
    @ ++row;

    @ if (++ry == 8)
    @   row += (GFX_TEXT_BMP_COLS - 1) * 8;
    add r3, r3, #1
    cmp r3, #8
    addeq r4, r4, r10 @ add if zero?

    subs r6, r6, #1
    bne .no_shf @ branch if not zero
    pop {r4-r10}
    bx lr
*/

@ Ok fr this time i Think im better than gcc. Go my block transfer instructions.

.global _gfx_text_bmap_fill

.section ".iwram", "ax", %progbits
.arm
.align 2
_gfx_text_bmap_fill:
    push {r4-r10}

    @ r4 = data param
    ldr r4, [sp, #(7 * 4)]

    @ r0 = param oc
    @ r1 = tile ptr (derived from param or)
    @ r2 = column count
    @ r3 = dec loop counter (param rows)
    @ (r5)
    mov r5, #GFX_TEXT_BMP_COLS
    mul r1, r5
    add r1, r0
    lsl r1, #5 @ SIZEOF_TILE (32) as shift
    ldr r5, =GFX_TEXT_BMP_VRAM
    add r1, r5

    @ r6-r9 used for block transfer
    @ r10: row stride
    mov r10, #(GFX_TEXT_BMP_COLS * SIZEOF_TILE)

    @ r0: tile row origin
    mov r0, r1

.row_loop:
    @ r5: dec col counter
    mov r5, r2

.col_loop:
    @ copy source data to dest tile
    @ 4 uints at a time
    ldmia r4!, {r6-r9}
    stmia r1!, {r6-r9}
    ldmia r4, {r6-r9}
    stmia r1!, {r6-r9}
    sub r4, #16

    @ end of col_loop
    subs r5, #1
    bne .col_loop

    @ end of row_loop
    add r0, r10
    mov r1, r0
    subs r3, #1
    bne .row_loop

    pop {r4-r10}
    bx lr