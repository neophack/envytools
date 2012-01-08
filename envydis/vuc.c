/*
 * Copyright (C) 2010-2011 Marcin Kościelnicki <koriakin@0x04.net>
 * Copyright (C) 2011 Paweł Czaplejewicz <pcz@porcupinefactory.org>
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "dis-intern.h"

#define VP2 1
#define VP3 2

/*
 * Code target field
 *
 * This field represents a code address and is used for branching and the
 * likes. Target is counted in 32-bit words from the start of microcode.
 */

static struct bitfield ctargoff = { 8, 11 };
static struct bitfield rbrtargoff = { 34, 6, .pcrel = 1 };
#define BTARG atombtarg, &ctargoff
#define CTARG atomctarg, &ctargoff
#define RBRTARG atombtarg, &rbrtargoff

static struct bitfield imm4off = { 12, 4 };
static struct bitfield imm6off = { { 12, 4, 24, 2 } };
static struct bitfield imm12off = { { 8, 8, 20, 4 } };
static struct bitfield imm14off = { { 8, 8, 20, 6 } };
static struct bitfield bimmldoff = { { 12, 4, 20, 6 } };
static struct bitfield bimmldsoff = { { 12, 4, 24, 2 } };
static struct bitfield bimmstoff = { 16, 10 };
static struct bitfield bimmstsoff = { 16, 4, 24, 2 };
#define IMM4 atomimm, &imm4off
#define IMM6 atomimm, &imm6off
#define IMM12 atomimm, &imm12off
#define IMM14 atomimm, &imm14off

/*
 * Register fields
 */

static struct sreg reg_sr[] = {
	{ 0, 0, SR_ZERO },
	{ -1 },
};
static struct sreg pred_sr[] = {
	{ 1, "np0" },
	{ 15, 0, SR_ONE },
	{ -1 },
};
static struct sreg sreg_sr[] = {
    { 2, "pidx" },
    { 4, "h2v" },
    { 5, "v2h" },
    { 6, "stat" },
    { 7, "parm" },
    { 8, "pc" },
    { 10, "cstop" },
    { 12, "arthi" },
    { 13, "artlo" },
    { 14, "pred" },
    { 15, "icnt" },
    { 16, "mvxl0" },
    { 17, "mvyl0" },
    { 18, "mvxl1" },
    { 19, "mvyl1" },
    { 20, "refl0" },
    { 21, "refl1" },
    { 24, "mbflags" },
    { 25, "qpy" },
    { 26, "qpc" },
    { 27, "mbpart" },
    { 28, "mbxy" },
    { 29, "mbaddr" },
    { 30, "mbtype" },
    { 31, "submbtype" },
    { -1 },
};

static struct bitfield src1_bf = { 8, 4 };
static struct bitfield srsrc_bf = { 8, 4, 24, 2 };
static struct bitfield src2_bf = { 12, 4 };
static struct bitfield dst_bf = { 16, 4 };
static struct bitfield srdst_bf = { 16, 4, 24, 2 };
static struct bitfield psrc1_bf = { 8, 4 };
static struct bitfield psrc2_bf = { 12, 4 };
static struct bitfield pdst_bf = { 16, 4 };
static struct bitfield pred_bf = { 20, 4 };
static struct bitfield rbrpred_bf = { 30, 3, .addend = 8 };
static struct reg src1_r = { &src1_bf, "r", .specials = reg_sr };
static struct reg src2_r = { &src2_bf, "r", .specials = reg_sr };
static struct reg dst_r = { &dst_bf, "r", .specials = reg_sr };
static struct reg srdst_r = { &srdst_bf, "sr", .specials = sreg_sr, .cool = 1 };
static struct reg srsrc_r = { &srsrc_bf, "sr", .specials = sreg_sr, .cool = 1 };
static struct reg psrc1_r = { &psrc1_bf, "p", .cool = 1, .specials = pred_sr };
static struct reg psrc2_r = { &psrc2_bf, "p", .cool = 1, .specials = pred_sr };
static struct reg pdst_r = { &pdst_bf, "p", .cool = 1, .specials = pred_sr };
static struct reg pred_r = { &pred_bf, "p", .cool = 1, .specials = pred_sr };
static struct reg rbrpred_r = { &rbrpred_bf, "p", .cool = 1, .specials = pred_sr };
#define SRC1 atomreg, &src1_r
#define SRC2 atomreg, &src2_r
#define DST atomreg, &dst_r
#define SRSRC atomreg, &srsrc_r
#define SRDST atomreg, &srdst_r
#define PSRC1 atomreg, &psrc1_r
#define PSRC2 atomreg, &psrc2_r
#define PDST atomreg, &pdst_r
#define PRED atomreg, &pred_r
#define RBRPRED atomreg, &rbrpred_r

