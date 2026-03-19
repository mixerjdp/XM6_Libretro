#include "os.h"
#include "xm6.h"

#include <cstring>

namespace {

static const DWORD k_render_color0 = 0x80000000u;
static const DWORD k_px68k_ibit_marker = 0x40000000u;

static inline WORD pack_rgb565i(DWORD pixel)
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

static inline DWORD unpack_rgb565i(WORD pixel)
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

static inline WORD blend_tr_half(WORD base, WORD top)
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

static inline WORD dim_half_fill(WORD pixel)
{
	static const WORD half_mask = (WORD)0xF79E;
	return (WORD)((pixel & half_mask) >> 1);
}

static void draw_layer_opaq_or_overlay(WORD *dst, const WORD *src, int len, BOOL opaq)
{
	if (opaq) {
		std::memcpy(dst, src, (size_t)len * sizeof(WORD));
		return;
	}
	for (int i = 0; i < len; ++i) {
		const WORD w = src[i];
		if (w != 0) {
			dst[i] = w;
		}
	}
}

static void draw_bg_or_text_layer(WORD *dst, const WORD *line, const BYTE *tr_flag, int len, BOOL opaq, BOOL td, BYTE mask)
{
	if (opaq) {
		std::memcpy(dst, line, (size_t)len * sizeof(WORD));
		return;
	}
	if (td) {
		for (int i = 0; i < len; ++i) {
			if ((tr_flag[i] & mask) && line[i] != 0) {
				dst[i] = line[i];
			}
		}
		return;
	}
	for (int i = 0; i < len; ++i) {
		if (line[i] != 0) {
			dst[i] = line[i];
		}
	}
}

static void draw_text_line_tr(WORD *dst, const WORD *line, const BYTE *tr_flag, const WORD *grp_sp, int len, BOOL opaq)
{
	const BYTE text_mask = 1;

	if (opaq) {
		for (int i = 0; i < len; ++i) {
			WORD v = line[i];
			const WORD w = grp_sp[i];
			if (w != 0) {
				v = blend_tr_half(v, w);
			} else {
				if (!(tr_flag[i] & text_mask)) {
					v = 0;
				}
			}
			dst[i] = v;
		}
		return;
	}

	for (int i = 0; i < len; ++i) {
		if (!(tr_flag[i] & text_mask)) {
			continue;
		}
		WORD v = line[i];
		if (v == 0) {
			continue;
		}
		const WORD w = grp_sp[i];
		if (w != 0) {
			v = blend_tr_half(v, w);
		}
		dst[i] = v;
	}
}

static void draw_bg_line_tr(WORD *dst, const WORD *line, const BYTE *tr_flag, const WORD *grp_sp, int len, BOOL opaq)
{
	const BYTE bg_mask = 2;

	if (opaq) {
		for (int i = 0; i < len; ++i) {
			WORD v = line[i];
			const WORD w = grp_sp[i];
			if (w != 0) {
				v = blend_tr_half(v, w);
			}
			dst[i] = v;
		}
		return;
	}

	for (int i = 0; i < len; ++i) {
		if (!(tr_flag[i] & bg_mask)) {
			continue;
		}
		WORD v = line[i];
		if (v == 0) {
			continue;
		}
		const WORD w = grp_sp[i];
		if (w != 0) {
			v = blend_tr_half(v, w);
		}
		dst[i] = v;
	}
}

static void draw_pri_line(WORD *dst, const WORD *grp_sp, int len)
{
	for (int i = 0; i < len; ++i) {
		const WORD w = grp_sp[i];
		if (w != 0) {
			dst[i] = w;
		}
	}
}

static void draw_half_fill_line(WORD *dst, const WORD *grp_sp, int len)
{
	for (int i = 0; i < len; ++i) {
		if (dst[i] == 0 && grp_sp[i] != 0) {
			dst[i] = dim_half_fill(grp_sp[i]);
		}
	}
}

} // namespace

