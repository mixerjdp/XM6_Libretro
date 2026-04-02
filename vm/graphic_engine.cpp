//---------------------------------------------------------------------------
//
//	X68000 Emulator "XM6"
//
//	Px68k graphic engine abstraction.
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vc.h"
#include "sprite.h"
#include "rend_soft.h"
#include "render.h"
#include "graphic_engine.h"

BOOL FASTCALL IsCMOV(void);

namespace {

static const DWORD REND_COLOR0 = 0x80000000u;
static const DWORD REND_PX68K_IBIT = 0x40000000u;

static inline DWORD BlendHalf(DWORD a, DWORD b)
{
	DWORD rb = ((a & 0x00ff00ffu) + (b & 0x00ff00ffu)) >> 1;
	DWORD g = ((a & 0x0000ff00u) + (b & 0x0000ff00u)) >> 1;
	return (rb & 0x00ff00ffu) | (g & 0x0000ff00u);
}

static inline BOOL IsVisiblePixel(DWORD pixel)
{
	return ((pixel & REND_COLOR0) == 0) && ((pixel & 0x00ffffffu) != 0);
}

static inline DWORD BlendPx68kSemi(DWORD top, DWORD base)
{
	DWORD flags = (top | base) & REND_PX68K_IBIT;
	DWORD color = BlendHalf(top & ~REND_COLOR0, base & ~REND_COLOR0);
	return color | flags;
}

static void ComposeTwoSources(DWORD *dst, const DWORD *top, const DWORD *base, int len, BOOL blend)
{
	for (int x = 0; x < len; ++x) {
		const DWORD a = top ? top[x] : 0;
		const DWORD b = base ? base[x] : 0;
		if (!IsVisiblePixel(a)) {
			dst[x] = IsVisiblePixel(b) ? b : 0;
		}
		else if (!IsVisiblePixel(b)) {
			dst[x] = a;
		}
		else {
			dst[x] = blend ? BlendPx68kSemi(a, b) : a;
		}
	}
}

static void ComposeThreeSources(DWORD *dst, const DWORD *a, const DWORD *b, const DWORD *c, int len, BOOL blend)
{
	for (int x = 0; x < len; ++x) {
		DWORD out = 0;
		const DWORD p0 = a ? a[x] : 0;
		const DWORD p1 = b ? b[x] : 0;
		const DWORD p2 = c ? c[x] : 0;

		if (IsVisiblePixel(p0)) {
			out = p0;
		}
		if (IsVisiblePixel(p1)) {
			out = out ? (blend ? BlendPx68kSemi(p1, out) : p1) : p1;
		}
		if (IsVisiblePixel(p2)) {
			out = out ? (blend ? BlendPx68kSemi(p2, out) : p2) : p2;
		}
		dst[x] = out;
	}
}

static inline BOOL ShouldApplyPx68kTrBlend(const Render *owner)
{
	const VC *vc_device = owner->GetVCDevice();
	ASSERT(vc_device);
	const VC::vc_t *vc = vc_device->GetWorkAddr();
	return ((vc->vr2h & 0x5d) == 0x1d);
}

static void BuildPx68kMixMap(const Render *owner, int map[3])
{
	const VC *vc_device = owner->GetVCDevice();
	ASSERT(vc_device);
	const VC::vc_t *vc = vc_device->GetWorkAddr();
	int tx = (int)vc->tx;
	int sp = (int)vc->sp;
	int gr = (int)vc->gr;

	if (tx == 3) tx--;
	if (sp == 3) sp--;
	if (gr == 3) gr--;

	if (tx == sp) {
		if (tx < gr) {
			tx = 0;
			sp = 1;
			gr = 2;
		}
		else {
			gr = 0;
			tx = 1;
			sp = 2;
		}
	}
	if (tx == gr) {
		if (tx < sp) {
			tx = 0;
			gr = 1;
			sp = 2;
		}
		else {
			sp = 0;
			tx = 1;
			gr = 2;
		}
	}
	if (sp == gr) {
		if (sp < tx) {
			sp = 0;
			gr = 1;
			tx = 2;
		}
		else {
			tx = 0;
			sp = 1;
			gr = 2;
		}
	}

	map[tx] = 0;
	map[sp] = 1;
	map[gr] = 2;
}

static BOOL BuildPx68kSpritePriLine(Render *owner, int y, DWORD *dst, int len)
{
	Render::render_t *rw;
	DWORD *reg;
	DWORD **ptr;
	DWORD pcgno;
	DWORD *line;
	BOOL any = FALSE;
	const Sprite *sprite;

	ASSERT(owner);
	ASSERT(dst);

	sprite = owner->GetSpriteDevice();
	rw = owner->GetWorkAddr();

	if (!sprite || !sprite->IsDisplay()) {
		memset(dst, 0, sizeof(DWORD) * len);
		return FALSE;
	}
	if (len > 1024) {
		len = 1024;
	}
	if (rw->mixlen > 512) {
		memset(dst, 0, sizeof(DWORD) * len);
		return FALSE;
	}

	memset(dst, 0, sizeof(DWORD) * len);
	reg = &rw->spreg[127 << 2];
	ptr = &rw->spptr[127 << 9];
	ptr += y;

	for (int i = 127; i >= 0; --i) {
		if (rw->spuse[i] && (reg[3] == 3) && *ptr) {
			pcgno = reg[2] & 0x0fff;
			if (!rw->pcgready[pcgno]) {
				ASSERT(rw->pcguse[pcgno] > 0);
				rw->pcgready[pcgno] = TRUE;
				RendPCGNew(pcgno, rw->sprmem, rw->pcgbuf, rw->paldata);
			}
			const DWORD sprite_x = (DWORD)(((int)reg[0]) & 0x03ff);
			line = *ptr;
			if (IsCMOV()) {
				RendSpriteC(line, dst, sprite_x, reg[2] & 0x4000);
			}
			else {
				RendSprite(line, dst, sprite_x, reg[2] & 0x4000);
			}
			any = TRUE;
		}
		reg -= 4;
		ptr -= 512;
	}
	return any;
}

static void OverlaySpritePriLine(DWORD *dst, const DWORD *src, int len, BOOL aquales)
{
	for (int x = 0; x < len; ++x) {
		const DWORD s = src[x];
		if (!IsVisiblePixel(s)) {
			continue;
		}
		if (aquales && IsVisiblePixel(dst[x])) {
			continue;
		}
		dst[x] = s;
	}
}

} /* namespace */

