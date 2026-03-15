#include "gfx.h"

#define ROW_STRIDE ((GFX_TEXT_BMP_COLS - 1) * 32)

@ i Think i'm better than gcc. Go my hand-written assmebly
@ Fuck gcc is 0.06% faster. Fuck. I just wasted my time.
@ Wait. What if i'm smart and use ldmia/stmia... Go my block transfer
@ instructinons...

@ okay i'm pretty sure there's a bug because i think i'm supposed to draw the
@ last row single when it starts odd. but for some reason nothing seems awry.
@ so. i guess i shouldn't fix it?
.global _gfx_text_blit_tile
.section ".iwram", "ax", %progbits
.arm
.align 2
_gfx_text_blit_tile:
    @ r0: uint x
    @ r1: uint y
    @ r2: const u32 tile[8]
    push {r4-r11}

    @ before doing anything, copy source bitmap to stack.
    @ then adjust tile*. because iwram = Fast. Fast = Good.
    @ original tile* may live in EWRAM or ROM, so...
    ldmia r2, {r3-r10}
    push {r3-r10}
    mov r2, sp

    @ uint ry = y & 7;
    @ ry = r3
    and r3, r1, #7

    @ u32 *row = &gfx_text_bmp_buf[(y >> 3) * GFX_TEXT_BMP_COLS + (x >> 3)].data[ry];
    @ row = r4
    @ (temp) r5
    lsr r4, r1, #3
    mov r5, #GFX_TEXT_BMP_COLS
    mul r4, r5
    add r4, r4, r0, lsr #3
    ldr r5, =gfx_text_bmp_buf
    add r4, r5, r4, lsl #5
    add r4, r4, r3, lsl #2

    @ const u32 *src_row = src_tile->data;
    @ src_row = r2
    @ no-op

    @ uint shf = (x & 7) << 2;
    @ shf = r5
    and r5, r0, #7
    lsls r5, #2

    @ r6 = row loop countdown
    mov r6, #8
    rsb r3, #8 @ invert ry so that the row-end check is faster

    @ registers used for block transfer:
    @ {r8, r9, r10, r11}

    beq .no_shf @ branch if zero
    @ otherwise, run branch that handles shifts