void FASTCALL Xm6Px68kComposeScanlineFast(DWORD *out_argb32,
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
		grp565[i] = grp ? pack_rgb565i(grp[i]) : 0;
		grp_sp565[i] = grp_sp ? pack_rgb565i(grp_sp[i]) : 0;
		grp_sp2_565[i] = grp_sp2 ? pack_rgb565i(grp_sp2[i]) : 0;
		bgtext565[i] = bgtext_line ? pack_rgb565i(bgtext_line[i]) : 0;
	}

	BOOL opaq = TRUE;
	BOOL tdrawed = FALSE;

	// Pri = 2/3 layers.
	if (gr_pri >= 2) {
		if (gon) {
			draw_layer_opaq_or_overlay(dst565, grp565, len, opaq);
			opaq = FALSE;
		}
		if (tron) {
			draw_layer_opaq_or_overlay(dst565, grp_sp2_565, len, opaq);
			opaq = FALSE;
		}
	}
	if ((sp_pri >= 2) && bgon) {
		if (tr_mode && (gr_pri != 2) && tron) {
			if (gr_pri < tx_pri) {
				draw_bg_line_tr(dst565, bgtext565, tr_flag, grp_sp565, len, opaq);
			} else {
				draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, tdrawed, 2);
			}
		} else {
			draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, tdrawed, 2);
		}
		tdrawed = TRUE;
		opaq = FALSE;
	}
	if ((tx_pri >= 2) && ton) {
		if (tr_mode && (gr_pri != 2) && tron) {
			draw_text_line_tr(dst565, bgtext565, tr_flag, grp_sp565, len, opaq);
		} else {
			draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, tdrawed, 1);
		}
		opaq = FALSE;
		tdrawed = TRUE;
	}

	// Pri = 1 layers.
	if ((gr_pri == 1) && gon) {
		draw_layer_opaq_or_overlay(dst565, grp565, len, opaq);
		opaq = FALSE;
	}
	if ((sp_pri == 1) && bgon) {
		if (tr_mode && (gr_pri == 0) && tron) {
			if (gr_pri < tx_pri) {
				draw_bg_line_tr(dst565, bgtext565, tr_flag, grp_sp565, len, opaq);
			} else {
				draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, (BOOL)(tx_pri == 2), 2);
			}
		} else {
			draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, (BOOL)(tx_pri == 2), 2);
		}
		tdrawed = TRUE;
		opaq = FALSE;
	}
	if ((tx_pri == 1) && tr_mode && (gr_pri != 0) && (sp_pri > gr_pri) && bgon && tron) {
		draw_bg_line_tr(dst565, bgtext565, tr_flag, grp_sp565, len, opaq);
		tdrawed = TRUE;
		opaq = FALSE;
		if (tron) {
			draw_layer_opaq_or_overlay(dst565, grp_sp2_565, len, opaq);
		}
	} else if ((gr_pri == 1) && tron && gon && (vr2h & 0x10)) {
		draw_layer_opaq_or_overlay(dst565, grp_sp2_565, len, opaq);
		opaq = FALSE;
	}
	if ((tx_pri == 1) && ton) {
		if (tr_mode && (gr_pri == 0) && tron) {
			draw_text_line_tr(dst565, bgtext565, tr_flag, grp_sp565, len, opaq);
		} else {
			draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, (BOOL)(sp_pri >= 1), 1);
		}
		opaq = FALSE;
		tdrawed = TRUE;
	}

	// Pri = 0 layers.
	if ((gr_pri == 0) && gon) {
		draw_layer_opaq_or_overlay(dst565, grp565, len, opaq);
		opaq = FALSE;
	}
	if ((sp_pri == 0) && bgon) {
		draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, (BOOL)(tx_pri >= 1), 2);
		tdrawed = TRUE;
		opaq = FALSE;
	}
	if ((tx_pri == 0) && tr_mode && (sp_pri > gr_pri) && bgon && tron) {
		draw_bg_line_tr(dst565, bgtext565, tr_flag, grp_sp565, len, opaq);
		tdrawed = TRUE;
		opaq = FALSE;
		if (tron) {
			draw_layer_opaq_or_overlay(dst565, grp_sp2_565, len, opaq);
		}
	} else if ((gr_pri == 0) && tron && (vr2h & 0x10)) {
		draw_layer_opaq_or_overlay(dst565, grp_sp2_565, len, opaq);
		opaq = FALSE;
	}
	if ((tx_pri == 0) && ton) {
		draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, TRUE, 1);
		tdrawed = TRUE;
		opaq = FALSE;
	}

	// Special priority / half-fill.
	if (pri_mode && pron) {
		draw_pri_line(dst565, grp_sp565, len);
	} else if (dim_mode && tron) {
		draw_half_fill_line(dst565, grp_sp565, len);
	}

	if (opaq) {
		std::memset(dst565, 0, (size_t)len * sizeof(WORD));
	}

	for (int i = 0; i < len; ++i) {
		const WORD w = dst565[i];
		out_argb32[i] = (w != 0) ? unpack_rgb565i(w) : 0;
	}
}