void FASTCALL GraphicEngine::ProcessCommon(Render *owner)
{
	int i;

	if (owner->render.first >= owner->render.last) {
		return;
	}

	if (owner->render.vc) {
		owner->ComposeVideo();
	}

	if (owner->render.contrast) {
		owner->Contrast();
	}

	if (owner->render.palette) {
		owner->Palette();
	}

	if (owner->render.first == 0) {
		if (owner->sprite->IsDisplay()) {
			if (!owner->render.bgspdisp) {
				for (i = 0; i < 512; i++) {
					owner->render.bgspmod[i] = TRUE;
				}
				owner->render.bgspdisp = TRUE;
			}
		}
		else {
			if (owner->render.bgspdisp) {
				for (i = 0; i < 512; i++) {
					owner->render.bgspmod[i] = TRUE;
				}
				owner->render.bgspdisp = FALSE;
			}
		}
	}

	if ((owner->render.v_mul == 2) && !owner->render.lowres) {
		for (i = owner->render.first; i < owner->render.last; i++) {
			if ((i & 1) == 0) {
				owner->Text(i >> 1);
				owner->Grp(0, i >> 1);
				owner->Grp(1, i >> 1);
				owner->Grp(2, i >> 1);
				owner->Grp(3, i >> 1);
				owner->BGSprite(i >> 1);
				owner->Mix(i >> 1);
			}
		}
		owner->render.first = owner->render.last;
		return;
	}

	if ((owner->render.v_mul == 0) && owner->render.lowres) {
		for (i = owner->render.first; i < owner->render.last; i++) {
			owner->Text((i << 1) + 0);
			owner->Text((i << 1) + 1);
			owner->Grp(0, (i << 1) + 0);
			owner->Grp(0, (i << 1) + 1);
			owner->Grp(1, (i << 1) + 0);
			owner->Grp(1, (i << 1) + 1);
			owner->Grp(2, (i << 1) + 0);
			owner->Grp(2, (i << 1) + 1);
			owner->Grp(3, (i << 1) + 0);
			owner->Grp(3, (i << 1) + 1);
			owner->BGSprite((i << 1) + 0);
			owner->BGSprite((i << 1) + 1);
			owner->Mix((i << 1) + 0);
			owner->Mix((i << 1) + 1);
		}
		owner->render.first = owner->render.last;
		return;
	}

	for (i = owner->render.first; i < owner->render.last; i++) {
		owner->Text(i);
		owner->Grp(0, i);
		owner->Grp(1, i);
		owner->Grp(2, i);
		owner->Grp(3, i);
		owner->BGSprite(i);
		owner->Mix(i);
	}
	owner->render.first = owner->render.last;
}