static struct mem dmemldi_m = { "D", 0, &src1_r, &bimmldoff };
static struct mem dmemldis_m = { "D", 0, &src1_r, &bimmldsoff };
static struct mem dmemldr_m = { "D", 0, &src1_r, 0, &src2_r };
static struct mem dmemsti_m = { "D", 0, &src1_r, &bimmstoff };
static struct mem dmemstis_m = { "D", 0, &src1_r, &bimmstsoff };
static struct mem dmemstr_m = { "D", 0, &src1_r, 0, &dst_r };
#define DMEMLDI atommem, &dmemldi_m
#define DMEMLDIS atommem, &dmemldis_m
#define DMEMLDR atommem, &dmemldr_m
#define DMEMSTI atommem, &dmemsti_m
#define DMEMSTIS atommem, &dmemstis_m
#define DMEMSTR atommem, &dmemstr_m

static struct mem b6memldi_m = { "B6", 0, &src1_r, &bimmldoff };
static struct mem b6memldis_m = { "B6", 0, &src1_r, &bimmldsoff };
static struct mem b6memldr_m = { "B6", 0, &src1_r, 0, &src2_r };
static struct mem b6memsti_m = { "B6", 0, &src1_r, &bimmstoff };
static struct mem b6memstis_m = { "B6", 0, &src1_r, &bimmstsoff };
static struct mem b6memstr_m = { "B6", 0, &src1_r, 0, &dst_r };
#define B6MEMLDI atommem, &b6memldi_m
#define B6MEMLDIS atommem, &b6memldis_m
#define B6MEMLDR atommem, &b6memldr_m
#define B6MEMSTI atommem, &b6memsti_m
#define B6MEMSTIS atommem, &b6memstis_m
#define B6MEMSTR atommem, &b6memstr_m

static struct mem b7memldi_m = { "B7", 0, &src1_r, &bimmldoff };
static struct mem b7memldis_m = { "B7", 0, &src1_r, &bimmldsoff };
static struct mem b7memldr_m = { "B7", 0, &src1_r, 0, &src2_r };
static struct mem b7memsti_m = { "B7", 0, &src1_r, &bimmstoff };
static struct mem b7memstis_m = { "B7", 0, &src1_r, &bimmstsoff };
static struct mem b7memstr_m = { "B7", 0, &src1_r, 0, &dst_r };
#define B7MEMLDI atommem, &b7memldi_m
#define B7MEMLDIS atommem, &b7memldis_m
#define B7MEMLDR atommem, &b7memldr_m
#define B7MEMSTI atommem, &b7memsti_m
#define B7MEMSTIS atommem, &b7memstis_m
#define B7MEMSTR atommem, &b7memstr_m

static struct mem i1memldi_m = { "I1", 0, &src1_r, &bimmldoff };
static struct mem i1memldis_m = { "I1", 0, &src1_r, &bimmldsoff };
static struct mem i1memldr_m = { "I1", 0, &src1_r, 0, &src2_r };
#define I1MEMLDI atommem, &i1memldi_m
#define I1MEMLDIS atommem, &i1memldis_m
#define I1MEMLDR atommem, &i1memldr_m

static struct mem o2memsti_m = { "O2", 0, &src1_r, &bimmstoff };
static struct mem o2memstis_m = { "O2", 0, &src1_r, &bimmstsoff };
static struct mem o2memstr_m = { "O2", 0, &src1_r, 0, &dst_r };
#define O2MEMSTI atommem, &o2memsti_m
#define O2MEMSTIS atommem, &o2memstis_m
#define O2MEMSTR atommem, &o2memstr_m

