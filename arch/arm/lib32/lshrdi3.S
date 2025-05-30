/* Copyright 1995, 1996, 1998, 1999, 2000, 2003, 2004, 2005
   Free Software Foundation, Inc.

This file is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

In addition to the permissions in the GNU General Public License, the
Free Software Foundation gives you unlimited permission to link the
compiled version of this file into combinations with other programs,
and to distribute those combinations without any restriction coming
from the use of this file.  (The General Public License restrictions
do apply in other respects; for example, they cover modification of
the file, and distribution when not linked into a combine
executable.)

This file is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

*/

#include <linux/linkage.h>
#include <asm/assembler.h>

#ifdef __ARMEB__
#define al r1
#define ah r0
#else
#define al r0
#define ah r1
#endif

.section .text.__lshrdi3
ENTRY(__lshrdi3)
ENTRY(__aeabi_llsr)

	subs	r3, r2, #32
	rsb	ip, r2, #32
	movmi	al, al, lsr r2
	movpl	al, ah, lsr r3
 ARM(	orrmi	al, al, ah, lsl ip	)
 THUMB(	lslmi	r3, ah, ip		)
 THUMB(	orrmi	al, al, r3		)
	mov	ah, ah, lsr r2
	ret	lr

ENDPROC(__lshrdi3)
ENDPROC(__aeabi_llsr)