void FASTCALL Px68kGraphicEngine::ComposeLine(Render *owner, int y)
{
	Render::render_t *rw;
	DWORD *q;
	DWORD *p;
	DWORD *r;
	DWORD *ptr[3];
	DWORD buf[1024];
	DWORD sprite_pri_buf[1024];
	const int len = owner->GetWorkAddr()->mixlen;
	const auto resolve = [owner, y](int index) -> DWORD * {
		int offset;
		DWORD *p;

		offset = (*owner->render.mixy[index] + y) & owner->render.mixand[index];
		p = owner->render.mixptr[index];
		ASSERT(p);
		p += (offset << owner->render.mixshift[index]);
		p += (*owner->render.mixx[index] & owner->render.mixand[index]);
		return p;
	};

	rw = owner->GetWorkAddr();

	if ((!rw->mix[y]) || (!rw->mixbuf)) {
		return;
	}
	if (rw->mixheight <= y) {
		return;
	}
	ASSERT(len > 0);

	rw->mix[y] = FALSE;
	q = &rw->mixbuf[rw->mixwidth * y];

	switch (rw->mixtype) {
		case 0:
			memset(q, 0, sizeof(DWORD) * len);
			return;

		case 1:
			p = resolve(0);
			memcpy(q, p, sizeof(DWORD) * len);
			return;

		case 2:
			p = resolve(0);
			r = resolve(1);
			if (ShouldApplyPx68kTrBlend(owner)) {
				ComposeTwoSources(q, p, r, len, TRUE);
			}
			else {
				RendMix02(q, p, r, rw->drawflag + (y << 6), len);
			}
			{
				BOOL has_pri = BuildPx68kSpritePriLine(owner, y, sprite_pri_buf, len);
				if (has_pri) {
					const VC *vc_device = owner->GetVCDevice();
					const VC::vc_t *vc = vc_device ? vc_device->GetWorkAddr() : NULL;
					if (vc && ((vc->vr2h & 0x5c) == 0x14)) {
						OverlaySpritePriLine(q, sprite_pri_buf, len, FALSE);
					}
					else if (vc && ((vc->vr2h & 0x5d) == 0x1c)) {
						OverlaySpritePriLine(q, sprite_pri_buf, len, TRUE);
					}
				}
			}
			return;

		case 3:
			p = resolve(0);
			r = resolve(1);
			if (ShouldApplyPx68kTrBlend(owner)) {
				ComposeTwoSources(q, p, r, len, TRUE);
			}
			else {
				RendMix03(q, p, r, rw->drawflag + (y << 6), len);
			}
			{
				BOOL has_pri = BuildPx68kSpritePriLine(owner, y, sprite_pri_buf, len);
				if (has_pri) {
					const VC *vc_device = owner->GetVCDevice();
					const VC::vc_t *vc = vc_device ? vc_device->GetWorkAddr() : NULL;
					if (vc && ((vc->vr2h & 0x5c) == 0x14)) {
						OverlaySpritePriLine(q, sprite_pri_buf, len, FALSE);
					}
					else if (vc && ((vc->vr2h & 0x5d) == 0x1c)) {
						OverlaySpritePriLine(q, sprite_pri_buf, len, TRUE);
					}
				}
			}
			return;

		case 4:
			owner->MixGrp(y, buf);
			memcpy(q, buf, sizeof(DWORD) * len);
			return;

		case 5:
			owner->MixGrp(y, buf);
			p = resolve(0);
			if (ShouldApplyPx68kTrBlend(owner)) {
				ComposeTwoSources(q, p, buf, len, TRUE);
			}
			else {
				RendMix03(q, p, buf, rw->drawflag + (y << 6), len);
			}
			{
				BOOL has_pri = BuildPx68kSpritePriLine(owner, y, sprite_pri_buf, len);
				if (has_pri) {
					const VC *vc_device = owner->GetVCDevice();
					const VC::vc_t *vc = vc_device ? vc_device->GetWorkAddr() : NULL;
					if (vc && ((vc->vr2h & 0x5c) == 0x14)) {
						OverlaySpritePriLine(q, sprite_pri_buf, len, FALSE);
					}
					else if (vc && ((vc->vr2h & 0x5d) == 0x1c)) {
						OverlaySpritePriLine(q, sprite_pri_buf, len, TRUE);
					}
				}
			}
			return;

		case 6:
			owner->MixGrp(y, buf);
			p = resolve(0);
			if (ShouldApplyPx68kTrBlend(owner)) {
				ComposeTwoSources(q, buf, p, len, TRUE);
			}
			else {
				RendMix03(q, buf, p, rw->drawflag + (y << 6), len);
			}
			{
				BOOL has_pri = BuildPx68kSpritePriLine(owner, y, sprite_pri_buf, len);
				if (has_pri) {
					const VC *vc_device = owner->GetVCDevice();
					const VC::vc_t *vc = vc_device ? vc_device->GetWorkAddr() : NULL;
					if (vc && ((vc->vr2h & 0x5c) == 0x14)) {
						OverlaySpritePriLine(q, sprite_pri_buf, len, FALSE);
					}
					else if (vc && ((vc->vr2h & 0x5d) == 0x1c)) {
						OverlaySpritePriLine(q, sprite_pri_buf, len, TRUE);
					}
				}
			}
			return;

		case 7:
			{
				int map[3];
				BuildPx68kMixMap(owner, map);
			ptr[0] = resolve(0);
			ptr[1] = resolve(1);
			ptr[2] = resolve(2);
				if (ShouldApplyPx68kTrBlend(owner)) {
					ComposeThreeSources(q, ptr[map[0]], ptr[map[1]], ptr[map[2]], len, TRUE);
				}
				else {
					RendMix04(q, ptr[map[0]], ptr[map[1]], ptr[map[2]], rw->drawflag + (y << 6), len);
				}
				{
					BOOL has_pri = BuildPx68kSpritePriLine(owner, y, sprite_pri_buf, len);
					if (has_pri) {
						const VC *vc_device = owner->GetVCDevice();
						const VC::vc_t *vc = vc_device ? vc_device->GetWorkAddr() : NULL;
						if (vc && ((vc->vr2h & 0x5c) == 0x14)) {
							OverlaySpritePriLine(q, sprite_pri_buf, len, FALSE);
						}
						else if (vc && ((vc->vr2h & 0x5d) == 0x1c)) {
							OverlaySpritePriLine(q, sprite_pri_buf, len, TRUE);
						}
					}
				}
			}
			return;

	case 8:
			{
				int map[3];
				BuildPx68kMixMap(owner, map);
			owner->MixGrp(y, buf);
			ptr[0] = resolve(0);
			ptr[1] = resolve(1);
			ptr[2] = buf;
				if (ShouldApplyPx68kTrBlend(owner)) {
					ComposeThreeSources(q, ptr[map[0]], ptr[map[1]], ptr[map[2]], len, TRUE);
				}
				else {
					RendMix04(q, ptr[map[0]], ptr[map[1]], ptr[map[2]], rw->drawflag + (y << 6), len);
				}
				{
					BOOL has_pri = BuildPx68kSpritePriLine(owner, y, sprite_pri_buf, len);
					if (has_pri) {
						const VC *vc_device = owner->GetVCDevice();
						const VC::vc_t *vc = vc_device ? vc_device->GetWorkAddr() : NULL;
						if (vc && ((vc->vr2h & 0x5c) == 0x14)) {
							OverlaySpritePriLine(q, sprite_pri_buf, len, FALSE);
						}
						else if (vc && ((vc->vr2h & 0x5d) == 0x1c)) {
							OverlaySpritePriLine(q, sprite_pri_buf, len, TRUE);
						}
					}
				}
			}
			return;
	}

	if (owner->IsTransparencyEnabled()) {
		const DWORD *bg_line = owner->GetBGSpBuf();
		if (bg_line) {
			bg_line += (y << 9);
			for (int x = 0; x < len; ++x) {
				const DWORD src = bg_line[x];
				if (!IsVisiblePixel(src)) {
					continue;
				}
				if ((src & REND_PX68K_IBIT) != 0) {
					if (!IsVisiblePixel(q[x])) {
						q[x] = src;
					}
					else {
						q[x] = BlendPx68kSemi(src, q[x]);
					}
				}
			}
		}
	}

	ASSERT(FALSE);
}

