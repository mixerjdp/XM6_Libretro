//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Render Fast / px68k-style scanline compositor.
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "crtc.h"
#include "vc.h"
#include "sprite.h"
#include "rend_soft.h"
#include "render.h"

#include <cstring>

namespace {

static const DWORD k_render_color0 = 0x80000000u;
static const DWORD k_px68k_ibit_marker = 0x40000000u;

static inline BOOL FastVisiblePixel(DWORD pixel)
{
	return (BOOL)((pixel & k_render_color0) == 0);
}

static inline WORD Px68kPack565I(DWORD pixel)
{
	if (pixel & k_render_color0) {
		return 0;
	}

	const DWORD rgb = pixel & 0x00ffffffu;
	const DWORD r8 = (rgb >> 16) & 0xffu;
	const DWORD g8 = (rgb >> 8) & 0xffu;
	const DWORD b8 = (rgb >> 0) & 0xffu;
	const WORD r5 = (WORD)(r8 >> 3);
	const WORD g5 = (WORD)(g8 >> 3);
	const WORD b5 = (WORD)(b8 >> 3);
	WORD out = (WORD)((r5 << 11) | (g5 << 6) | (b5 << 0));
	if (pixel & k_px68k_ibit_marker) {
		out |= (WORD)0x0020;
	}
	return out;
}

static inline DWORD Px68kUnpack565I(WORD pixel)
{
	const DWORD r5 = (DWORD)((pixel >> 11) & 0x1f);
	const DWORD g5 = (DWORD)((pixel >> 6) & 0x1f);
	const DWORD b5 = (DWORD)((pixel >> 0) & 0x1f);
	const DWORD i1 = (DWORD)((pixel & 0x0020) ? 1u : 0u);
	const DWORD g6 = (DWORD)((g5 << 1) | i1);

	const DWORD r8 = (DWORD)((r5 << 3) | (r5 >> 2));
	const DWORD g8 = (DWORD)((g6 << 2) | (g6 >> 4));
	const DWORD b8 = (DWORD)((b5 << 3) | (b5 >> 2));

	DWORD out = (DWORD)((r8 << 16) | (g8 << 8) | (b8 << 0));
	if (pixel & 0x0020) {
		out |= k_px68k_ibit_marker;
	}
	return out;
}

static inline WORD BlendTrHalf(WORD base, WORD top)
{
	// px68k TR half-color mixing:
	// w = top; v = base;
	// w &= Pal_HalfMask; if (v & Ibit) w += Pal_Ix2;
	// v &= Pal_HalfMask; out = (v + w) >> 1;
	static const WORD half_mask = (WORD)0xF79E;
	static const WORD ibit = (WORD)0x0020;
	static const WORD ix2 = (WORD)0x0040;

	WORD w = (WORD)(top & half_mask);
	if (base & ibit) {
		w = (WORD)(w + ix2);
	}
	const WORD v = (WORD)(base & half_mask);
	return (WORD)((v + w) >> 1);
}

static inline WORD DimHalfFill(WORD pixel)
{
	static const WORD half_mask = (WORD)0xF79E;
	return (WORD)((pixel & half_mask) >> 1);
}

static inline BOOL ShouldApplyTrBlend(const VC::vc_t *vc)
{
	return (BOOL)(vc && ((vc->vr2h & 0x5d) == 0x1d));
}

static inline int CalcBGHAdjustPixels(const CRTC *crtc, const Sprite *sprite)
{
	if (!sprite || !crtc) {
		return 0;
	}

	Sprite::sprite_t spr;
	sprite->GetSprite(&spr);

	const CRTC::crtc_t *const crtc_state = crtc->GetWorkAddr();
	if (!crtc_state) {
		return 0;
	}

	const int bg_hdisp = (int)(spr.h_disp & 0xff);
	const int crtc_hstart = (int)(crtc_state->reg[0x04] & 0xff);
	return (bg_hdisp - (crtc_hstart + 4)) * 8;
}

static void FastClearLine(DWORD *buf, int len)
{
	for (int i = 0; i < len; ++i) {
		buf[i] = k_render_color0;
	}
}

static void FastBuildLine64K(const Render::render_t *work, int y, int len, DWORD *dst, BOOL *active)
{
	DWORD x;
	DWORD off;
	WORD raw;
	DWORD pixel;

	x = work->grpx[0] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[0]) & 0x1ff) << 10) + (x << 1);
	for (int i = 0; i < len; ++i) {
		raw = *(WORD *)(&work->grpgv[off]);
		pixel = work->palptr[(DWORD)(raw & 0xfffe)];
		if ((raw & 0xfffe) == 0) {
			pixel |= k_render_color0;
		}
		dst[i] = pixel;
		if (FastVisiblePixel(pixel)) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildLine1024(const Render::render_t *work, int y, int len, DWORD *dst, BOOL *active)
{
	const WORD *src;
	DWORD line;
	DWORD x;
	WORD raw;
	int bits;
	int remain;
	int index;

	line = (DWORD)((y + (int)work->grpy[0]) & 0x3ff);
	if ((line & 0x200) == 0) {
		line <<= 10;
		bits = (work->grpx[0] & 0x200) ? 4 : 0;
	}
	else {
		line = (line & 0x1ff) << 10;
		bits = (work->grpx[0] & 0x200) ? 12 : 8;
	}
	x = work->grpx[0] & 0x1ff;
	src = (const WORD *)(&work->grpgv[line + (x << 1)]);
	remain = ((int)(x ^ 0x1ff) + 1);
	for (int i = 0; i < len; ++i) {
		raw = *src++;
		index = (raw >> bits) & 0x0f;
		dst[i] = work->paldata[index];
		if (FastVisiblePixel(dst[i])) {
			*active = TRUE;
		}
		if (--remain == 0) {
			src -= 0x200;
			bits ^= 4;
			remain = 512;
		}
	}
}

static void FastBuildLine4(const Render::render_t *work, int page, int y, int len, DWORD *dst, BOOL opaq, BOOL *active)
{
	DWORD off;
	DWORD x;
	DWORD pixel;
	int index;
	BYTE data;

	x = work->grpx[page] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[page]) & 0x1ff) << 10) + (x << 1);
	if (page >= 2) {
		off++;
	}
	for (int i = 0; i < len; ++i) {
		data = work->grpgv[off];
		index = (page & 1) ? ((data >> 4) & 0x0f) : (data & 0x0f);
		if (index != 0) {
			pixel = work->paldata[index];
			if (opaq || FastVisiblePixel(pixel)) {
				dst[i] = pixel;
			}
		}
		else if (opaq) {
			dst[i] = k_render_color0;
		}
		if (FastVisiblePixel(dst[i])) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildLine4TR(const Render::render_t *work, int page, int y, int len, DWORD *dst, const DWORD *grp_sp, BOOL opaq, BOOL *active)
{
	DWORD off;
	DWORD x;
	DWORD pixel;
	DWORD base;
	int index;
	BYTE data;

	page &= 3;
	x = work->grpx[page] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[page]) & 0x1ff) << 10) + (x << 1);
	if (page >= 2) {
		off++;
	}
	for (int i = 0; i < len; ++i) {
		pixel = opaq ? k_render_color0 : dst[i];
		data = work->grpgv[off];
		index = (page & 1) ? ((data >> 4) & 0x0f) : (data & 0x0f);
		if (FastVisiblePixel(grp_sp[i])) {
			if (index != 0) {
				base = work->paldata[index];
				if (FastVisiblePixel(base)) {
					pixel = BlendTrHalf((WORD)Px68kPack565I(base), (WORD)Px68kPack565I(grp_sp[i]));
					pixel = Px68kUnpack565I((WORD)pixel);
				}
			}
		}
		else if (index != 0) {
			base = work->paldata[index];
			if (opaq || FastVisiblePixel(base)) {
				pixel = base;
			}
		}
		dst[i] = pixel;
		if (FastVisiblePixel(pixel)) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildSpecial4(const Render::render_t *work, int page, int y, int len, DWORD *grp_sp, DWORD *grp_sp2, BOOL *active)
{
	DWORD off;
	DWORD x;
	int index;
	BYTE data;

	x = work->grpx[page] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[page]) & 0x1ff) << 10) + (x << 1);
	if (page >= 2) {
		off++;
	}
	for (int i = 0; i < len; ++i) {
		data = work->grpgv[off];
		index = (page & 1) ? ((data >> 4) & 0x0f) : (data & 0x0f);
		if (index & 1) {
			grp_sp[i] = work->paldata[index & 0x0e];
			grp_sp2[i] = k_render_color0;
			if (FastVisiblePixel(grp_sp[i])) {
				*active = TRUE;
			}
		}
		else {
			grp_sp[i] = k_render_color0;
			grp_sp2[i] = work->paldata[index & 0x0e];
			if (FastVisiblePixel(grp_sp2[i])) {
				*active = TRUE;
			}
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildLine8(const Render::render_t *work, int pair, int y, int len, DWORD *dst, BOOL opaq, BOOL *active)
{
	DWORD x;
	DWORD off;
	DWORD pixel;
	int index;

	x = work->grpx[pair * 2 + 0] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[pair * 2 + 0]) & 0x1ff) << 10) + (x << 1) + pair;
	for (int i = 0; i < len; ++i) {
		pixel = opaq ? k_render_color0 : dst[i];
		index = work->grpgv[off];
		if (index != 0) {
			pixel = work->paldata[index];
			if (!opaq && !FastVisiblePixel(pixel)) {
				pixel = dst[i];
			}
		}
		dst[i] = pixel;
		if (FastVisiblePixel(pixel)) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildSpecial8(const Render::render_t *work, int pair, int y, int len, DWORD *grp_sp, DWORD *grp_sp2, BOOL *grp_sp_tr, BOOL *active)
{
	DWORD x;
	DWORD off;
	DWORD pixel;
	int index;
	int even_index;

	x = work->grpx[pair * 2 + 0] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[pair * 2 + 0]) & 0x1ff) << 10) + (x << 1) + pair;
	for (int i = 0; i < len; ++i) {
		index = work->grpgv[off];
		even_index = (index & 0xfe);
		grp_sp_tr[i] = FALSE;
		if (even_index != 0) {
			pixel = work->paldata[even_index];
		}
		else {
			pixel = k_render_color0;
		}
		if (index & 1) {
			grp_sp[i] = pixel;
			if (even_index != 0) {
				grp_sp[i] |= k_px68k_ibit_marker;
			}
			grp_sp2[i] = k_render_color0;
			if (FastVisiblePixel(grp_sp[i])) {
				*active = TRUE;
			}
		}
		else {
			grp_sp[i] = k_render_color0;
			grp_sp2[i] = pixel;
			if ((even_index != 0) && (Px68kPack565I(pixel) == 0)) {
				grp_sp_tr[i] = TRUE;
				*active = TRUE;
			}
			else if (FastVisiblePixel(grp_sp2[i])) {
				*active = TRUE;
			}
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildLine8TR(const Render::render_t *work, int pair, int y, int len, DWORD *dst, const DWORD *grp_sp, BOOL opaq, BOOL *active)
{
	DWORD x;
	DWORD off;
	DWORD pixel;
	DWORD base;
	int index;

	x = work->grpx[pair * 2 + 0] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[pair * 2 + 0]) & 0x1ff) << 10) + (x << 1) + pair;
	for (int i = 0; i < len; ++i) {
		pixel = opaq ? 0 : dst[i];
		index = work->grpgv[off];
		if (FastVisiblePixel(grp_sp[i])) {
			if (index != 0) {
				base = work->paldata[index];
				if (Px68kPack565I(base) != 0) {
					pixel = Px68kUnpack565I(BlendTrHalf((WORD)Px68kPack565I(base), (WORD)Px68kPack565I(grp_sp[i])));
				}
				else {
					pixel = 0;
				}
			}
		}
		else if (index != 0) {
			base = work->paldata[index];
			if (opaq || FastVisiblePixel(base)) {
				pixel = base;
			}
		}
		dst[i] = pixel;
		if (FastVisiblePixel(pixel)) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildLine8TRGT(const Render::render_t *work, int pair, int y, int len, DWORD *dst, const DWORD *grp_sp, BOOL *grp_sp_tr, BOOL opaq, BOOL *active)
{
	DWORD x;
	DWORD off;
	DWORD pixel;
	int index;

	x = work->grpx[pair * 2 + 0] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[pair * 2 + 0]) & 0x1ff) << 10) + (x << 1) + pair;
	for (int i = 0; i < len; ++i) {
		pixel = opaq ? 0 : dst[i];
		index = work->grpgv[off];
		if (FastVisiblePixel(grp_sp[i]) || grp_sp_tr[i]) {
			pixel = 0;
		}
		else if (index != 0) {
			pixel = work->paldata[index];
		}
		dst[i] = pixel;
		grp_sp_tr[i] = FALSE;
		if (FastVisiblePixel(pixel)) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildSpecial64K(const Render::render_t *work, int y, int len, DWORD *grp_sp, DWORD *grp_sp2, BOOL *active)
{
	DWORD x;
	DWORD off;
	WORD raw;
	DWORD pixel;

	x = work->grpx[0] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[0]) & 0x1ff) << 10) + (x << 1);
	for (int i = 0; i < len; ++i) {
		raw = *(WORD *)(&work->grpgv[off]);
		pixel = work->palptr[(DWORD)(raw & 0xfffe)];
		if ((raw & 0xfffe) == 0) {
			pixel |= k_render_color0;
		}
		if (raw & 1) {
			grp_sp[i] = pixel;
			grp_sp2[i] = k_render_color0;
			if (FastVisiblePixel(grp_sp[i])) {
				*active = TRUE;
			}
		}
		else {
			grp_sp[i] = k_render_color0;
			grp_sp2[i] = pixel;
			if (FastVisiblePixel(grp_sp2[i])) {
				*active = TRUE;
			}
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildSpecial1024(const Render::render_t *work, int y, int len, DWORD *grp_sp, DWORD *grp_sp2, BOOL *active)
{
	const WORD *src;
	DWORD line;
	DWORD x;
	int bits;
	int remain;
	WORD raw;
	int index;

	line = (DWORD)((y + (int)work->grpy[0]) & 0x3ff);
	if ((line & 0x200) == 0) {
		line <<= 10;
		bits = (work->grpx[0] & 0x200) ? 4 : 0;
	}
	else {
		line = (line & 0x1ff) << 10;
		bits = (work->grpx[0] & 0x200) ? 12 : 8;
	}
	x = work->grpx[0] & 0x1ff;
	src = (const WORD *)(&work->grpgv[line + (x << 1)]);
	remain = ((int)(x ^ 0x1ff) + 1);
	for (int i = 0; i < len; ++i) {
		raw = *src++;
		index = (raw >> bits) & 0x0f;
		if (index & 1) {
			grp_sp[i] = work->paldata[index & 0x0e];
			grp_sp2[i] = k_render_color0;
			if (FastVisiblePixel(grp_sp[i])) {
				*active = TRUE;
			}
		}
		else {
			grp_sp[i] = k_render_color0;
			grp_sp2[i] = work->paldata[index & 0x0e];
			if (FastVisiblePixel(grp_sp2[i])) {
				*active = TRUE;
			}
		}
		if (--remain == 0) {
			src -= 0x200;
			bits ^= 4;
			remain = 512;
		}
	}
}

static void FastPrepareBGTextLine(DWORD *line, BYTE *tr_flag, const BYTE *bg_flag, const DWORD *text_line, const DWORD *bg_line,
	int len, BOOL ton, BOOL bgon, int tx_pri, int sp_pri, BOOL bg_opaq)
{
	for (int i = 0; i < len; ++i) {
		line[i] = k_render_color0;
		tr_flag[i] = bg_flag ? (bg_flag[i] & 2) : 0;
	}

	if (sp_pri < tx_pri) {
		if (ton) {
			for (int i = 0; i < len; ++i) {
				line[i] = text_line[i];
				if (FastVisiblePixel(text_line[i])) {
					tr_flag[i] |= 1;
				}
			}
		}
		if (bgon && bg_flag) {
			for (int i = 0; i < len; ++i) {
				if (bg_opaq || (bg_flag[i] & 2)) {
					line[i] = bg_line[i];
				}
			}
		}
		return;
	}

	if (bgon && bg_flag) {
		for (int i = 0; i < len; ++i) {
			if (bg_opaq || (bg_flag[i] & 2)) {
				line[i] = bg_line[i];
			}
		}
	}

	if (ton) {
		if (!bgon) {
			for (int i = 0; i < len; ++i) {
				line[i] = text_line[i];
				if (FastVisiblePixel(text_line[i])) {
					tr_flag[i] |= 1;
				}
			}
		}
		else {
			for (int i = 0; i < len; ++i) {
				if (FastVisiblePixel(text_line[i])) {
					line[i] = text_line[i];
					tr_flag[i] |= 1;
				}
			}
		}
	}
}

static void Xm6Px68kComposeScanlineFast(DWORD *out_argb32,
                                         int len,
                                         const DWORD *grp,
                                         const DWORD *grp_sp,
                                         const DWORD *grp_sp2,
                                         const DWORD *bgtext_line,
                                         const BYTE *tr_flag,
                                         BOOL gon,
                                         BOOL tron,
                                         BOOL pron,
                                         BOOL ton,
                                         BOOL bgon,
                                         int gr_pri,
                                         int sp_pri,
                                         int tx_pri,
                                         BYTE vr2h,
                                         BOOL transparency_enabled)
{
	if (!transparency_enabled) {
		tron = FALSE;
		pron = FALSE;
	}
	if (!out_argb32 || len <= 0) {
		return;
	}
	if (len > 1024) {
		len = 1024;
	}

	const BOOL tr_mode = (BOOL)((vr2h & 0x5d) == 0x1d);
	const BOOL dim_mode = (BOOL)((vr2h & 0x5d) == 0x1c);
	const BOOL pri_mode = (BOOL)((vr2h & 0x5c) == 0x14);

	WORD dst565[1024];
	WORD grp565[1024];
	WORD grp_sp565[1024];
	WORD grp_sp2_565[1024];
	WORD bgtext565[1024];

	for (int i = 0; i < len; ++i) {
		dst565[i] = 0;
		grp565[i] = grp ? Px68kPack565I(grp[i]) : 0;
		grp_sp565[i] = grp_sp ? Px68kPack565I(grp_sp[i]) : 0;
		grp_sp2_565[i] = grp_sp2 ? Px68kPack565I(grp_sp2[i]) : 0;
		bgtext565[i] = bgtext_line ? Px68kPack565I(bgtext_line[i]) : 0;
	}

	BOOL opaq = TRUE;
	BOOL tdrawed = FALSE;

	if (gr_pri >= 2) {
		if (gon) {
			for (int i = 0; i < len; ++i) {
				dst565[i] = grp565[i];
			}
			opaq = FALSE;
		}
		if (tron) {
			for (int i = 0; i < len; ++i) {
				if (grp_sp2_565[i] != 0) {
					dst565[i] = grp_sp2_565[i];
				}
			}
			opaq = FALSE;
		}
	}
	if ((sp_pri >= 2) && bgon) {
		if (tr_mode && (gr_pri != 2) && tron) {
			if (gr_pri < tx_pri) {
				for (int i = 0; i < len; ++i) {
					if (bgtext565[i] != 0) {
						if (grp_sp565[i] != 0) {
							dst565[i] = Px68kPack565I(BlendTrHalf(bgtext565[i], grp_sp565[i]));
						}
						else {
							dst565[i] = bgtext565[i];
						}
					}
				}
			}
			else {
				for (int i = 0; i < len; ++i) {
					if (bgtext565[i] != 0) {
						dst565[i] = bgtext565[i];
					}
				}
			}
		}
		else {
			for (int i = 0; i < len; ++i) {
				if (bgtext565[i] != 0) {
					dst565[i] = bgtext565[i];
				}
			}
		}
		tdrawed = TRUE;
		opaq = FALSE;
	}
	if ((tx_pri >= 2) && ton) {
		if (tr_mode && (gr_pri != 2) && tron) {
			for (int i = 0; i < len; ++i) {
				if (bgtext565[i] != 0) {
					dst565[i] = bgtext565[i];
				}
			}
		}
		else {
			for (int i = 0; i < len; ++i) {
				if (bgtext565[i] != 0) {
					dst565[i] = bgtext565[i];
				}
			}
		}
		opaq = FALSE;
		tdrawed = TRUE;
	}

	if ((gr_pri == 1) && gon) {
		for (int i = 0; i < len; ++i) {
			if (grp565[i] != 0) {
				dst565[i] = grp565[i];
			}
		}
		opaq = FALSE;
	}
	if ((sp_pri == 1) && bgon) {
		for (int i = 0; i < len; ++i) {
			if (bgtext565[i] != 0) {
				dst565[i] = bgtext565[i];
			}
		}
		tdrawed = TRUE;
		opaq = FALSE;
	}
	if ((tx_pri == 1) && tr_mode && (gr_pri != 0) && (sp_pri > gr_pri) && bgon && tron) {
		for (int i = 0; i < len; ++i) {
			if (bgtext565[i] != 0) {
				dst565[i] = bgtext565[i];
			}
		}
		tdrawed = TRUE;
		opaq = FALSE;
		if (tron) {
			for (int i = 0; i < len; ++i) {
				if (grp_sp2_565[i] != 0) {
					dst565[i] = grp_sp2_565[i];
				}
			}
		}
	}
	else if ((gr_pri == 1) && tron && gon && (vr2h & 0x10)) {
		for (int i = 0; i < len; ++i) {
			if (grp_sp2_565[i] != 0) {
				dst565[i] = grp_sp2_565[i];
			}
		}
		opaq = FALSE;
	}
	if ((tx_pri == 1) && ton) {
		for (int i = 0; i < len; ++i) {
			if (bgtext565[i] != 0) {
				dst565[i] = bgtext565[i];
			}
		}
		opaq = FALSE;
		tdrawed = TRUE;
	}

	if ((gr_pri == 0) && gon) {
		for (int i = 0; i < len; ++i) {
			if (grp565[i] != 0) {
				dst565[i] = grp565[i];
			}
		}
		opaq = FALSE;
	}
	if ((sp_pri == 0) && bgon) {
		for (int i = 0; i < len; ++i) {
			if (bgtext565[i] != 0) {
				dst565[i] = bgtext565[i];
			}
		}
		tdrawed = TRUE;
		opaq = FALSE;
	}
	if ((tx_pri == 0) && tr_mode && (sp_pri > gr_pri) && bgon && tron) {
		for (int i = 0; i < len; ++i) {
			if (bgtext565[i] != 0) {
				dst565[i] = bgtext565[i];
			}
		}
		tdrawed = TRUE;
		opaq = FALSE;
		if (tron) {
			for (int i = 0; i < len; ++i) {
				if (grp_sp2_565[i] != 0) {
					dst565[i] = grp_sp2_565[i];
				}
			}
		}
	}
	else if ((gr_pri == 0) && tron && (vr2h & 0x10)) {
		for (int i = 0; i < len; ++i) {
			if (grp_sp2_565[i] != 0) {
				dst565[i] = grp_sp2_565[i];
			}
		}
		opaq = FALSE;
	}
	if ((tx_pri == 0) && ton) {
		for (int i = 0; i < len; ++i) {
			if (bgtext565[i] != 0) {
				dst565[i] = bgtext565[i];
			}
		}
		tdrawed = TRUE;
		opaq = FALSE;
	}

	if (pri_mode && pron) {
		for (int i = 0; i < len; ++i) {
			if (grp_sp565[i] != 0) {
				dst565[i] = grp_sp565[i];
			}
		}
	}
	else if (dim_mode && tron) {
		for (int i = 0; i < len; ++i) {
			if (dst565[i] == 0 && grp_sp565[i] != 0) {
				dst565[i] = DimHalfFill(grp_sp565[i]);
			}
		}
	}

	if (opaq) {
		std::memset(dst565, 0, (size_t)len * sizeof(WORD));
	}

	for (int i = 0; i < len; ++i) {
		const WORD w = dst565[i];
		out_argb32[i] = (w != 0) ? Px68kUnpack565I(w) : 0;
	}
}

} // namespace

void FASTCALL Render::FastDrawSpriteLinePX(int raster, int pri, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active)
{
	int n;
	DWORD *reg;
	DWORD **ptr;
	int x;
	int dx;
	DWORD pixel;
	const DWORD sprite_mask = 0x4000;
	const int bg_hadjust = CalcBGHAdjustPixels(crtc, sprite);

	reg = &render.spreg[127 << 2];
	ptr = &render.spptr[127 << 9];
	ptr += (raster & 0x1ff);
	for (n = 127; n >= 0; n--) {
		if (render.spuse[n] && ((int)(reg[3] & 3) == pri) && *ptr) {
			DWORD pcgno = reg[2] & 0xfff;
			if (!render.pcgready[pcgno]) {
				ASSERT(render.pcguse[pcgno] > 0);
				render.pcgready[pcgno] = TRUE;
				RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
			}
			const DWORD *line = *ptr;
			x = ((int)reg[0] + bg_hadjust) & 0x3ff;
			for (int i = 0; i < 16; i++) {
				dx = x - 16 + ((reg[2] & sprite_mask) ? (15 - i) : i);
				if ((dx < 0) || (dx >= render.mixlen)) {
					continue;
				}
				pixel = line[i];
				if (!FastVisiblePixel(pixel)) {
					continue;
				}
				if (bg_pri[dx] >= (WORD)(n * 8)) {
					bg_line[dx] = pixel;
					bg_flag[dx] |= 2;
					bg_pri[dx] = (WORD)(n * 8);
					*active = TRUE;
				}
			}
		}
		reg -= 4;
		ptr -= 512;
	}
}

void FASTCALL Render::FastDrawBGPageLinePX(int page, int raster, BOOL gd, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active)
{
	int yblk;
	int line;
	DWORD **ptr;
	int x;
	int i;
	int gx;
	int tile;
	int off;
	DWORD **ent;
	DWORD *src;
	DWORD bgdata;
	DWORD pixel;
	DWORD pcgno;
	int bank;
	const int bg_hadjust = CalcBGHAdjustPixels(crtc, sprite);

	(void)bg_pri;
	if (!render.bgdisp[page]) {
		return;
	}
	if ((page == 1) && render.bgsize) {
		return;
	}

	yblk = (int)render.bgy[page] + (raster & 0x1ff);
	if (render.bgsize) {
		yblk &= 0x3ff;
		yblk >>= 4;
	}
	else {
		yblk &= 0x1ff;
		yblk >>= 3;
	}
	if (render.bgall[page][yblk]) {
		render.bgall[page][yblk] = FALSE;
		BGBlock(page, yblk);
	}

	line = (int)render.bgy[page] + (raster & 0x1ff);
	if (render.bgsize) {
		line &= 0x3ff;
		x = (int)((render.bgx[page] - bg_hadjust) & 0x3ff);
	}
	else {
		line &= 0x1ff;
		x = (int)((render.bgx[page] - bg_hadjust) & 0x1ff);
	}
	ptr = render.bgptr[page] + (line << 7);

	for (i = 0; i < render.mixlen; i++) {
		if (render.bgsize) {
			gx = (x + i) & 0x3ff;
			tile = gx >> 4;
			off = gx & 15;
		}
		else {
			gx = (x + i) & 0x1ff;
			tile = gx >> 3;
			off = gx & 7;
		}
		ent = &ptr[tile << 1];
		src = ent[0];
		bgdata = (DWORD)(size_t)ent[1];
		if (!src) {
			continue;
		}
		pcgno = bgdata & 0x0fff;
		if (!render.pcgready[pcgno]) {
			ASSERT(render.pcguse[pcgno] > 0);
			render.pcgready[pcgno] = TRUE;
			RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
		}
		if (bgdata & 0x4000) {
			pixel = src[render.bgsize ? (15 - off) : (7 - off)];
		}
		else {
			pixel = src[off];
		}

		if (FastVisiblePixel(pixel)) {
			bg_line[i] = pixel;
			bg_flag[i] |= 2;
			*active = TRUE;
			continue;
		}
		if (!gd) {
			continue;
		}
		bank = (int)((bgdata >> 8) & 0x0f);
		if (bank == 0) {
			continue;
		}
		if (bg_flag[i] & 2) {
			continue;
		}

		pixel &= ~k_render_color0;
		bg_line[i] = pixel;
		bg_flag[i] |= 2;
		*active = TRUE;
	}
}

void FASTCALL Render::FastBuildBGLinePX(int src_y, BOOL ton, int tx_pri, int sp_pri, DWORD *bg_line, BYTE *bg_flag, BOOL *active, BOOL *bg_opaq)
{
	int i;
	WORD *bg_pri;
	const BOOL sprite_visible = sprite->IsDisplay();
	const BOOL has_bg = (BOOL)(render.bgdisp[0] || (render.bgdisp[1] && !render.bgsize));
	const BOOL bgsp_on = render.bgspflag;
	const BOOL gd = (BOOL)(sp_pri >= tx_pri);
	const BOOL opaq = gd ? TRUE : (BOOL)!ton;
	const DWORD base = (render.paldata[0x100] & ~k_render_color0);
	const int raster = (src_y & 0x1ff);
	const BOOL has_sources = (BOOL)(sprite_visible || has_bg);

	bg_pri = render.fast_bg_pribuf;
	for (i = 0; i < render.mixlen; i++) {
		bg_line[i] = opaq ? base : k_render_color0;
		bg_flag[i] = 0;
		bg_pri[i] = 0xffff;
	}
	*active = FALSE;
	if (bg_opaq) {
		*bg_opaq = FALSE;
	}
	if (!bgsp_on || !has_sources || (render.mixlen > 512)) {
		return;
	}

	*active = has_sources;
	if (sprite_visible) {
		FastDrawSpriteLinePX(raster, 1, bg_line, bg_flag, bg_pri, active);
	}
	if (render.bgdisp[1] && !render.bgsize) {
		FastDrawBGPageLinePX(1, raster, gd, bg_line, bg_flag, bg_pri, active);
	}
	if (sprite_visible) {
		FastDrawSpriteLinePX(raster, 2, bg_line, bg_flag, bg_pri, active);
	}
	if (render.bgdisp[0]) {
		FastDrawBGPageLinePX(0, raster, gd, bg_line, bg_flag, bg_pri, active);
	}
	if (sprite_visible) {
		FastDrawSpriteLinePX(raster, 3, bg_line, bg_flag, bg_pri, active);
	}

	if (bg_opaq) {
		*bg_opaq = opaq;
	}
}

void FASTCALL Render::FastMixGrp(int y, DWORD *grp, DWORD *grp_sp, DWORD *grp_sp2,
	BOOL *grp_sp_tr, BOOL *gon, BOOL *tron, BOOL *pron)
{
	const VC::vc_t *p;
	const CRTC::crtc_t *c;
	BOOL active;
	BOOL pair0_first;
	BOOL opaq;
	int page;
	int slot;
	int raster;
	int top_pair;
	int other_pair;

	ASSERT(grp);
	ASSERT(grp_sp);
	ASSERT(grp_sp2);
	ASSERT(grp_sp_tr);
	ASSERT(gon);
	ASSERT(tron);
	ASSERT(pron);

	p = vc->GetWorkAddr();
	c = crtc->GetWorkAddr();
	raster = y & 0x3ff;
	if (c && ((c->reg[0x29] & 0x1c) == 0x1c)) {
		raster = (raster << 1) & 0x3ff;
	}

	FastClearLine(grp, render.mixlen);
	FastClearLine(grp_sp, render.mixlen);
	FastClearLine(grp_sp2, render.mixlen);
	std::memset(grp_sp_tr, 0, (size_t)render.mixlen * sizeof(BOOL));
	*gon = FALSE;
	*tron = FALSE;
	*pron = FALSE;

	switch (render.grptype) {
		case 0:
			if (render.grpen[0] && p->gon) {
				active = FALSE;
				FastBuildLine1024(&render, raster, render.mixlen, grp, &active);
				*gon = TRUE;
			}
			if (render.grpen[0] && p->exon && p->bp && p->gon) {
				active = FALSE;
				FastBuildSpecial1024(&render, raster, render.mixlen, grp_sp, grp_sp2, &active);
				*tron = TRUE;
				*pron = TRUE;
			}
			return;

		case 1:
			if (p->exon && p->gs[0]) {
				page = (int)(p->gp[0] & 3);
				if (render.grpen[page]) {
					active = FALSE;
					FastBuildSpecial4(&render, page, raster, render.mixlen, grp_sp, grp_sp2, &active);
					*tron = TRUE;
					*pron = TRUE;
				}
			}

			opaq = TRUE;
			if (p->gs[3]) {
				page = (int)(p->gp[3] & 3);
				if (render.grpen[page]) {
					active = FALSE;
					FastBuildLine4(&render, page, raster, render.mixlen, grp, opaq, &active);
					*gon = TRUE;
					opaq = FALSE;
				}
			}
			if (p->gs[2]) {
				page = (int)(p->gp[2] & 3);
				if (render.grpen[page]) {
					active = FALSE;
					FastBuildLine4(&render, page, raster, render.mixlen, grp, opaq, &active);
					*gon = TRUE;
					opaq = FALSE;
				}
			}
			if (p->gs[1]) {
				page = (int)(p->gp[1] & 3);
				if (render.grpen[page]) {
					active = FALSE;
					if (((p->vr2h & 0x1e) == 0x1e) && *tron) {
						FastBuildLine4TR(&render, page, raster, render.mixlen, grp, grp_sp, opaq, &active);
					}
					else {
						FastBuildLine4(&render, page, raster, render.mixlen, grp, opaq, &active);
					}
					*gon = TRUE;
					opaq = FALSE;
				}
			}
			if (p->gs[0] && ((p->vr2h & 0x14) != 0x14)) {
				page = (int)(p->gp[0] & 3);
				if (render.grpen[page]) {
					active = FALSE;
					FastBuildLine4(&render, page, raster, render.mixlen, grp, opaq, &active);
					*gon = TRUE;
				}
			}
			return;

		case 2:
			pair0_first = (BOOL)(((int)p->gp[0]) <= ((int)p->gp[2]));
			top_pair = pair0_first ? 0 : 1;
			other_pair = pair0_first ? 1 : 0;

			opaq = TRUE;

			if (p->exon && p->gs[0]) {
				active = FALSE;
				FastBuildSpecial8(&render, top_pair, raster, render.mixlen, grp_sp, grp_sp2, grp_sp_tr, &active);
				*tron = TRUE;
				*pron = TRUE;
			}

			if (render.grpen[other_pair * 2]) {
				active = FALSE;
				if (((p->vr2h & 0x1e) == 0x1e) && *tron) {
					FastBuildLine8TR(&render, other_pair, raster, render.mixlen, grp, grp_sp, opaq, &active);
				}
				else if (((p->vr2h & 0x1d) == 0x1d) && *tron) {
					FastBuildLine8TRGT(&render, other_pair, raster, render.mixlen, grp, grp_sp, grp_sp_tr, opaq, &active);
				}
				else {
					FastBuildLine8(&render, other_pair, raster, render.mixlen, grp, opaq, &active);
				}
				*gon = TRUE;
				opaq = FALSE;
			}
			if (render.grpen[top_pair * 2] && ((p->vr2h & 0x14) != 0x14)) {
				active = FALSE;
				FastBuildLine8(&render, top_pair, raster, render.mixlen, grp, opaq, &active);
				*gon = TRUE;
			}
			return;

		case 3:
		case 4:
			if (render.grpen[0] && p->gon) {
				active = FALSE;
				FastBuildLine64K(&render, raster, render.mixlen, grp, &active);
				*gon = TRUE;
			}
			if (render.grpen[0] && p->exon && p->bp && (render.mixpage > 0)) {
				active = FALSE;
				FastBuildSpecial64K(&render, raster, render.mixlen, grp_sp, grp_sp2, &active);
				*tron = TRUE;
				*pron = TRUE;
			}
			return;

		default:
			for (slot = 0; slot < render.mixlen; slot++) {
				if (FastVisiblePixel(grp[slot])) {
					*gon = TRUE;
					break;
				}
			}
			return;
	}
}

void FASTCALL Render::MixFastLine(int dst_y, int src_y)
{
	DWORD *dst;
	DWORD *grp = render.fast_grp_linebuf;
	DWORD *grp_sp = render.fast_grp_linebuf_sp;
	DWORD *grp_sp2 = render.fast_grp_linebuf_sp2;
	BOOL *grp_sp_tr = render.fast_grp_linebuf_sp_tr;
	DWORD text_line[1024];
	DWORD *bg_line = render.fast_bg_linebuf;
	DWORD bgtext_line[1024];
	DWORD out[1024];
	BYTE *bg_flag = render.fast_text_trflag;
	BYTE tr_flag[1024];
	const VC::vc_t *p;
	int tx_pri;
	int sp_pri;
	int gr_pri;
	BOOL gon;
	BOOL tron;
	BOOL pron;
	BOOL ton;
	BOOL bgon;
	BOOL bg_active;
	BOOL bg_opaq;
	DWORD mix_stamp;

	if (!render.mixbuf) {
		return;
	}
	if ((dst_y < 0) || (dst_y >= render.mixheight)) {
		return;
	}
	if ((src_y < 0) || (src_y >= 1024)) {
		return;
	}
	ASSERT(render.mixlen > 0);

	mix_stamp = render.fast_mix_stamp[src_y];
	if ((!render.mix[src_y]) && (render.fast_mix_done[src_y] == mix_stamp)) {
		return;
	}
	render.mix[src_y] = FALSE;
	render.fast_mix_done[src_y] = mix_stamp;
	dst = &render.mixbuf[render.mixwidth * dst_y];
	p = vc->GetWorkAddr();
	tx_pri = (int)(p->tx & 3);
	sp_pri = (int)(p->sp & 3);
	gr_pri = (int)(p->gr & 3);
	ton = render.texten;

	FastMixGrp(src_y, grp, grp_sp, grp_sp2, grp_sp_tr, &gon, &tron, &pron);

	for (int i = 0; i < render.mixlen; i++) {
		out[i] = 0;
		if (ton) {
			text_line[i] = render.textout[(((src_y + (int)render.texty) & 0x3ff) << 10) + (((int)render.textx + i) & 0x3ff)];
		}
		else {
			text_line[i] = k_render_color0;
		}
		bg_line[i] = k_render_color0;
		bg_flag[i] = 0;
	}

	bg_active = FALSE;
	bg_opaq = FALSE;
	FastBuildBGLinePX(src_y, ton, tx_pri, sp_pri, bg_line, bg_flag, &bg_active, &bg_opaq);
	bgon = bg_active;
	FastPrepareBGTextLine(bgtext_line, tr_flag, bg_flag, text_line, bg_line, render.mixlen, ton, bgon, tx_pri, sp_pri, bg_opaq);

	Xm6Px68kComposeScanlineFast(out, render.mixlen,
		grp, grp_sp, grp_sp2,
		bgtext_line, tr_flag,
		gon, tron, pron, ton, bgon,
		gr_pri, sp_pri, tx_pri,
		p->vr2h, transparency_enabled);

	RendMix01(dst, out, render.drawflag + (src_y << 6), render.mixlen);
}