static struct mem o5memsti_m = { "O5", 0, &src1_r, &bimmstoff };
static struct mem o5memstis_m = { "O5", 0, &src1_r, &bimmstsoff };
static struct mem o5memstr_m = { "O5", 0, &src1_r, 0, &dst_r };
#define O5MEMSTI atommem, &o5memsti_m
#define O5MEMSTIS atommem, &o5memstis_m
#define O5MEMSTR atommem, &o5memstr_m

static struct mem i4memldi_m = { "I4", 0, &src1_r, &bimmldoff };
static struct mem i4memldis_m = { "I4", 0, &src1_r, &bimmldsoff };
static struct mem i4memldr_m = { "I4", 0, &src1_r, 0, &src2_r };
#define I4MEMLDI atommem, &i4memldi_m
#define I4MEMLDIS atommem, &i4memldis_m
#define I4MEMLDR atommem, &i4memldr_m

static struct insn tabp[] = {
	{ 0x00f00000, 0x00f00000 },
	{ 0, 0, PRED },
};

// tabdst and tabsrc1 are used only with base instructions
static struct insn tabdst[] = {
	{ 0x00000000, 0x10000000, DST },
	{ 0x10000000, 0x14000000, SRDST }, // only one of dst, src1 can be $sr
	{ 0, 0, OOPS },
};

static struct insn tabsrc1[] = {
	{ 0x00000000, 0x04000000, SRC1 },
	{ 0x04000000, 0x14000000, SRSRC }, // only one of dst, src1 can be $sr
	{ 0, 0, OOPS },
};

static struct insn tabsrc2[] = {
	{ 0x00000000, 0x08000000, SRC2 },
	{ 0x08000000, 0x1c000000, IMM6 },
	{ 0x18000000, 0x1c000000, IMM4 },
	{ 0x0c000000, 0x0c000000, IMM4 },
	{ 0, 0, OOPS },
};

static struct insn tabarithsrc2[] = {
    { 0x00000000, 0x08000000, SRC2 },
    { 0x08000000, 0x08000000, IMM6 },
    { 0, 0, OOPS },
};

// Tables describing parameters for "set" functions
static struct insn tabpdst[] = {
    { 0x20000000, 0x20000000, PDST },
    { 0x00000000, 0x20000000, PRED },
    { 0, 0, OOPS },
};

static struct insn tabpmod[] = {
    { 0x00000060, 0x00000060 }, // don't modify predicate
    
    { 0x00000000, 0x000000e0, N("pand"), T(pdst) },
    { 0x00000020, 0x000000e0, N("por"), T(pdst) },
    { 0x00000040, 0x000000e0, T(pdst) },
    { 0x00000080, 0x000000e0, N("pandn"), T(pdst) },
    { 0x000000a0, 0x000000e0, N("porn"), T(pdst) },
    { 0x000000c0, 0x000000e0, N("pnot"), T(pdst) },
    { 0, 0, OOPS },
};


static struct insn tabovr[] = {
    { 0x3c000000, 0x3c0000ff, N("bra"), BTARG },
    { 0x3c000002, 0x3c0000ff, N("call"), CTARG },
    { 0x34000003, 0x3c0000ff, N("ret") },
    { 0x14000004, 0x140000ff, N("sleep") },	/* halt everything until there's work to do */
    { 0x14000005, 0x140000ff, N("wstc"), T(src2) },	/* do nothing until bit #SRC2 of $stat is clear */
    { 0x14000006, 0x140000ff, U("o06"), T(src2) },