void FASTCALL OriginalGraphicEngine::StartFrame(Render *owner)
{
	owner->StartFrameOriginal();
}

void FASTCALL OriginalGraphicEngine::EndFrame(Render *owner)
{
	owner->EndFrameOriginal();
}

void FASTCALL OriginalGraphicEngine::HSync(Render *owner, int raster)
{
	owner->HSyncOriginal(raster);
}

void FASTCALL OriginalGraphicEngine::SetCRTC(Render *owner)
{
	owner->SetCRTCOriginal();
}

void FASTCALL OriginalGraphicEngine::SetVC(Render *owner)
{
	owner->SetVCOriginal();
}

void FASTCALL OriginalGraphicEngine::Video(Render *owner)
{
	owner->Video();
}

void FASTCALL OriginalGraphicEngine::Process(Render *owner)
{
	ProcessCommon(owner);
}

void FASTCALL Px68kGraphicEngine::StartFrame(Render *owner)
{
	owner->StartFrameOriginal();
}

void FASTCALL Px68kGraphicEngine::EndFrame(Render *owner)
{
	owner->EndFrameOriginal();
}

void FASTCALL Px68kGraphicEngine::HSync(Render *owner, int raster)
{
	owner->HSyncOriginal(raster);
}

void FASTCALL Px68kGraphicEngine::SetCRTC(Render *owner)
{
	owner->SetCRTCOriginal();
}

