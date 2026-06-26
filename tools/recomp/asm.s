
a.out:     file format elf32-littlearm


Disassembly of section .iwram:

00000000 <mpm_nullsample>:
   0:	00000080 	andeq	r0, r0, r0, lsl #1

00000004 <mmMixerMix>:
   4:	e3500000 	cmp	r0, #0
   8:	012fff1e 	bxeq	lr
   c:	e92d4ff0 	push	{r4, r5, r6, r7, r8, r9, sl, fp, lr}
  10:	e92d0001 	stmfd	sp!, {r0}
  14:	e200a007 	and	sl, r0, #7
  18:	e1a021a0 	lsr	r2, r0, #3
  1c:	e59f0488 	ldr	r0, [pc, #1160]	@ 4ac <.mpm_copy2_end+0x10>
  20:	e5900000 	ldr	r0, [r0]
  24:	e3a01000 	mov	r1, #0
  28:	e1a03001 	mov	r3, r1
  2c:	e1a04001 	mov	r4, r1
  30:	e1a05001 	mov	r5, r1
  34:	e1a06001 	mov	r6, r1
  38:	e1a07001 	mov	r7, r1
  3c:	e1a08001 	mov	r8, r1
  40:	e1a09001 	mov	r9, r1
  44:	e3520000 	cmp	r2, #0
  48:	0a000002 	beq	58 <mmMixerMix+0x54>
  4c:	e8a003fa 	stmia	r0!, {r1, r3, r4, r5, r6, r7, r8, r9}
  50:	e2522001 	subs	r2, r2, #1
  54:	1afffffc 	bne	4c <mmMixerMix+0x48>
  58:	e35a0000 	cmp	sl, #0
  5c:	0a000002 	beq	6c <mmMixerMix+0x68>
  60:	e4801004 	str	r1, [r0], #4
  64:	e25aa001 	subs	sl, sl, #1
  68:	1afffffc 	bne	60 <mmMixerMix+0x5c>
  6c:	e59fc43c 	ldr	ip, [pc, #1084]	@ 4b0 <.mpm_copy2_end+0x14>
  70:	e59cc000 	ldr	ip, [ip]
  74:	e3a0b000 	mov	fp, #0

00000078 <.mpm_cloop>:
  78:	e59ca000 	ldr	sl, [ip]
  7c:	e35a0000 	cmp	sl, #0
  80:	4a0000cd 	bmi	3bc <.mpm_next>
  84:	e59c900c 	ldr	r9, [ip, #12]
  88:	e3590000 	cmp	r9, #0
  8c:	0a0000ca 	beq	3bc <.mpm_next>
  90:	e59f041c 	ldr	r0, [pc, #1052]	@ 4b4 <.mpm_copy2_end+0x18>
  94:	e5900000 	ldr	r0, [r0]
  98:	e0090990 	mul	r9, r0, r9
  9c:	e1a09729 	lsr	r9, r9, #14
  a0:	e59f8404 	ldr	r8, [pc, #1028]	@ 4ac <.mpm_copy2_end+0x10>
  a4:	e5988000 	ldr	r8, [r8]
  a8:	e59c7004 	ldr	r7, [ip, #4]
  ac:	e5dc6008 	ldrb	r6, [ip, #8]
  b0:	e5dc0009 	ldrb	r0, [ip, #9]
  b4:	e2600c01 	rsb	r0, r0, #256	@ 0x100
  b8:	e0050690 	mul	r5, r0, r6
  bc:	e1a05425 	lsr	r5, r5, #8
  c0:	e08bb005 	add	fp, fp, r5
  c4:	e2600c01 	rsb	r0, r0, #256	@ 0x100
  c8:	e0060690 	mul	r6, r0, r6
  cc:	e1a06426 	lsr	r6, r6, #8
  d0:	e08bb806 	add	fp, fp, r6, lsl #16
  d4:	e59d4000 	ldr	r4, [sp]

000000d8 <.mpm_remix_test>:
  d8:	e3a02000 	mov	r2, #0
  dc:	e0010994 	mul	r1, r4, r9
  e0:	e3590d5e 	cmp	r9, #6016	@ 0x1780
  e4:	aa000002 	bge	f4 <.mpm_remix_test+0x1c>
  e8:	e3510706 	cmp	r1, #1572864	@ 0x180000
  ec:	83a01706 	movhi	r1, #1572864	@ 0x180000
  f0:	83a02001 	movhi	r2, #1
  f4:	e51a000c 	ldr	r0, [sl, #-12]
  f8:	e0670600 	rsb	r0, r7, r0, lsl #12
  fc:	e1510000 	cmp	r1, r0
 100:	81a01000 	movhi	r1, r0
 104:	8a000001 	bhi	110 <.calc_mix>
 108:	e3520000 	cmp	r2, #0
 10c:	0a00003c 	beq	204 <.mpm_mix_full>

00000110 <.calc_mix>:
 110:	e52d1004 	push	{r1}		@ (str r1, [sp, #-4]!)
 114:	e1a00001 	mov	r0, r1
 118:	e3a02000 	mov	r2, #0
 11c:	e0500809 	subs	r0, r0, r9, lsl #16
 120:	22822801 	addcs	r2, r2, #65536	@ 0x10000
 124:	2afffffc 	bcs	11c <.calc_mix+0xc>
 128:	e0800809 	add	r0, r0, r9, lsl #16
 12c:	e1500789 	cmp	r0, r9, lsl #15
 130:	20400789 	subcs	r0, r0, r9, lsl #15
 134:	22822902 	addcs	r2, r2, #32768	@ 0x8000
 138:	e1500709 	cmp	r0, r9, lsl #14
 13c:	20400709 	subcs	r0, r0, r9, lsl #14
 140:	22822901 	addcs	r2, r2, #16384	@ 0x4000
 144:	e1500689 	cmp	r0, r9, lsl #13
 148:	20400689 	subcs	r0, r0, r9, lsl #13
 14c:	22822a02 	addcs	r2, r2, #8192	@ 0x2000
 150:	e1500609 	cmp	r0, r9, lsl #12
 154:	20400609 	subcs	r0, r0, r9, lsl #12
 158:	22822a01 	addcs	r2, r2, #4096	@ 0x1000
 15c:	e1500589 	cmp	r0, r9, lsl #11
 160:	20400589 	subcs	r0, r0, r9, lsl #11
 164:	22822b02 	addcs	r2, r2, #2048	@ 0x800
 168:	e1500509 	cmp	r0, r9, lsl #10
 16c:	20400509 	subcs	r0, r0, r9, lsl #10
 170:	22822b01 	addcs	r2, r2, #1024	@ 0x400
 174:	e1500489 	cmp	r0, r9, lsl #9
 178:	20400489 	subcs	r0, r0, r9, lsl #9
 17c:	22822c02 	addcs	r2, r2, #512	@ 0x200
 180:	e1500409 	cmp	r0, r9, lsl #8
 184:	20400409 	subcs	r0, r0, r9, lsl #8
 188:	22822c01 	addcs	r2, r2, #256	@ 0x100
 18c:	e1500389 	cmp	r0, r9, lsl #7
 190:	20400389 	subcs	r0, r0, r9, lsl #7
 194:	22822080 	addcs	r2, r2, #128	@ 0x80
 198:	e1500309 	cmp	r0, r9, lsl #6
 19c:	20400309 	subcs	r0, r0, r9, lsl #6
 1a0:	22822040 	addcs	r2, r2, #64	@ 0x40
 1a4:	e1500289 	cmp	r0, r9, lsl #5
 1a8:	20400289 	subcs	r0, r0, r9, lsl #5
 1ac:	22822020 	addcs	r2, r2, #32
 1b0:	e1500209 	cmp	r0, r9, lsl #4
 1b4:	20400209 	subcs	r0, r0, r9, lsl #4
 1b8:	22822010 	addcs	r2, r2, #16
 1bc:	e1500189 	cmp	r0, r9, lsl #3
 1c0:	20400189 	subcs	r0, r0, r9, lsl #3
 1c4:	22822008 	addcs	r2, r2, #8
 1c8:	e1500109 	cmp	r0, r9, lsl #2
 1cc:	20400109 	subcs	r0, r0, r9, lsl #2
 1d0:	22822004 	addcs	r2, r2, #4
 1d4:	e1500089 	cmp	r0, r9, lsl #1
 1d8:	20400089 	subcs	r0, r0, r9, lsl #1
 1dc:	22822002 	addcs	r2, r2, #2
 1e0:	e1500009 	cmp	r0, r9
 1e4:	20400009 	subcs	r0, r0, r9
 1e8:	22822001 	addcs	r2, r2, #1
 1ec:	e3500001 	cmp	r0, #1
 1f0:	e2a20000 	adc	r0, r2, #0
 1f4:	e49d1004 	pop	{r1}		@ (ldr r1, [sp], #4)
 1f8:	e0444000 	sub	r4, r4, r0
 1fc:	e1a03000 	mov	r3, r0
 200:	ea000001 	b	20c <.mpm_mix_short>

00000204 <.mpm_mix_full>:
 204:	e1a03004 	mov	r3, r4
 208:	e3a04000 	mov	r4, #0

0000020c <.mpm_mix_short>:
 20c:	e92d1810 	push	{r4, fp, ip}
 210:	e3530000 	cmp	r3, #0
 214:	0a00004c 	beq	34c <.mpm_mix_complete>
 218:	e3590d5e 	cmp	r9, #6016	@ 0x1780
 21c:	aa000031 	bge	2e8 <.dont_use_fetch>
 220:	e92d1ff8 	push	{r3, r4, r5, r6, r7, r8, r9, sl, fp, ip}
 224:	e59f028c 	ldr	r0, [pc, #652]	@ 4b8 <.mpm_copy2_end+0x1c>
 228:	e08aa627 	add	sl, sl, r7, lsr #12
 22c:	e3caa003 	bic	sl, sl, #3
 230:	e2811901 	add	r1, r1, #16384	@ 0x4000
 234:	e251190a 	subs	r1, r1, #163840	@ 0x28000
 238:	3a00000b 	bcc	26c <.exit_fetch>

0000023c <.fetch>:
 23c:	e8ba4bfc 	ldm	sl!, {r2, r3, r4, r5, r6, r7, r8, r9, fp, lr}
 240:	e8a04bfc 	stmia	r0!, {r2, r3, r4, r5, r6, r7, r8, r9, fp, lr}
 244:	e251190a 	subs	r1, r1, #163840	@ 0x28000
 248:	3a000007 	bcc	26c <.exit_fetch>
 24c:	e8ba4bfc 	ldm	sl!, {r2, r3, r4, r5, r6, r7, r8, r9, fp, lr}
 250:	e8a04bfc 	stmia	r0!, {r2, r3, r4, r5, r6, r7, r8, r9, fp, lr}
 254:	e251190a 	subs	r1, r1, #163840	@ 0x28000
 258:	3a000003 	bcc	26c <.exit_fetch>
 25c:	e8ba4bfc 	ldm	sl!, {r2, r3, r4, r5, r6, r7, r8, r9, fp, lr}
 260:	e8a04bfc 	stmia	r0!, {r2, r3, r4, r5, r6, r7, r8, r9, fp, lr}
 264:	e251190a 	subs	r1, r1, #163840	@ 0x28000
 268:	2afffff3 	bcs	23c <.fetch>

0000026c <.exit_fetch>:
 26c:	e2911801 	adds	r1, r1, #65536	@ 0x10000
 270:	4a00000b 	bmi	2a4 <.end_medfetch>

00000274 <.medfetch>:
 274:	e8ba00fc 	ldm	sl!, {r2, r3, r4, r5, r6, r7}
 278:	e8a000fc 	stmia	r0!, {r2, r3, r4, r5, r6, r7}
 27c:	e2511906 	subs	r1, r1, #98304	@ 0x18000
 280:	3a000007 	bcc	2a4 <.end_medfetch>
 284:	e8ba00fc 	ldm	sl!, {r2, r3, r4, r5, r6, r7}
 288:	e8a000fc 	stmia	r0!, {r2, r3, r4, r5, r6, r7}
 28c:	e2511906 	subs	r1, r1, #98304	@ 0x18000
 290:	3a000003 	bcc	2a4 <.end_medfetch>
 294:	e8ba00fc 	ldm	sl!, {r2, r3, r4, r5, r6, r7}
 298:	e8a000fc 	stmia	r0!, {r2, r3, r4, r5, r6, r7}
 29c:	e2511906 	subs	r1, r1, #98304	@ 0x18000
 2a0:	2afffff3 	bcs	274 <.medfetch>

000002a4 <.end_medfetch>:
 2a4:	e2911906 	adds	r1, r1, #98304	@ 0x18000
 2a8:	4a000007 	bmi	2cc <.end_fetch>

000002ac <.fetchsmall>:
 2ac:	e49a2004 	ldr	r2, [sl], #4
 2b0:	e4802004 	str	r2, [r0], #4
 2b4:	e2511901 	subs	r1, r1, #16384	@ 0x4000
 2b8:	da000003 	ble	2cc <.end_fetch>
 2bc:	e49a2004 	ldr	r2, [sl], #4
 2c0:	e4802004 	str	r2, [r0], #4
 2c4:	e2511901 	subs	r1, r1, #16384	@ 0x4000
 2c8:	cafffff7 	bgt	2ac <.fetchsmall>

000002cc <.end_fetch>:
 2cc:	e8bd1ff8 	pop	{r3, r4, r5, r6, r7, r8, r9, sl, fp, ip}

000002d0 <fooo>:
 2d0:	e1a00627 	lsr	r0, r7, #12
 2d4:	e92d0401 	push	{r0, sl}
 2d8:	e1c77600 	bic	r7, r7, r0, lsl #12
 2dc:	e2000003 	and	r0, r0, #3
 2e0:	e59fa1d0 	ldr	sl, [pc, #464]	@ 4b8 <.mpm_copy2_end+0x1c>
 2e4:	e08aa000 	add	sl, sl, r0

000002e8 <.dont_use_fetch>:
 2e8:	e3180003 	tst	r8, #3
 2ec:	0a00000a 	beq	31c <.mpm_aligned>
 2f0:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 2f4:	e0877009 	add	r7, r7, r9
 2f8:	e0020590 	mul	r2, r0, r5
 2fc:	e1d810b0 	ldrh	r1, [r8]
 300:	e08112a2 	add	r1, r1, r2, lsr #5
 304:	e0c810b4 	strh	r1, [r8], #4
 308:	e0020690 	mul	r2, r0, r6
 30c:	e1d810b0 	ldrh	r1, [r8]
 310:	e08112a2 	add	r1, r1, r2, lsr #5
 314:	e0c810b2 	strh	r1, [r8], #2
 318:	e2433001 	sub	r3, r3, #1

0000031c <.mpm_aligned>:
 31c:	e1550006 	cmp	r5, r6
 320:	0a000006 	beq	340 <.mpm_mix_ac>
 324:	e3550000 	cmp	r5, #0
 328:	0a000003 	beq	33c <.mpm_mix_ar>
 32c:	e3560000 	cmp	r6, #0
 330:	0a000000 	beq	338 <.mpm_mix_al>
 334:	ea0000c2 	b	644 <mmMix_ArbPanning>

00000338 <.mpm_mix_al>:
 338:	ea000066 	b	4d8 <mmMix_HardLeft>

0000033c <.mpm_mix_ar>:
 33c:	ea000068 	b	4e4 <mmMix_HardRight>

00000340 <.mpm_mix_ac>:
 340:	e3550000 	cmp	r5, #0
 344:	1a000099 	bne	5b0 <mmMix_CenteredPanning>
 348:	ea00005f 	b	4cc <mmMix_Skip>

0000034c <.mpm_mix_complete>:
 34c:	e3590d5e 	cmp	r9, #6016	@ 0x1780
 350:	b8bd0401 	poplt	{r0, sl}
 354:	b0877600 	addlt	r7, r7, r0, lsl #12
 358:	e8bd1810 	pop	{r4, fp, ip}
 35c:	e51a100c 	ldr	r1, [sl, #-12]
 360:	e1a01601 	lsl	r1, r1, #12
 364:	e1510007 	cmp	r1, r7
 368:	ca000010 	bgt	3b0 <.mpm_channelfinished>
 36c:	e51a1008 	ldr	r1, [sl, #-8]
 370:	e3510000 	cmp	r1, #0
 374:	4a000003 	bmi	388 <.mpm_channel_stop>
 378:	e0477601 	sub	r7, r7, r1, lsl #12
 37c:	e3540000 	cmp	r4, #0
 380:	da00000a 	ble	3b0 <.mpm_channelfinished>
 384:	eaffff53 	b	d8 <.mpm_remix_test>

00000388 <.mpm_channel_stop>:
 388:	e3a01102 	mov	r1, #-2147483648	@ 0x80000000
 38c:	e58c1000 	str	r1, [ip]
 390:	e59fa124 	ldr	sl, [pc, #292]	@ 4bc <.mpm_copy2_end+0x20>
 394:	e3a09000 	mov	r9, #0
 398:	e3a07000 	mov	r7, #0
 39c:	e1b03004 	movs	r3, r4
 3a0:	da000002 	ble	3b0 <.mpm_channelfinished>
 3a4:	e3a04000 	mov	r4, #0
 3a8:	e3a01000 	mov	r1, #0
 3ac:	eaffff96 	b	20c <.mpm_mix_short>

000003b0 <.mpm_channelfinished>:
 3b0:	e3540000 	cmp	r4, #0
 3b4:	1affff47 	bne	d8 <.mpm_remix_test>
 3b8:	e58c7004 	str	r7, [ip, #4]

000003bc <.mpm_next>:
 3bc:	e28cc010 	add	ip, ip, #16
 3c0:	e59f00f8 	ldr	r0, [pc, #248]	@ 4c0 <.mpm_copy2_end+0x24>
 3c4:	e5900000 	ldr	r0, [r0]
 3c8:	e15c0000 	cmp	ip, r0
 3cc:	1affff29 	bne	78 <.mpm_cloop>
 3d0:	e59f00d4 	ldr	r0, [pc, #212]	@ 4ac <.mpm_copy2_end+0x10>
 3d4:	e5900000 	ldr	r0, [r0]
 3d8:	e59f20e4 	ldr	r2, [pc, #228]	@ 4c4 <.mpm_copy2_end+0x28>
 3dc:	e5922000 	ldr	r2, [r2]
 3e0:	e59f30e0 	ldr	r3, [pc, #224]	@ 4c8 <.mpm_copy2_end+0x2c>
 3e4:	e5933000 	ldr	r3, [r3]
 3e8:	e0823083 	add	r3, r2, r3, lsl #1
 3ec:	e8bd0010 	ldmfd	sp!, {r4}
 3f0:	e1a0c8ab 	lsr	ip, fp, #17
 3f4:	e1a0c18c 	lsl	ip, ip, #3
 3f8:	e1a0b80b 	lsl	fp, fp, #16
 3fc:	e1a0b8ab 	lsr	fp, fp, #17
 400:	e1a0b18b 	lsl	fp, fp, #3
 404:	e2544001 	subs	r4, r4, #1
 408:	da000023 	ble	49c <.mpm_copy2_end>

0000040c <.mpm_copy2>:
 40c:	e4906004 	ldr	r6, [r0], #4
 410:	e046500b 	sub	r5, r6, fp
 414:	e1a05805 	lsl	r5, r5, #16
 418:	e1b059c5 	asrs	r5, r5, #19
 41c:	e3750080 	cmn	r5, #128	@ 0x80
 420:	b3e0507f 	mvnlt	r5, #127	@ 0x7f
 424:	e355007f 	cmp	r5, #127	@ 0x7f
 428:	c3a0507f 	movgt	r5, #127	@ 0x7f
 42c:	e07b7826 	rsbs	r7, fp, r6, lsr #16
 430:	e1b071c7 	asrs	r7, r7, #3
 434:	e3770080 	cmn	r7, #128	@ 0x80
 438:	b3e0707f 	mvnlt	r7, #127	@ 0x7f
 43c:	e357007f 	cmp	r7, #127	@ 0x7f
 440:	c3a0707f 	movgt	r7, #127	@ 0x7f
 444:	e20550ff 	and	r5, r5, #255	@ 0xff
 448:	e1855407 	orr	r5, r5, r7, lsl #8
 44c:	e0c250b2 	strh	r5, [r2], #2
 450:	e4906004 	ldr	r6, [r0], #4
 454:	e046500c 	sub	r5, r6, ip
 458:	e1a05805 	lsl	r5, r5, #16
 45c:	e1b059c5 	asrs	r5, r5, #19
 460:	e3750080 	cmn	r5, #128	@ 0x80
 464:	b3e0507f 	mvnlt	r5, #127	@ 0x7f
 468:	e355007f 	cmp	r5, #127	@ 0x7f
 46c:	c3a0507f 	movgt	r5, #127	@ 0x7f
 470:	e07c7826 	rsbs	r7, ip, r6, lsr #16
 474:	e1b071c7 	asrs	r7, r7, #3
 478:	e3770080 	cmn	r7, #128	@ 0x80
 47c:	b3e0707f 	mvnlt	r7, #127	@ 0x7f
 480:	e357007f 	cmp	r7, #127	@ 0x7f
 484:	c3a0707f 	movgt	r7, #127	@ 0x7f
 488:	e20550ff 	and	r5, r5, #255	@ 0xff
 48c:	e1855407 	orr	r5, r5, r7, lsl #8
 490:	e0c350b2 	strh	r5, [r3], #2
 494:	e2544002 	subs	r4, r4, #2
 498:	caffffdb 	bgt	40c <.mpm_copy2>

0000049c <.mpm_copy2_end>:
 49c:	e59f0020 	ldr	r0, [pc, #32]	@ 4c4 <.mpm_copy2_end+0x28>
 4a0:	e5802000 	str	r2, [r0]
 4a4:	e8bd4ff0 	pop	{r4, r5, r6, r7, r8, r9, sl, fp, lr}
 4a8:	e12fff1e 	bx	lr
	...

000004cc <mmMix_Skip>:
 4cc:	e0000993 	mul	r0, r3, r9
 4d0:	e0877000 	add	r7, r7, r0
 4d4:	eaffff9c 	b	34c <.mpm_mix_complete>

000004d8 <mmMix_HardLeft>:
 4d8:	eb000008 	bl	500 <mmMix_SingleChannel>
 4dc:	ca00009a 	bgt	74c <mmMix_Remainder>
 4e0:	eaffff99 	b	34c <.mpm_mix_complete>

000004e4 <mmMix_HardRight>:
 4e4:	e1a05006 	mov	r5, r6
 4e8:	e2888004 	add	r8, r8, #4
 4ec:	eb000003 	bl	500 <mmMix_SingleChannel>
 4f0:	e3a05000 	mov	r5, #0
 4f4:	e2488004 	sub	r8, r8, #4
 4f8:	ca000093 	bgt	74c <mmMix_Remainder>
 4fc:	eaffff92 	b	34c <.mpm_mix_complete>

00000500 <mmMix_SingleChannel>:
 500:	e2533008 	subs	r3, r3, #8
 504:	4a000027 	bmi	5a8 <.mpmah_8e>

00000508 <.mpmah_8>:
 508:	e8980806 	ldm	r8, {r1, r2, fp}
 50c:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 510:	e0877009 	add	r7, r7, r9
 514:	e7da4627 	ldrb	r4, [sl, r7, lsr #12]
 518:	e0877009 	add	r7, r7, r9
 51c:	e1800804 	orr	r0, r0, r4, lsl #16
 520:	e0040590 	mul	r4, r0, r5
 524:	e3c4481f 	bic	r4, r4, #2031616	@ 0x1f0000
 528:	e08112a4 	add	r1, r1, r4, lsr #5
 52c:	e4881008 	str	r1, [r8], #8
 530:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 534:	e0877009 	add	r7, r7, r9
 538:	e7da4627 	ldrb	r4, [sl, r7, lsr #12]
 53c:	e0877009 	add	r7, r7, r9
 540:	e1800804 	orr	r0, r0, r4, lsl #16
 544:	e0040590 	mul	r4, r0, r5
 548:	e3c4481f 	bic	r4, r4, #2031616	@ 0x1f0000
 54c:	e08bb2a4 	add	fp, fp, r4, lsr #5
 550:	e488b008 	str	fp, [r8], #8
 554:	e8980806 	ldm	r8, {r1, r2, fp}
 558:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 55c:	e0877009 	add	r7, r7, r9
 560:	e7da4627 	ldrb	r4, [sl, r7, lsr #12]
 564:	e0877009 	add	r7, r7, r9
 568:	e1800804 	orr	r0, r0, r4, lsl #16
 56c:	e0040590 	mul	r4, r0, r5
 570:	e3c4481f 	bic	r4, r4, #2031616	@ 0x1f0000
 574:	e08112a4 	add	r1, r1, r4, lsr #5
 578:	e4881008 	str	r1, [r8], #8
 57c:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 580:	e0877009 	add	r7, r7, r9
 584:	e7da4627 	ldrb	r4, [sl, r7, lsr #12]
 588:	e0877009 	add	r7, r7, r9
 58c:	e1800804 	orr	r0, r0, r4, lsl #16
 590:	e0040590 	mul	r4, r0, r5
 594:	e3c4481f 	bic	r4, r4, #2031616	@ 0x1f0000
 598:	e08bb2a4 	add	fp, fp, r4, lsr #5
 59c:	e488b008 	str	fp, [r8], #8
 5a0:	e2533008 	subs	r3, r3, #8
 5a4:	5affffd7 	bpl	508 <.mpmah_8>

000005a8 <.mpmah_8e>:
 5a8:	e2933008 	adds	r3, r3, #8
 5ac:	e12fff1e 	bx	lr

000005b0 <mmMix_CenteredPanning>:
 5b0:	e2533006 	subs	r3, r3, #6
 5b4:	4a00001e 	bmi	634 <.mpmac_6e>

000005b8 <.mpmac_6>:
 5b8:	e8981856 	ldm	r8, {r1, r2, r4, r6, fp, ip}
 5bc:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 5c0:	e0877009 	add	r7, r7, r9
 5c4:	e7dae627 	ldrb	lr, [sl, r7, lsr #12]
 5c8:	e0877009 	add	r7, r7, r9
 5cc:	e180080e 	orr	r0, r0, lr, lsl #16
 5d0:	e00e0590 	mul	lr, r0, r5
 5d4:	e3cee81f 	bic	lr, lr, #2031616	@ 0x1f0000
 5d8:	e08112ae 	add	r1, r1, lr, lsr #5
 5dc:	e08222ae 	add	r2, r2, lr, lsr #5
 5e0:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 5e4:	e0877009 	add	r7, r7, r9
 5e8:	e7dae627 	ldrb	lr, [sl, r7, lsr #12]
 5ec:	e0877009 	add	r7, r7, r9
 5f0:	e180080e 	orr	r0, r0, lr, lsl #16
 5f4:	e00e0590 	mul	lr, r0, r5
 5f8:	e3cee81f 	bic	lr, lr, #2031616	@ 0x1f0000
 5fc:	e08442ae 	add	r4, r4, lr, lsr #5
 600:	e08662ae 	add	r6, r6, lr, lsr #5
 604:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 608:	e0877009 	add	r7, r7, r9
 60c:	e7dae627 	ldrb	lr, [sl, r7, lsr #12]
 610:	e0877009 	add	r7, r7, r9
 614:	e180080e 	orr	r0, r0, lr, lsl #16
 618:	e00e0590 	mul	lr, r0, r5
 61c:	e3cee81f 	bic	lr, lr, #2031616	@ 0x1f0000
 620:	e08bb2ae 	add	fp, fp, lr, lsr #5
 624:	e08cc2ae 	add	ip, ip, lr, lsr #5
 628:	e8a81856 	stmia	r8!, {r1, r2, r4, r6, fp, ip}
 62c:	e2533006 	subs	r3, r3, #6
 630:	5affffe0 	bpl	5b8 <.mpmac_6>

00000634 <.mpmac_6e>:
 634:	e1a06005 	mov	r6, r5
 638:	e2933006 	adds	r3, r3, #6
 63c:	ca000042 	bgt	74c <mmMix_Remainder>
 640:	eaffff41 	b	34c <.mpm_mix_complete>

00000644 <mmMix_ArbPanning>:
 644:	e253300a 	subs	r3, r3, #10
 648:	4a00003c 	bmi	740 <.mpmaa_10e>

0000064c <.mpmaa_10>:
 64c:	e8984816 	ldm	r8, {r1, r2, r4, fp, lr}
 650:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 654:	e0877009 	add	r7, r7, r9
 658:	e7dac627 	ldrb	ip, [sl, r7, lsr #12]
 65c:	e0877009 	add	r7, r7, r9
 660:	e180080c 	orr	r0, r0, ip, lsl #16
 664:	e00c0590 	mul	ip, r0, r5
 668:	e3ccc81f 	bic	ip, ip, #2031616	@ 0x1f0000
 66c:	e08112ac 	add	r1, r1, ip, lsr #5
 670:	e00c0690 	mul	ip, r0, r6
 674:	e3ccc81f 	bic	ip, ip, #2031616	@ 0x1f0000
 678:	e08222ac 	add	r2, r2, ip, lsr #5
 67c:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 680:	e0877009 	add	r7, r7, r9
 684:	e7dac627 	ldrb	ip, [sl, r7, lsr #12]
 688:	e0877009 	add	r7, r7, r9
 68c:	e180080c 	orr	r0, r0, ip, lsl #16
 690:	e00c0590 	mul	ip, r0, r5
 694:	e3ccc81f 	bic	ip, ip, #2031616	@ 0x1f0000
 698:	e08442ac 	add	r4, r4, ip, lsr #5
 69c:	e00c0690 	mul	ip, r0, r6
 6a0:	e3ccc81f 	bic	ip, ip, #2031616	@ 0x1f0000
 6a4:	e08bb2ac 	add	fp, fp, ip, lsr #5
 6a8:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 6ac:	e0877009 	add	r7, r7, r9
 6b0:	e7dac627 	ldrb	ip, [sl, r7, lsr #12]
 6b4:	e0877009 	add	r7, r7, r9
 6b8:	e180080c 	orr	r0, r0, ip, lsl #16
 6bc:	e00c0590 	mul	ip, r0, r5
 6c0:	e3ccc81f 	bic	ip, ip, #2031616	@ 0x1f0000
 6c4:	e08ee2ac 	add	lr, lr, ip, lsr #5
 6c8:	e8a84816 	stmia	r8!, {r1, r2, r4, fp, lr}
 6cc:	e8984816 	ldm	r8, {r1, r2, r4, fp, lr}
 6d0:	e00c0690 	mul	ip, r0, r6
 6d4:	e3ccc81f 	bic	ip, ip, #2031616	@ 0x1f0000
 6d8:	e08112ac 	add	r1, r1, ip, lsr #5
 6dc:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 6e0:	e0877009 	add	r7, r7, r9
 6e4:	e7dac627 	ldrb	ip, [sl, r7, lsr #12]
 6e8:	e0877009 	add	r7, r7, r9
 6ec:	e180080c 	orr	r0, r0, ip, lsl #16
 6f0:	e00c0590 	mul	ip, r0, r5
 6f4:	e3ccc81f 	bic	ip, ip, #2031616	@ 0x1f0000
 6f8:	e08222ac 	add	r2, r2, ip, lsr #5
 6fc:	e00c0690 	mul	ip, r0, r6
 700:	e3ccc81f 	bic	ip, ip, #2031616	@ 0x1f0000
 704:	e08442ac 	add	r4, r4, ip, lsr #5
 708:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 70c:	e0877009 	add	r7, r7, r9
 710:	e7dac627 	ldrb	ip, [sl, r7, lsr #12]
 714:	e0877009 	add	r7, r7, r9
 718:	e180080c 	orr	r0, r0, ip, lsl #16
 71c:	e00c0590 	mul	ip, r0, r5
 720:	e3ccc81f 	bic	ip, ip, #2031616	@ 0x1f0000
 724:	e08bb2ac 	add	fp, fp, ip, lsr #5
 728:	e00c0690 	mul	ip, r0, r6
 72c:	e3ccc81f 	bic	ip, ip, #2031616	@ 0x1f0000
 730:	e08ee2ac 	add	lr, lr, ip, lsr #5
 734:	e8a84816 	stmia	r8!, {r1, r2, r4, fp, lr}
 738:	e253300a 	subs	r3, r3, #10
 73c:	5affffc2 	bpl	64c <.mpmaa_10>

00000740 <.mpmaa_10e>:
 740:	e293300a 	adds	r3, r3, #10
 744:	ca000000 	bgt	74c <mmMix_Remainder>
 748:	eafffeff 	b	34c <.mpm_mix_complete>

0000074c <mmMix_Remainder>:
 74c:	e185b806 	orr	fp, r5, r6, lsl #16

00000750 <.mix_remaining>:
 750:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 754:	e0877009 	add	r7, r7, r9
 758:	e004009b 	mul	r4, fp, r0
 75c:	e1d810b0 	ldrh	r1, [r8]
 760:	e3c428ff 	bic	r2, r4, #16711680	@ 0xff0000
 764:	e08112a2 	add	r1, r1, r2, lsr #5
 768:	e0c810b2 	strh	r1, [r8], #2
 76c:	e1d810b2 	ldrh	r1, [r8, #2]
 770:	e0811aa4 	add	r1, r1, r4, lsr #21
 774:	e1c810b2 	strh	r1, [r8, #2]
 778:	e2533002 	subs	r3, r3, #2
 77c:	ba00000a 	blt	7ac <.end_mix_remaining>
 780:	e7da0627 	ldrb	r0, [sl, r7, lsr #12]
 784:	e0877009 	add	r7, r7, r9
 788:	e004009b 	mul	r4, fp, r0
 78c:	e1d810b0 	ldrh	r1, [r8]
 790:	e3c428ff 	bic	r2, r4, #16711680	@ 0xff0000
 794:	e08112a2 	add	r1, r1, r2, lsr #5
 798:	e0c810b4 	strh	r1, [r8], #4
 79c:	e1d810b0 	ldrh	r1, [r8]
 7a0:	e0811aa4 	add	r1, r1, r4, lsr #21
 7a4:	e0c810b2 	strh	r1, [r8], #2
 7a8:	caffffe8 	bgt	750 <.mix_remaining>

000007ac <.end_mix_remaining>:
 7ac:	eafffee6 	b	34c <.mpm_mix_complete>

Disassembly of section .ARM.attributes:

00000000 <.ARM.attributes>:
   0:	00001941 	andeq	r1, r0, r1, asr #18
   4:	61656100 	cmnvs	r5, r0, lsl #2
   8:	01006962 	tsteq	r0, r2, ror #18
   c:	0000000f 	andeq	r0, r0, pc
  10:	00543405 	subseq	r3, r4, r5, lsl #8
  14:	01080206 	tsteq	r8, r6, lsl #4
  18:	Address 0x18 is out of bounds.