    { 0x14000020, 0x140000ff, N("clicnt") },	/* reset $icnt to 0 */
    { 0x14000021, 0x140000ff, U("o21") }, /* ADV */
    { 0x14000022, 0x140000ff, U("o22") }, /* ADV */
    { 0x14000023, 0x140000ff, U("o23") }, /* VP2 */
    { 0x14000024, 0x140000ff, N("mbiread") },	/* read data for current macroblock into registers/memory */
    { 0x14000025, 0x140000ff, U("o25") }, /* ANEW */
    { 0x14000026, 0x140000ff, U("o26") }, /* ANEW */
    { 0x14000028, 0x140000ff, N("mbinext") },	/* advance input to the next macroblock */
    { 0x14000029, 0x140000ff, N("mvsread") },	/* read data for current macroblock/macroblock pair from input MVSURF */
    { 0x1400002a, 0x140000ff, N("mvswrite") },	/* write data for current macroblock/macroblock pair to output MVSURF */
    { 0x1400002b, 0x140000ff, U("o2b") }, /* ADV */
    { 0x1400002c, 0x140000ff, U("o2c") }, /* ANEW */
    
    { 0x14000040, 0x1c0000ff, N("setand"), T(pdst), PSRC1, PSRC2 },
    { 0x14000041, 0x1c0000ff, N("setor"), T(pdst), PSRC1, PSRC2 },
    { 0x14000042, 0x1c0000ff, N("setne"), T(pdst), PSRC1, PSRC2 },
    { 0x14000043, 0x1c0000ff, N("nop") },
    { 0x14000044, 0x1c0000ff, N("setl"), T(pdst), PSRC1, PSRC2 },
    { 0x14000045, 0x1c0000ff, N("setge"), T(pdst), PSRC1, PSRC2 },
    { 0x14000046, 0x1c0000ff, N("sete"), T(pdst), PSRC1, PSRC2 },
    
    { 0x14000048, 0x140000ff, N("setg"), T(pdst), PSRC1, PSRC2 }, // signed
    { 0x14000049, 0x140000ff, N("setle"), T(pdst), PSRC1, PSRC2 },
    { 0x1400004a, 0x140000ff, N("sete"), T(pdst), PSRC1, PSRC2 },

    { 0x1400004c, 0x140000ff, N("setnor"), T(pdst), PSRC1, PSRC2 },
    { 0x1400004d, 0x140000ff, N("setnand"), T(pdst), PSRC1, PSRC2 },
    { 0x1400004e, 0x140000ff, N("setne"), T(pdst), PSRC1, PSRC2 },
    
    { 0x1c000080, 0x3c0000ff, N("st"), DMEMSTI, SRC2 },
    { 0x3c000080, 0x3c0000ff, N("st"), DMEMSTIS, SRC2 },
    { 0x14000080, 0x1c0000ff, N("st"), DMEMSTR, SRC2 },
    { 0x14000081, 0x1c0000ff, N("ld"), DST, DMEMLDR },
    { 0x1c000081, 0x3c0000ff, N("ld"), DST, DMEMLDI },
    { 0x3c000081, 0x3c0000ff, N("ld"), DST, DMEMLDIS },
    { 0x14000083, 0x1c0000ff, N("ld1"), DST, I1MEMLDR },
    { 0x1c000083, 0x3c0000ff, N("ld1"), DST, I1MEMLDI },
    { 0x3c000083, 0x3c0000ff, N("ld1"), DST, I1MEMLDIS },
    { 0x1c000084, 0x3c0000ff, N("st2"), O2MEMSTI, SRC2 },
    { 0x3c000084, 0x3c0000ff, N("st2"), O2MEMSTIS, SRC2 },
    { 0x14000084, 0x1c0000ff, N("st2"), O2MEMSTR, SRC2 },
    { 0x14000089, 0x1c0000ff, N("ld4"), DST, I4MEMLDR },
    { 0x1c000089, 0x3c0000ff, N("ld4"), DST, I4MEMLDI },
    { 0x3c000089, 0x3c0000ff, N("ld4"), DST, I4MEMLDIS },
    { 0x1c00008a, 0x3c0000ff, N("st5"), O5MEMSTI, SRC2 },
    { 0x3c00008a, 0x3c0000ff, N("st5"), O5MEMSTIS, SRC2 },
    { 0x1400008a, 0x1c0000ff, N("st5"), O5MEMSTR, SRC2 },
    { 0x1c00008c, 0x3c0000ff, N("st6"), B6MEMSTI, SRC2 },
    { 0x3c00008c, 0x3c0000ff, N("st6"), B6MEMSTIS, SRC2 },
    { 0x3400008c, 0x1c0000ff, N("st6"), B6MEMSTR, SRC2 },
    { 0x1400008d, 0x1c0000ff, N("ld6"), DST, B6MEMLDR },
    { 0x1c00008d, 0x3c0000ff, N("ld6"), DST, B6MEMLDI },
    { 0x3c00008d, 0x3c0000ff, N("ld6"), DST, B6MEMLDIS },
    { 0x1c00008e, 0x3c0000ff, N("st7"), B7MEMSTI, SRC2 },
    { 0x3c00008e, 0x3c0000ff, N("st7"), B7MEMSTIS, SRC2 },
    { 0x1400008e, 0x1c0000ff, N("st7"), B7MEMSTR, SRC2 },
    { 0x1400008f, 0x1c0000ff, N("ld7"), DST, B7MEMLDR },
    { 0x1c00008f, 0x3c0000ff, N("ld7"), DST, B7MEMLDI },
    { 0x3c00008f, 0x3c0000ff, N("ld7"), DST, B7MEMLDIS },