void FASTCALL Px68kGraphicEngine::SetVC(Render *owner)
{
	owner->SetVCOriginal();
}

void FASTCALL Px68kGraphicEngine::Video(Render *owner)
{
	owner->Video();
}

void FASTCALL Px68kGraphicEngine::Process(Render *owner)
{
	int i;

	if (owner->render.first >= owner->render.last) {
		return;
	}

	if (owner->render.vc) {
		owner->ComposeVideo();
	}

	if (owner->render.contrast) {
		owner->Contrast();
	}

	if (owner->render.palette) {
		owner->Palette();
	}

	if (owner->render.first == 0) {
		if (owner->sprite->IsDisplay()) {
			if (!owner->render.bgspdisp) {
				for (i = 0; i < 512; i++) {
					owner->render.bgspmod[i] = TRUE;
				}
				owner->render.bgspdisp = TRUE;
			}
		}
		else {
			if (owner->render.bgspdisp) {
				for (i = 0; i < 512; i++) {
					owner->render.bgspmod[i] = TRUE;
				}
				owner->render.bgspdisp = FALSE;
			}
		}
	}

	if ((owner->render.v_mul == 2) && !owner->render.lowres) {
		for (i = owner->render.first; i < owner->render.last; i++) {
			if ((i & 1) == 0) {
				owner->Text(i >> 1);
				owner->Grp(0, i >> 1);
				owner->Grp(1, i >> 1);
				owner->Grp(2, i >> 1);
				owner->Grp(3, i >> 1);
				owner->BGSprite(i >> 1);
				owner->MixFastLine(i >> 1, i >> 1);
			}
		}
		owner->render.first = owner->render.last;
		return;
	}

	if ((owner->render.v_mul == 0) && owner->render.lowres) {
		for (i = owner->render.first; i < owner->render.last; i++) {
			owner->Text((i << 1) + 0);
			owner->Text((i << 1) + 1);
			owner->Grp(0, (i << 1) + 0);
			owner->Grp(0, (i << 1) + 1);
			owner->Grp(1, (i << 1) + 0);
			owner->Grp(1, (i << 1) + 1);
			owner->Grp(2, (i << 1) + 0);
			owner->Grp(2, (i << 1) + 1);
			owner->Grp(3, (i << 1) + 0);
			owner->Grp(3, (i << 1) + 1);
			owner->BGSprite((i << 1) + 0);
			owner->BGSprite((i << 1) + 1);
			owner->MixFastLine((i << 1) + 0, (i << 1) + 0);
			owner->MixFastLine((i << 1) + 1, (i << 1) + 1);
		}
		owner->render.first = owner->render.last;
		return;
	}

	for (i = owner->render.first; i < owner->render.last; i++) {
		owner->Text(i);
		owner->Grp(0, i);
		owner->Grp(1, i);
		owner->Grp(2, i);
		owner->Grp(3, i);
		owner->BGSprite(i);
		owner->MixFastLine(i, i);
	}
	owner->render.first = owner->render.last;
}