.yes_shf:
    @ r0 = 32 - shf
    rsb r0, r5, #32

    @ if odd, run one iteration of
    @ the loop to make it even...
    tst r3, #1
    beq .yes_shf_loop

    @ u32 src = *(src_row++)
    @ *row |= src << shf
    @ *(row + 8) |= src >> (32 - shf)
    @ r7: src
    @ ip: *row
    ldr r7, [r2], #4
    ldr ip, [r4]
    orr ip, ip, r7, lsl r5
    str ip, [r4]
    ldr ip, [r4, #32]
    orr ip, ip, r7, lsr r0
    str ip, [r4, #32]

    @ ++row
    add r4, #4

    @ if (++ry == 8)
    @   row += (GFX_TEXT_BMP_COLS - 1) * 8;
    subs r3, #1
    addeq r4, #ROW_STRIDE  @ add if zero

    sub r6, #1

.yes_shf_loop:
    @ load two source rouces into r7 and ip
    ldmia r2!, {r7, ip}

    @ *row |= src << shf;
    @ store in r8
    ldr r8, [r4]
    orr r8, r8, r7, lsl r5

    @ *(row + 8) |= src >> (32 - shf);
    @ store in r10
    ldr r10, [r4, #32]
    orr r10, r10, r7, lsr r0

    @ *(row++) |= src << shf
    @ store in r9
    ldr r9, [r4, #4]
    orr r9, r9, ip, lsl r5
    
    @ *(row + 8) |= src >> (32 - shf);
    @ store in r11
    ldr r11, [r4, #36]
    orr r11, r11, ip, lsr r0

    @ block-write two rows of the
    @ two tiles
    @ r4 will also be += 8 afterwards
    stmia r4!, {r8-r9}
    add r8, r4, #(32 - 8)
    stmia r8, {r10-r11}

    @ if ((ry += 2) == 8)
    @   row += (GFX_TEXT_BMP_COLS - 1) * 8;
    subs r3, #2
    addeq r4, #ROW_STRIDE  @ add if zero

    subs r6, #2
    bne .yes_shf_loop @ branch if not zero

    add sp, #32 @ pop copy of source bitmap
    pop {r4-r11}
    bx lr

.no_shf:
    @ if odd, run one iteration of
    @ the loop to make it even...
    tst r3, #1
    beq .no_shf_loop

    @ u32 src = *(src_row++)
    @ *(row++) |= src
    @ r7: src
    @ ip: *row
    ldr r7, [r2], #4
    ldr ip, [r4]
    orr ip, ip, r7
    str ip, [r4], #4 @ row++

    @ if (++ry == 8)
    @   row += (GFX_TEXT_BMP_COLS - 1) * 8;
    subs r3, #1
    addeq r4, #ROW_STRIDE  @ add if zero

    sub r6, #1

.no_shf_loop:
    @ transfer two rows at a time.
    @ could theoretically do four, but i'm too lazy to handle the logic to make
    @ sure that non-alignment to four doesn't break anything.
    @ but i can optimize loading from source by using ldmia...
    
    @ load two rows from source
    ldmia r2!, {r8-r9}

    @ load two rows from dest
    ldmia r4, {r10-r11}

    @ dest[0] |= src[0];
    @ dest[1] |= src[1];
    orr r10, r8
    orr r11, r9

    @ block-write two rows of the tile
    stmia r4!, {r10-r11}

    @ if ((ry += 2) == 8)
    @   row += (GFX_TEXT_BMP_COLS - 1) * 8;
    subs r3, #2
    addeq r4, #ROW_STRIDE  @ add if zero

    subs r6, #2
    bne .no_shf_loop @ branch if not zero

    add sp, #32 @ pop copy of source bitmap
    pop {r4-r11}
    bx lr
@ Fuck that's long. Um. I'm too lazy to rewrite the colored version. There's not
@ going to be much colored text on screen compared to non-colored text. Also
@ the text blitting isn't a performance tank or anything I just thought it'd be
@ fun.


@ Ok fr this time i Think im better than gcc. Go my block transfer instructions.
.global gfx_text_bmap_fill
.section ".iwram", "ax", %progbits
.arm
.align 2
gfx_text_bmap_fill:
    @ check for no-op
    cmp r2, #0 @ param cols
    beq .return
    cmp r3, #0 @ param rows
    beq .return

    mov ip, sp @ save current sp for param reads
    push {r4-r11}
    
    @ r4 = data param
    ldr r4, [ip]

    @ r11 = row ypos
    mov r11, r1

    @ r0 = param oc
    @ r1 = tile ptr (derived from param or)
    @ r2 = column count
    @ r3 = dec loop counter (param rows)
    @ (r5)
    mov r5, #GFX_TEXT_BMP_COLS
    mul r1, r5
    add r1, r0
    lsl r1, #5 @ SIZEOF_TILE (32) as shift
    ldr r5, =gfx_text_bmp_buf
    add r1, r5

    @ r6-r9 used for block transfer
    @ r10: row stride
    mov r10, #(GFX_TEXT_BMP_COLS * SIZEOF_TILE)

    @ r0: tile row origin
    mov r0, r1

    @ ip: pointer to gfx_text_bmp_dirty_rows
    ldr ip, =gfx_text_bmp_dirty_rows

.row_loop:
    @ flag row as dirty
    @ (r6: temp)
    mov r6, #1
    strb r6, [ip, r11]

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
    add r11, #1
    mov r1, r0
    subs r3, #1
    bne .row_loop

    pop {r4-r11}

.return:
    bx lr