    // arithmetic subsystem block
    { 0x140000a0, 0x140000ff, N("mul"), SRC1, T(arithsrc2) },
    { 0x140000a1, 0x140000ff, N("muls"), SRC1, T(arithsrc2) },
    { 0x140000a2, 0x140000ff, N("shift"), T(arithsrc2) },
    { 0x140000a4, 0x140000ff, U("oa4"), T(src2) }, /* VC1 */
    { 0x140000a8, 0x140000ff, U("oa8"), T(src2) }, /* VC1 */
    { 0x140000ac, 0x140000ff, U("oac"), T(src2) }, /* 263 */
    
    { 0, 0, OOPS },
};

static struct insn tabm[] = {
    // Desired forms for instructions with overlapping predicate
    { 0x00000060, 0x2000007f, N("slct"), T(dst), PRED, T(src1), T(src2) }, // dst = PRED ? src1 : src2. Can set predicate: DST is odd
    
    { 0x00000001, 0x0800001f, N("mov"), T(pmod), T(dst), T(src2) },
    { 0x18000061, 0x1800007f, N("mov"), SRDST, IMM12 },
    { 0x08000061, 0x1800007f, N("mov"), DST, IMM14 }, // mov can set predicate: DST is odd
    
    // General forms for instructions with overlaps
    { 0x00000000, 0x0000001f, U("slct"), T(pmod), T(dst), T(pdst), T(src1), T(src2) }, // XXX: source and destination predicate is the same
    
    { 0x18000001, 0x1800001f, U("mov"), T(pmod), SRDST, IMM12 },
    { 0x08000001, 0x1800001f, U("mov"), T(pmod), DST, IMM14 }, // overlap: in immediate form bits 9:12 of immediate are same as destination predicate
    
    // all accepted forms
    { 0x00000004, 0x0000001f, N("add"), T(pmod), T(dst), T(src1), T(src2) }, // set pred if result is odd
    { 0x00000005, 0x0000001f, N("sub"), T(pmod), T(dst), T(src1), T(src2) },// set pred if result is odd
    { 0x00000006, 0x0000001f, N("subr"), T(pmod), T(dst), T(src1), T(src2), .vartype = VP2 }, // set pred if result is odd

    
    { 0x00000008, 0x0000001f, N("setsg"), T(pmod), T(src1), T(src2) },
    { 0x00000009, 0x0000001f, N("setsl"), T(pmod), T(src1), T(src2) },
    { 0x0000000a, 0x0000001f, N("setse"), T(pmod), T(src1), T(src2) },    
    { 0x0000000b, 0x000000ff, N("setsle"), T(pmod), T(src1), T(src2) },
    
    { 0x0000000c, 0x0000001f, N("minsz"), T(pmod), T(dst), T(src1), T(src2) }, // (a > b) ? b : max(a, 0), PRED := SRC1 > SRC2
    { 0x0000000d, 0x0000001f, N("clampsex"), T(pmod), T(dst), T(src1), T(src2) }, // clamp to -2^b..2^b-1, PRED := not clamped
    { 0x0000000e, 0x0000001f, N("sex"), T(pmod), T(dst), T(src1), T(src2) }, // PRED := no change
    
