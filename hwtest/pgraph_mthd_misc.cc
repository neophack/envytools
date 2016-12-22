/*
 * Copyright (C) 2016 Marcin Kościelnicki <koriakin@0x04.net>
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "pgraph.h"
#include "pgraph_mthd.h"
#include "nva.h"

namespace hwtest {
namespace pgraph {

namespace {

class MthdCtxSwitchTest : public MthdTest {
	bool special_notify() override {
		return true;
	}
	void adjust_orig_mthd() override {
		if (chipset.card_type < 3) {
			insrt(orig.notify, 16, 1, 0);
		}
	}
	void choose_mthd() override {
		if (chipset.card_type < 3) {
			int classes[20] = {
				0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
				0x08, 0x09, 0x0a, 0x0b, 0x0c,
				0x0d, 0x0e, 0x1d, 0x1e,
				0x10, 0x11, 0x12, 0x13, 0x14,
			};
			cls = classes[rnd() % 20];
		} else {
			cls = rnd() & 0x1f;
		}
		mthd = 0;
	}
	void emulate_mthd() override {
		// For NV3 and up, handled in MthdTest core.
		if (chipset.card_type < 3) {
			bool chsw = false;
			int och = extr(exp.ctx_switch[0], 16, 7);
			int nch = extr(val, 16, 7);
			if ((val & 0x007f8000) != (exp.ctx_switch[0] & 0x007f8000))
				chsw = true;
			if (!extr(exp.ctx_control, 16, 1))
				chsw = true;
			bool volatile_reset = extr(val, 31, 1) && extr(exp.debug[2], 28, 1) && (!extr(exp.ctx_control, 16, 1) || och == nch);
			if (chsw) {
				exp.ctx_control |= 0x01010000;
				exp.intr |= 0x10;
				exp.access &= ~0x101;
			} else {
				exp.ctx_control &= ~0x01000000;
			}
			insrt(exp.access, 12, 5, cls);
			insrt(exp.debug[1], 0, 1, volatile_reset);
			if (volatile_reset) {
				pgraph_volatile_reset(&exp);
			}
			exp.ctx_switch[0] = val & 0x807fffff;
			if (exp.notify & 0x100000) {
				exp.intr |= 0x10000001;
				exp.invalid |= 0x10000;
				exp.access &= ~0x101;
				exp.notify &= ~0x100000;
			}
		}
	}
public:
	MthdCtxSwitchTest(hwtest::TestOptions &opt, uint32_t seed) : MthdTest(opt, seed) {}
};

class MthdNotifyTest : public MthdTest {
	bool special_notify() override {
		return true;
	}
	void adjust_orig_mthd() override {
		if (!(rnd() & 3)) {
			insrt(orig.notify, 0, 16, 0);
		}
	}
	void choose_mthd() override {
		if (rnd() & 1) {
			val &= 0x1f;
		}
		if (chipset.card_type < 3) {
			int classes[20] = {
				0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
				0x08, 0x09, 0x0a, 0x0b, 0x0c,
				0x0d, 0x0e, 0x1d, 0x1e,
				0x10, 0x11, 0x12, 0x13, 0x14,
			};
			cls = classes[rnd() % 20];
		} else {
			int classes[22] = {
				0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
				0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
				0x0d, 0x0e, 0x10, 0x11, 0x12, 0x14,
				0x15, 0x17, 0x18, 0x1c,
			};
			cls = classes[rnd() % 22];
		}
		mthd = 0x104;
	}
	bool is_valid_val() override {
		if (chipset.card_type < 3) {
			if ((cls & 0xf) == 0xd || (cls & 0xf) == 0xe)
				return true;
			if (val != 0)
				return false;
		} else {
			if (val & ~0xf)
				return false;
		}
		return true;
	}
	void emulate_mthd() override {
		if (chipset.card_type < 3) {
			if (exp.notify & 0x100000 && !exp.invalid)
				exp.intr |= 0x10000000;
			if (!(exp.ctx_switch[0] & 0x100))
				exp.invalid |= 0x100;
			if (exp.notify & 0x110000)
				exp.invalid |= 0x1000;
			if (exp.invalid) {
				exp.intr |= 1;
				exp.access &= ~0x101;
			} else {
				exp.notify |= 0x10000;
			}
		} else {
			if (!extr(exp.invalid, 16, 1)) {
				if (extr(exp.notify, 16, 1)) {
					exp.intr |= 1;
					exp.invalid |= 0x1000;
					exp.fifo_enable = 0;
				}
			}
			if (!extr(exp.invalid, 16, 1) && !extr(exp.invalid, 4, 1)) {
				insrt(exp.notify, 16, 1, 1);
				insrt(exp.notify, 20, 4, val);
			}
		}
	}
public:
	MthdNotifyTest(hwtest::TestOptions &opt, uint32_t seed) : MthdTest(opt, seed) {}
};

}

void MthdNotify::adjust_orig_mthd() {
	if (!(rnd() & 3)) {
		if (chipset.card_type < 4) {
			insrt(orig.notify, 0, 16, 0);
		} else {
			insrt(orig.ctx_switch[1], 16, 16, 0);
		}
	}
}

bool MthdNotify::is_valid_val() {
	if (chipset.card_type < 3) {
		if ((cls & 0xf) == 0xd || (cls & 0xf) == 0xe)
			return true;
		return val == 0;
	} else if (chipset.card_type < 4) {
		return val < 0x10;
	} else {
		return val < 2;
	}
}

void MthdNotify::emulate_mthd() {
	if (chipset.card_type < 3) {
		if (exp.notify & 0x100000 && !exp.invalid)
			exp.intr |= 0x10000000;
		if (!(exp.ctx_switch[0] & 0x100))
			exp.invalid |= 0x100;
		if (exp.notify & 0x110000)
			exp.invalid |= 0x1000;
		if (exp.invalid) {
			exp.intr |= 1;
			exp.access &= ~0x101;
		} else {
			exp.notify |= 0x10000;
		}
	} else if (chipset.card_type < 4) {
		if (!extr(exp.invalid, 16, 1)) {
			if (extr(exp.notify, 16, 1)) {
				exp.intr |= 1;
				exp.invalid |= 0x1000;
				exp.fifo_enable = 0;
			}
		}
		if (!extr(exp.invalid, 16, 1) && !extr(exp.invalid, 4, 1)) {
			insrt(exp.notify, 16, 1, 1);
			insrt(exp.notify, 20, 4, val);
		}
	} else {
		int rval = val & 1;
		if (chipset.card_type >= 0x10)
			rval = val & 3;
		if (extr(exp.notify, 16, 1))
			nv04_pgraph_blowup(&exp, 0x1000);
		if (!extr(exp.ctx_switch[1], 16, 16))
			pgraph_state_error(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			insrt(exp.notify, 16, 1, rval < 2);
			insrt(exp.notify, 20, 1, rval & 1);
		}
	}
}


bool PGraphMthdMiscTests::supported() {
	return chipset.card_type < 4;
}

Test::Subtests PGraphMthdMiscTests::subtests() {
	return {
		{"ctx_switch", new MthdCtxSwitchTest(opt, rnd())},
		{"notify", new MthdNotifyTest(opt, rnd())},
	};
}

}
}