    { 0x0000000f, 0x0000001f, N("setzero"), T(pmod), T(src1), T(src2), .vartype = VP2 }, // PRED := is zero
    
    { 0x00000010, 0x0000001f, N("bset"), T(pmod), T(dst), T(src1), T(src2) }, // PRED := even
    { 0x00000011, 0x0000001f, N("bclr"), T(pmod), T(dst), T(src1), T(src2) }, // PRED := odd
    { 0x00000012, 0x0000001f, N("btest"), T(pmod), T(src1), T(src2) },
    
    
    { 0x00000014, 0x0000001f, N("rot8"), T(pmod), T(dst), T(src1) }, // PRED := odd
    { 0x00000015, 0x0000001f, N("shl"), T(pmod), T(dst), T(src1), T(src2) }, // pdst becomes last bit shifted out
    { 0x00000016, 0x0000001f, N("shr"), T(pmod), T(dst), T(src1), T(src2) }, // pdst becomes last bit shifted out
    { 0x00000017, 0x0000001f, N("sar"), T(pmod), T(dst), T(src1), T(src2) }, // pdst becomes last bit shifted out
    
    { 0x00000018, 0x0000001f, N("and"), T(pmod), T(dst), T(src1), T(src2) }, /* PRED := odd */
    { 0x00000019, 0x0000001f, N("or"), T(pmod), T(dst), T(src1), T(src2) }, // PRED := even
    { 0x0000001a, 0x0000001f, N("xor"), T(pmod), T(dst), T(src1), T(src2) }, // PRED := odd
    { 0x0000001b, 0x0000001f, N("not"), T(pmod), T(dst), T(src1), T(src2) }, // PRED := odd
    { 0x0000001c, 0x0000001f, N("lut"), T(pmod), T(dst), T(src1), T(src2) }, /* 264, VC1 */
     
     
    // Opcodes with flag bits not verified
	{ 0x00000066, 0x000000ff, N("avgs"), T(dst), T(src1), T(src2), .vartype = VP3 }, // (a+b)/2, rounding UP, signed
	{ 0x00000067, 0x000000ff, N("avgu"), T(dst), T(src1), T(src2), .vartype = VP3 }, // (a+b)/2, rounding UP, unsigned
	{ 0x0000006f, 0x000000ff, N("div2s"), T(dst), T(src1), .vartype = VP3 }, // signed div by 2, round to 0. Not present on vp2?
	{ 0x0000007d, 0x000000ff, N("min"), T(dst), T(src1), T(src2), .vartype = VP3 },
	{ 0x0000007e, 0x000000ff, N("max"), T(dst), T(src1), T(src2), .vartype = VP3},
    
	{ 0, 0, OOPS },
};

static struct insn tabops[] = {
	{ 0x34000043, 0x3c0000ff, N("nop") },
	{ 0x14000000, 0x34000000, T(ovr) },
    { 0x34000000, 0x34000000, T(p), T(ovr) },
	{ 0x00000000, 0x20000000, T(m) },
	{ 0x20000000, 0x20000000, T(p), T(m) },
	{ 0, 0, OOPS },
};

static struct insn tabroot[] = {
	{ 0, 0, OP64, T(ops), .vartype = VP3 },
	{ 0xffc0000000ull, 0xffc0000000ull, OP64, T(ops), .vartype = VP2 },
	{ 0x0000000000ull, 0x0200000000ull, OP64, RBRPRED, N("rbra"), RBRTARG, T(ops), .vartype = VP2 },
	{ 0x0200000000ull, 0x0200000000ull, OP64, N("not"), RBRPRED, N("rbra"), RBRTARG, T(ops), .vartype = VP2 },
	{ 0, 0, OOPS },
};

static const struct disvariant vuc_vars[] = {
    "vp2", VP2,
    "vp3", VP3,
};

const struct disisa vuc_isa_s = {
	tabroot,
	8,
	8,
	8,
    .vars = vuc_vars,
    .varsnum = sizeof vuc_vars / sizeof *vuc_vars,
};
