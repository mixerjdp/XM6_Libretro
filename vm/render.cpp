//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ?o?h?D(ytanaka@ipc-tokai.or.jp)
//	[ ?????_?? ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "crtc.h"
#include "vc.h"
#include "tvram.h"
#include "gvram.h"
#include "sprite.h"
#include "rend_soft.h"
#include "render.h"

BOOL FASTCALL IsCMOV(void);

//===========================================================================
//
//	?????_??
//
//===========================================================================
//#define REND_LOG

//---------------------------------------------------------------------------
//
//	????`
//
//---------------------------------------------------------------------------
#define REND_COLOR0		0x80000000		// ?J???[0?t???O(rend_asm.asm??g?p)
#define REND_PX68K_IBIT	0x40000000		// px68k Ibit marker (XRGB8888: top byte ignored)

static int FASTCALL CalcBGHAdjustPixels(int compositor_mode, const CRTC *crtc, const Sprite *sprite)
{
	if (compositor_mode != Render::compositor_fast) {
		return 0;
	}

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

class Render::Backend
{
public:
	explicit Backend(int m) : mode(m)
	{
	}

	void Activate(Render *owner)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->InvalidateAll();
		}
	}

	void StartFrame(Render *owner)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->StartFrameFast();
			return;
		}
		owner->StartFrameOriginal();
	}

	void EndFrame(Render *owner)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->EndFrameFast();
			return;
		}
		owner->EndFrameOriginal();
	}

	void HSync(Render *owner, int raster)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->HSyncFast(raster);
			return;
		}
		owner->HSyncOriginal(raster);
	}

	void SetCRTC(Render *owner)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->SetCRTCFast();
			return;
		}
		owner->SetCRTCOriginal();
	}

	void SetVC(Render *owner)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->SetVCFast();
			return;
		}
		owner->SetVCOriginal();
	}

private:
	int mode;
};
//---------------------------------------------------------------------------
//
//	?R???X?g???N?^
//
//---------------------------------------------------------------------------
Render::Render(VM *p) : Device(p)
{
	// ?f?o?C?XID???????
	dev.id = MAKEID('R', 'E', 'N', 'D');
	dev.desc = "Renderer";

	// ?f?o?C?X?|?C???^
	crtc = NULL;
	vc = NULL;
	sprite = NULL;
	backend = NULL;
	backend_original = NULL;
	backend_fast = NULL;
	compositor_mode = compositor_original;
	palbuf_original = NULL;
	palbuf_fast = NULL;
	fast_fallback_count = 0;
	transparency_enabled = TRUE;
	original_bg0_render_enabled = TRUE;
	render.fast_stamp_counter = 1;
	memset(render.fast_mix_stamp, 0, sizeof(render.fast_mix_stamp));
	memset(render.fast_mix_done, 0, sizeof(render.fast_mix_done));
	memset(render.fast_bg_stamp, 0, sizeof(render.fast_bg_stamp));
	memset(render.fast_bg_done, 0, sizeof(render.fast_bg_done));

	// ???[?N?G???A??????(CRTC)
	render.crtc = FALSE;
	render.width = 768;
	render.h_mul = 1;
	render.height = 512;
	render.v_mul = 1;

	// ???[?N?G???A??????(?p???b?g)
	render.palbuf = NULL;
	render.palptr = NULL;
	render.palvc = NULL;

	// ???[?N?G???A??????(?e?L?X?g)
	render.textflag = NULL;
	render.texttv = NULL;
	render.textbuf = NULL;
	render.textout = NULL;

	// ???[?N?G???A??????(?O???t?B?b?N)
	render.grpflag = NULL;
	render.grpgv = NULL;
	render.grpbuf[0] = NULL;
	render.grpbuf[1] = NULL;
	render.grpbuf[2] = NULL;
	render.grpbuf[3] = NULL;

	// ???[?N?G???A??????(PCG,?X?v???C?g,BG)
	render.pcgbuf = NULL;
	render.spptr = NULL;
	render.bgspbuf = NULL;
	render.zero = NULL;
	render.bgptr[0] = NULL;
	render.bgptr[1] = NULL;

	// ???[?N?G???A??????(????)
	render.mixbuf = NULL;
	render.mixwidth = 0;
	render.mixheight = 0;
	render.mixlen = 0;
	render.mixtype = 0;
	memset(render.mixptr, 0, sizeof(render.mixptr));
	memset(render.mixshift, 0, sizeof(render.mixshift));
	memset(render.mixx, 0, sizeof(render.mixx));
	memset(render.mixy, 0, sizeof(render.mixy));
	memset(render.mixand, 0, sizeof(render.mixand));
	memset(render.mixmap, 0, sizeof(render.mixmap));

	// ???[?N?G???A??????(?`??)
	render.drawflag = NULL;

	// ?????
	cmov = FALSE;
}

//---------------------------------------------------------------------------
//
//	??????
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Init()
{
	int i;

	ASSERT(this);

	// ??{?N???X
	if (!Device::Init()) {
		return FALSE;
	}

	// CRTC?�E��E�
	crtc = (CRTC*)vm->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
	ASSERT(crtc);

	// VC?�E��E�
	vc = (VC*)vm->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(vc);

	// ?p???b?g?o?b?t?@?m??(4MB x2: original + px68k-safe for fast)
	palbuf_original = NULL;
	palbuf_fast = NULL;
	try {
		palbuf_original = new DWORD[0x10000 * 16];
		palbuf_fast = new DWORD[0x10000 * 16];
	}
	catch (...) {
		if (palbuf_original) {
			delete[] palbuf_original;
			palbuf_original = NULL;
		}
		if (palbuf_fast) {
			delete[] palbuf_fast;
			palbuf_fast = NULL;
		}
		return FALSE;
	}
	if (!palbuf_original || !palbuf_fast) {
		if (palbuf_original) {
			delete[] palbuf_original;
			palbuf_original = NULL;
		}
		if (palbuf_fast) {
			delete[] palbuf_fast;
			palbuf_fast = NULL;
		}
		return FALSE;
	}
	render.palbuf = palbuf_original;

	// ?e?L?X?gVRAM?o?b?t?@?m??(4.7MB)
	try {
		render.textflag = new BOOL[1024 * 32];
		render.textbuf = new BYTE[1024 * 512];
		render.textout = new DWORD[1024 * (1024 + 1)];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.textflag) {
		return FALSE;
	}
	if (!render.textbuf) {
		return FALSE;
	}
	if (!render.textout) {
		return FALSE;
	}
	for (i=0; i<1024*32; i++) {
		render.textflag[i] = TRUE;
	}
	for (i=0; i<1024; i++) {
		render.textmod[i] = TRUE;
	}

	// ?O???t?B?b?NVRAM?o?b?t?@?m??(8.2MB)
	try {
		render.grpflag = new BOOL[512 * 32 * 4];
		render.grpbuf[0] = new DWORD[512 * 1024 * 4];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.grpflag) {
		return FALSE;
	}
	if (!render.grpbuf[0]) {
		return FALSE;
	}
	render.grpbuf[1] = render.grpbuf[0] + 512 * 1024;
	render.grpbuf[2] = render.grpbuf[1] + 512 * 1024;
	render.grpbuf[3] = render.grpbuf[2] + 512 * 1024;
	memset(render.grpflag, 0, sizeof(BOOL) * 32 * 512 * 4);
	for (i=0; i<512*4; i++) {
		render.grpmod[i] = FALSE;
		render.grppal[i] = TRUE;
	}

	// PCG?o?b?t?@?m??(4MB)
	try {
		render.pcgbuf = new DWORD[ 16 * 256 * 16 * 16 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.pcgbuf) {
		return FALSE;
	}

	// ?X?v???C?g?|?C???^?m??(256KB)
	try {
		render.spptr = new DWORD*[ 128 * 512 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.spptr) {
		return FALSE;
	}

	// BG?|?C???^?m??(768KB)
	try {
		render.bgptr[0] = new DWORD*[ (64 * 2) * 1024 ];
		memset(render.bgptr[0], 0, sizeof(DWORD*) * (64 * 2 * 1024));
		render.bgptr[1] = new DWORD*[ (64 * 2) * 1024 ];	// from 512 to 1024 since version2.04
		memset(render.bgptr[1], 0, sizeof(DWORD*) * (64 * 2 * 1024));
	}
	catch (...) {
		return FALSE;
	}
	if (!render.bgptr[0]) {
		return FALSE;
	}
	if (!render.bgptr[1]) {
		return FALSE;
	}
	memset(render.bgall, 0, sizeof(render.bgall));
	memset(render.bgmod, 0, sizeof(render.bgmod));

	// BG/?X?v???C?g?o?b?t?@?m??(1MB)
	try {
		render.bgspbuf = new DWORD[ 512 * 512 + 16];	// +16??b??[?u
	}
	catch (...) {
		return FALSE;
	}
	if (!render.bgspbuf) {
		return FALSE;
	}

	// ?`??t???O?o?b?t?@?m??(256KB)
	try {
		render.drawflag = new BOOL[64 * 1024];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.drawflag) {
		return FALSE;
	}
	memset(render.drawflag, 0, sizeof(BOOL) * 64 * 1024);

	// ?p???b?g??
	MakePalette();

	// ????????[?N?G???A
	render.contlevel = 0;
	cmov = ::IsCMOV();

	try {
		backend_original = new Backend(compositor_original);
		backend_fast = new Backend(compositor_fast);
	}
	catch (...) {
		return FALSE;
	}
	if (!backend_original || !backend_fast) {
		return FALSE;
	}
	backend = backend_original;
	compositor_mode = compositor_original;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	?N???[???A?b?v
//
//---------------------------------------------------------------------------
void FASTCALL Render::Cleanup()
{
	int i;

	ASSERT(this);

	if (backend_original) {
		delete backend_original;
		backend_original = NULL;
	}
	if (backend_fast) {
		delete backend_fast;
		backend_fast = NULL;
	}
	backend = NULL;
	compositor_mode = compositor_original;

	// ?`??t???O
	if (render.drawflag) {
		delete[] render.drawflag;
		render.drawflag = NULL;
	}

	// BG/?X?v???C?g?o?b?t?@
	if (render.bgspbuf) {
		delete[] render.bgspbuf;
		render.bgspbuf = NULL;
	}

	// BG?|?C???^
	if (render.bgptr[0]) {
		delete[] render.bgptr[0];
		render.bgptr[0] = NULL;
	}
	if (render.bgptr[1]) {
		delete[] render.bgptr[1];
		render.bgptr[1] = NULL;
	}

	// ?X?v???C?g?|?C???^
	if (render.spptr) {
		delete[] render.spptr;
		render.spptr = NULL;
	}

	// PCG?o?b?t?@
	if (render.pcgbuf) {
		delete[] render.pcgbuf;
		render.pcgbuf = NULL;
	}

	// ?O???t?B?b?NVRAM?o?b?t?@
	if (render.grpflag) {
		delete[] render.grpflag;
		render.grpflag = NULL;
	}
	if (render.grpbuf[0]) {
		delete[] render.grpbuf[0];
		for (i=0; i<4; i++) {
			render.grpbuf[i] = NULL;
		}
	}

	// ?e?L?X?gVRAM?o?b?t?@
	if (render.textflag) {
		delete[] render.textflag;
		render.textflag = NULL;
	}
	if (render.textbuf) {
		delete[] render.textbuf;
		render.textbuf = NULL;
	}
	if (render.textout) {
		delete[] render.textout;
		render.textout = NULL;
	}

	// ?p???b?g?o?b?t?@ (original + fast)
	if (palbuf_original) {
		delete[] palbuf_original;
		palbuf_original = NULL;
	}
	if (palbuf_fast) {
		delete[] palbuf_fast;
		palbuf_fast = NULL;
	}
	render.palbuf = NULL;

	// ??{?N???X??
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	???Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::Reset()
{
	TVRAM *tvram;
	GVRAM *gvram;
	int i;
	int j;
	int k;
	DWORD **ptr;

	ASSERT(this);
	LOG0(Log::Normal, "???Z?b?g");

	// ?r?f?I?R???g???[?????|?C???^?�E��E�
	ASSERT(vc);
	render.palvc = (const WORD*)vc->GetPalette();

	// ?e?L?X?gVRAM???|?C???^?�E��E�
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);
	render.texttv = tvram->GetTVRAM();

	// ?O???t?B?b?NVRAM???|?C???^?�E��E�
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);
	render.grpgv = gvram->GetGVRAM();

	// ?X?v???C?g?R???g???[?????|?C???^?�E��E�
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);
	render.sprmem = sprite->GetPCG() - 0x8000;

	// ???[?N?G???A??????
	render.first = 0;
	render.last = 0;
	render.enable = TRUE;
	render.act = TRUE;
	render.count = (compositor_mode == compositor_fast) ? 0 : 2;
	memset(render.mix, 0, sizeof(render.mix));
	memset(render.draw, 0, sizeof(render.draw));
	memset(render.mixptr, 0, sizeof(render.mixptr));
	memset(render.mixshift, 0, sizeof(render.mixshift));
	memset(render.mixx, 0, sizeof(render.mixx));
	memset(render.mixy, 0, sizeof(render.mixy));
	memset(render.mixand, 0, sizeof(render.mixand));
	memset(render.mixmap, 0, sizeof(render.mixmap));
	render.mixpage = 0;
	fast_fallback_count = 0;
	render.fast_stamp_counter = 1;
	memset(render.fast_mix_stamp, 0, sizeof(render.fast_mix_stamp));
	memset(render.fast_mix_done, 0, sizeof(render.fast_mix_done));
	memset(render.fast_bg_stamp, 0, sizeof(render.fast_bg_stamp));
	memset(render.fast_bg_done, 0, sizeof(render.fast_bg_done));

	if (render.mixbuf && (render.mixwidth > 0) && (render.mixheight > 0)) {
		memset(render.mixbuf, 0, sizeof(DWORD) * render.mixwidth * render.mixheight);
	}
	if (render.textbuf) {
		memset(render.textbuf, 0, sizeof(BYTE) * 1024 * 512);
	}
	if (render.textout) {
		memset(render.textout, 0, sizeof(DWORD) * 1024 * (1024 + 1));
	}
	if (render.grpbuf[0]) {
		memset(render.grpbuf[0], 0, sizeof(DWORD) * 512 * 1024 * 4);
	}
	if (render.bgspbuf) {
		memset(render.bgspbuf, 0, sizeof(DWORD) * (512 * 512 + 16));
	}

	// ???[?N?G???A??????(crtc, vc)
	render.crtc = FALSE;
	render.vc = FALSE;

	// ???[?N?G???A??????(?R???g???X?g)
	render.contrast = FALSE;

	// ???[?N?G???A??????(?p???b?g)
	render.palette = FALSE;
	render.palptr = render.palbuf;

	// ???[?N?G???A??????(?e?L?X?g)
	render.texten = FALSE;
	render.textx = 0;
	render.texty = 0;

	// ???[?N?G???A??????(?O???t?B?b?N)
	for (i=0; i<4; i++) {
		render.grpen[i] = FALSE;
		render.grpx[i] = 0;
		render.grpy[i] = 0;
	}
	render.grptype = 4;

	// ???[?N?G???A??????(PCG)
	// ???Z?b?g?????BG,Sprite???????\?????????PCG????g?p
	memset(render.pcgready, 0, sizeof(render.pcgready));
	memset(render.pcguse, 0, sizeof(render.pcguse));
	memset(render.pcgpal, 0, sizeof(render.pcgpal));

	// ???[?N?G???A??????(?X?v???C?g)
	memset(render.spptr, 0, sizeof(DWORD*) * 128 * 512);
	memset(render.spreg, 0, sizeof(render.spreg));
	memset(render.spuse, 0, sizeof(render.spuse));

	// ???[?N?G???A??????(BG)
	memset(render.bgreg, 0, sizeof(render.bgreg));
	render.bgdisp[0] = FALSE;
	render.bgdisp[1] = FALSE;
	render.bgarea[0] = FALSE;
	render.bgarea[1] = TRUE;
	render.bgsize = FALSE;
	render.bgx[0] = 0;
	render.bgx[1] = 0;
	render.bgy[0] = 0;
	render.bgy[1] = 0;

	// ???[?N?G???A??????(BG/?X?v???C?g)
	render.bgspflag = FALSE;
	render.bgspdisp = FALSE;
	memset(render.bgspmod, 0, sizeof(render.bgspmod));

	// BG???????????????(?????0000)
	for (i=0; i<(64*64); i++) {
		render.bgreg[0][i] = 0x10000;
		render.bgreg[1][i] = 0x10000;
	}
	render.pcgready[0] = TRUE;
	render.pcguse[0] = (64 * 64) * 2;
	render.pcgpal[0] = (64 * 64) * 2;
	memset(render.pcgbuf, 0, (16 * 16) * sizeof(DWORD));
	for (i=0; i<64; i++) {
		ptr = &render.bgptr[0][i << 3];
		for (j=0; j<64; j++) {
			for (k=0; k<8; k++) {
				ptr[(k << 7) + 0] = &render.pcgbuf[k << 4];
				ptr[(k << 7) + 1] = (DWORD*)0x10000;
			}
			ptr += 2;
		}
		ptr = &render.bgptr[0][(512 + (i << 3)) << 7];
		for (j=0; j<64; j++) {
			for (k=0; k<8; k++) {
				ptr[(k << 7) + 0] = &render.pcgbuf[k << 4];
				ptr[(k << 7) + 1] = (DWORD*)0x10000;
			}
			ptr += 2;
		}
		ptr = &render.bgptr[1][i << 10];
		for (j=0; j<64; j++) {
			for (k=0; k<8; k++) {
				ptr[(k << 7) + 0] = &render.pcgbuf[k << 4];
				ptr[(k << 7) + 1] = (DWORD*)0x10000;
			}
			ptr += 2;
		}
	}

	// ???[?N?G???A??????(????)
	render.mixtype = 0;
	if (backend) {
		backend->Activate(this);
	}
}

//---------------------------------------------------------------------------
//
//	?Z?[?u
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Save(Fileio *fio, int ver)
{
	ASSERT(this);
	LOG0(Log::Normal, "?Z?[?u");


	return TRUE;
}

//---------------------------------------------------------------------------
//
//	???[?h
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Load(Fileio *fio, int ver)
{
	ASSERT(this);
	LOG0(Log::Normal, "???[?h");

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	???K?p
//
//---------------------------------------------------------------------------
void FASTCALL Render::ApplyCfg(const Config *config)
{
	ASSERT(config);
	LOG0(Log::Normal, "???K?p");
}

//---------------------------------------------------------------------------
//
//	?t???[???J?n
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::SetCompositorMode(int mode)
{
	Backend *next = NULL;
	DWORD *next_palbuf = NULL;

	switch (mode) {
	case compositor_original:
		next = backend_original;
		next_palbuf = palbuf_original;
		break;
	case compositor_fast:
		next = backend_fast;
		next_palbuf = palbuf_fast;
		break;
	default:
		return FALSE;
	}

	compositor_mode = mode;
	if (!next) {
		return TRUE;
	}

	backend = next;
	if (next_palbuf && render.palbuf != next_palbuf) {
		render.palbuf = next_palbuf;
		render.palptr = render.palbuf + (render.contlevel << 16);
		InvalidateAll();
	}
	backend->Activate(this);
	return TRUE;
}

void FASTCALL Render::StartFrame()
{
	if (backend) {
		backend->StartFrame(this);
		return;
	}
	StartFrameOriginal();
}

void FASTCALL Render::EndFrame()
{
	if (backend) {
		backend->EndFrame(this);
		return;
	}
	EndFrameOriginal();
}

void FASTCALL Render::HSync(int raster)
{
	if (backend) {
		backend->HSync(this, raster);
		return;
	}
	HSyncOriginal(raster);
}

void FASTCALL Render::SetCRTC()
{
	if (backend) {
		backend->SetCRTC(this);
		return;
	}
	SetCRTCOriginal();
}

void FASTCALL Render::SetVC()
{
	if (backend) {
		backend->SetVC(this);
		return;
	}
	SetVCOriginal();
}

void FASTCALL Render::ForceRecompose()
{
	InvalidateAll();
}

void FASTCALL Render::InvalidateFrame()
{
	int i;
	DWORD stamp;

	render.vc = TRUE;
	render.palette = TRUE;

	memset(render.palmod, 1, sizeof(render.palmod));
	memset(render.mix, 1, sizeof(render.mix));
	memset(render.textmod, 1, sizeof(render.textmod));
	memset(render.textpal, 1, sizeof(render.textpal));
	memset(render.grpmod, 1, sizeof(render.grpmod));
	memset(render.grppal, 1, sizeof(render.grppal));
	memset(render.bgspmod, 1, sizeof(render.bgspmod));
	stamp = ++render.fast_stamp_counter;
	for (i=0; i<1024; i++) {
		render.fast_mix_stamp[i] = stamp;
		render.fast_mix_done[i] = 0;
	}
	for (i=0; i<512; i++) {
		render.fast_bg_stamp[i] = stamp;
		render.fast_bg_done[i] = 0;
	}
	if (render.drawflag) {
		memset(render.drawflag, 1, sizeof(BOOL) * (64 * 1024));
	}
}

void FASTCALL Render::InvalidateAll()
{
	render.crtc = TRUE;
	InvalidateFrame();
}

void FASTCALL Render::HSyncOriginal(int raster)
{
	render.last = raster;
	if (render.act) {
		Process();
	}
}

void FASTCALL Render::StartFrameOriginal()
{
	CRTC::crtc_t crtcdata;
	int i;

	ASSERT(this);

	// ????t???[????X?L?b?v????
	if ((render.count != 0) || !render.enable) {
		render.act = FALSE;
		return;
	}

	// ????t???[????????_?????O????
	render.act = TRUE;

	// ???X?^??N???A
	render.first = 0;
	render.last = -1;

	// CRTC?t???O?????
	if (render.crtc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "CRTC????");
#endif	// REND_LOG

		// ?f?[?^?�E��E�
		crtc->GetCRTC(&crtcdata);

		// h_dots?Av_dots??0?????
		if ((crtcdata.h_dots == 0) || (crtcdata.v_dots == 0)) {
			return;
		}

		// ????R?s?[
		render.width = crtcdata.h_dots;
		render.h_mul = crtcdata.h_mul;
		render.height = crtcdata.v_dots;
		render.v_mul = crtcdata.v_mul;
		render.lowres = crtcdata.lowres;
		if ((render.v_mul == 2) && !render.lowres) {
			render.height >>= 1;
		}

		// ?????o?b?t?@???????????
		render.mixlen = render.width;
		if (render.mixwidth < render.width) {
			render.mixlen = render.mixwidth;
		}

		// ?X?v???C?g???Z?b?g(mixlen??????????)
		SpriteReset();

		// ?S???C??????
		for (i=0; i<1024; i++) {
			render.mix[i] = TRUE;
		}

		// ?I?t
		render.crtc = FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	?t???[???I??
//
//---------------------------------------------------------------------------
void FASTCALL Render::EndFrameOriginal()
{
	ASSERT(this);

	// ??????�E��E�??????
	if (!render.act) {
		return;
	}

	// ??????????X?^?????
	if (render.last > 0) {
		render.last = render.height;
		Process();
	}

	// ?J?E???gUp
	render.count++;

	// ??????
	render.act = FALSE;
}

//---------------------------------------------------------------------------
//
//	?????o?b?t?@?Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetMixBuf(DWORD *buf, int width, int height)
{
	int i;

	ASSERT(this);
	ASSERT(width >= 0);
	ASSERT(height >= 0);

	// ???
	render.mixbuf = buf;
	render.mixwidth = width;
	render.mixheight = height;

	// ?????o?b?t?@???????????
	render.mixlen = render.width;
	if (render.mixwidth < render.width) {
		render.mixlen = render.mixwidth;
	}

	// ???????????w??
	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	CRTC?Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetCRTCOriginal()
{
	int i;
	int from;
	DWORD stamp;

	ASSERT(this);

	// ?t???OON???
	render.crtc = TRUE;
	render.vc = TRUE;

	if ((compositor_mode == compositor_fast) && render.act) {
		from = render.last;
		if (from < 0) {
			from = 0;
		}
		stamp = ++render.fast_stamp_counter;
		for (i=from; i<render.height && i<1024; i++) {
			render.mix[i] = TRUE;
			render.fast_mix_stamp[i] = stamp;
			render.fast_mix_done[i] = 0;
			render.bgspmod[i & 0x1ff] = TRUE;
			render.fast_bg_stamp[i & 0x1ff] = stamp;
			render.fast_bg_done[i & 0x1ff] = 0;
		}
	}
}

//---------------------------------------------------------------------------
//
//	VC?Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetVCOriginal()
{
	int i;
	int from;
	DWORD stamp;

	ASSERT(this);

	// ?t???OON???
	render.vc = TRUE;

	if ((compositor_mode == compositor_fast) && render.act) {
		from = render.last;
		if (from < 0) {
			from = 0;
		}
		stamp = ++render.fast_stamp_counter;
		for (i=from; i<render.height && i<1024; i++) {
			render.mix[i] = TRUE;
			render.fast_mix_stamp[i] = stamp;
			render.fast_mix_done[i] = 0;
			render.bgspmod[i & 0x1ff] = TRUE;
			render.fast_bg_stamp[i & 0x1ff] = stamp;
			render.fast_bg_done[i & 0x1ff] = 0;
		}
	}
}

//---------------------------------------------------------------------------
//
//	VC????
//
//---------------------------------------------------------------------------
void FASTCALL Render::Video()
{
	const VC::vc_t *p;
	const CRTC::crtc_t *q;
	int type;
	int i;
	int j;
	int sp;
	int gr;
	int tx;
	int map[4];
	DWORD *ptr[4];
	DWORD shift[4];
	DWORD an[4];

	// VC?t???O??~??
	render.vc = FALSE;

	// ?t???OON
	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}

	// VC?f?[?^?ACRTC?f?[?^??�E��E�
	p = vc->GetWorkAddr();
	q = crtc->GetWorkAddr();

	// ?e?L?X?g?C?l?[?u??
	if (p->ton && !q->tmem) {
		render.texten = TRUE;
	}
	else {
		render.texten = FALSE;
	}

	// ?O???t?B?b?N?^?C?v
	type = 0;
	if (!p->siz) {
		type = (int)(p->col + 1);
		if ((compositor_mode == compositor_fast) && (p->col == 2)) {
			type = 2;
		}
	}
	if (type != render.grptype) {
		render.grptype = type;
		for (i=0; i<512*4; i++) {
			render.grppal[i] = TRUE;
		}
	}

	// ?O???t?B?b?N?C?l?[?u???A?D??x?}?b?v
	render.mixpage = 0;
	for (i=0; i<4; i++) {
		render.grpen[i] = FALSE;
		map[i] = -1;
		an[i] = 512 - 1;
	}
	render.mixpage = 0;
	if (!q->gmem) {
		switch (render.grptype) {
			// 1024x1024x1
			case 0:
				render.grpen[0] = p->gon;
				if (render.grpen[0]) {
					map[0] = 0;
					render.mixpage = 1;
					an[0] = 1024 - 1;
				}
				break;
			// 512x512x4
			case 1:
				for (i=0; i<4; i++) {
					if (p->gs[i]) {
						ASSERT((p->gp[i] >= 0) && (p->gp[i] < 4));
						render.grpen[ p->gp[i] ] = TRUE;
						map[i] = p->gp[i];
						render.mixpage++;
					}
				}
				break;
			// 512x512x2
			case 2:
				if (compositor_mode == compositor_fast) {
				// px68k semantics:
				// - Only GS0 and GS2 are meaningful enables in 256-color mode.
				// - Page priority is decided by comparing GP0 vs GP2 (equal => page0 wins).
				// - The two 8-bit pages are represented as blocks 0 and 2 in XM6.
				{
					const BOOL page0_on = (BOOL)p->gs[0];
					const BOOL page2_on = (BOOL)p->gs[2];
					const BOOL page0_top = (BOOL)(((int)p->gp[0]) <= ((int)p->gp[2]));

					if (page0_on) { render.grpen[0] = TRUE; }
					if (page2_on) { render.grpen[2] = TRUE; }

					// Build bottom -> top (later layers overlay earlier ones).
					if (page0_top) {
						if (page2_on) { map[0] = 2; render.mixpage++; }
						if (page0_on) { map[1] = 0; render.mixpage++; }
					}
					else {
						if (page0_on) { map[0] = 0; render.mixpage++; }
						if (page2_on) { map[1] = 2; render.mixpage++; }
					}
				}
				}
				else {
				for (i=0; i<2; i++) {
					// page0 check
					if ((p->gp[i * 2 + 0] == 0) && (p->gp[i * 2 + 1] == 1)) {
						if (p->gs[i * 2 + 0]) {
							map[i] = 0;
							render.grpen[0] = TRUE;
							render.mixpage++;
						}
					}
					// page1 check
					if ((p->gp[i * 2 + 0] == 2) && (p->gp[i * 2 + 1] == 3)) {
						if (p->gs[i * 2 + 0]) {
							map[i] = 2;
							render.grpen[2] = TRUE;
							render.mixpage++;
						}
					}
				}
				}
				break;
			// 512x512x1
			case 3:
			case 4:
				render.grpen[0] = TRUE;
				render.mixpage = 1;
				map[0] = 0;
				for (i=0; i<4; i++) {
					if (!p->gs[i]) {
						render.grpen[0] = FALSE;
						render.mixpage = 0;
						map[0] = -1;
						break;
					}
				}
				break;
			default:
				ASSERT(FALSE);
				break;
		}
	}

	// ?O???t?B?b?N?o?b?t?@??Z?b?g
	j = 0;
	for (i=0; i<4; i++) {
		if (map[i] >= 0) {
			ASSERT((map[i] >= 0) && (map[i] <= 3));
			ptr[j] = render.grpbuf[ map[i] ];
			if (render.grptype == 0) {
				shift[j] = 11;
			}
			else {
				shift[j] = 10;
			}
			ASSERT(j <= i);
			map[j] = map[i];
			j++;
		}
	}

	// ?D?????�E��E�
	tx = p->tx;
	sp = p->sp;
	gr = p->gr;

	// ?^?C?v??????
	render.mixtype = 0;

	// BG/?X?v???C?g?\??????
	if ((q->hd >= 2) || (!p->son)) {
		if (render.bgspflag) {
			// BG/?X?v???C?g?\??ON->OFF
			render.bgspflag = FALSE;
			for (i=0; i<512; i++) {
				render.bgspmod[i] = TRUE;
			}
			render.bgspdisp = sprite->IsDisplay();
		}
	}
	else {
		if (!render.bgspflag) {
			// BG/?X?v???C?g?\??OFF->ON
			render.bgspflag = TRUE;
			for (i=0; i<512; i++) {
				render.bgspmod[i] = TRUE;
			}
			render.bgspdisp = sprite->IsDisplay();
		}
	}

	// ???(q->hd >= 2?????X?v???C?g????)
	if ((q->hd >= 2) || (!p->son)) {
		// ?X?v???C?g???
		if (!render.texten) {
			// ?e?L?X?g???
			if (render.mixpage == 0) {
				// ?O???t?B?b?N???(type=0)
				render.mixtype = 0;
				return;
			}
			if (render.mixpage == 1) {
				// ?O???t?B?b?N1????(type=1)
				render.mixptr[0] = ptr[0];
				render.mixshift[0] = shift[0];
				render.mixx[0] = &render.grpx[ map[0] ];
				render.mixy[0] = &render.grpy[ map[0] ];
				render.mixand[0] = an[0];
				render.mixtype = 1;
				return;
			}
			if (render.mixpage == 2) {
				// ?O???t?B?b?N2????(type=2)
				for (i=0; i<2; i++) {
					render.mixptr[i] = ptr[i];
					render.mixshift[i] = shift[i];
					render.mixx[i] = &render.grpx[ map[i] ];
					render.mixy[i] = &render.grpy[ map[i] ];
					render.mixand[i] = an[i];
				}
				render.mixtype = 2;
				return;
			}
			ASSERT((render.mixpage == 3) || (render.mixpage == 4));
			// ?O???t?B?b?N3??????(type=4)
			for (i=0; i<render.mixpage; i++) {
				render.mixptr[i + 4] = ptr[i];
				render.mixshift[i + 4] = shift[i];
				render.mixx[i + 4] = &render.grpx[ map[i] ];
				render.mixy[i + 4] = &render.grpy[ map[i] ];
				render.mixand[i + 4] = an[i];
			}
			render.mixtype = 4;
			return;
		}
		// ?e?L?X?g????
		if (render.mixpage == 0) {
			// ?O???t?B?b?N????B?e?L?X?g???(type=1)
			render.mixptr[0] = render.textout;
			render.mixshift[0] = 10;
			render.mixx[0] = &render.textx;
			render.mixy[0] = &render.texty;
			render.mixand[0] = 1024 - 1;
			render.mixtype = 1;
				return;
		}
		if (render.mixpage == 1) {
			// ?e?L?X?g+?O???t?B?b?N1??
			if (tx < gr) {
				// ?e?L?X?g?O??(type=3)
				render.mixptr[0] = render.textout;
				render.mixshift[0] = 10;
				render.mixx[0] = &render.textx;
				render.mixy[0] = &render.texty;
				render.mixand[0] = 1024 - 1;
				render.mixptr[1] = ptr[0];
				render.mixshift[1] = shift[0];
				render.mixx[1] = &render.grpx[ map[0] ];
				render.mixy[1] = &render.grpy[ map[0] ];
				render.mixand[1] = an[0];
				render.mixtype = 3;
				return;
			}
			// ?O???t?B?b?N?O??(type=3,tx=gr??O???t?B?b?N?O??A???II)
			render.mixptr[1] = render.textout;
			render.mixshift[1] = 10;
			render.mixx[1] = &render.textx;
			render.mixy[1] = &render.texty;
			render.mixand[1] = 1024 - 1;
			render.mixptr[0] = ptr[0];
			render.mixshift[0] = shift[0];
			render.mixx[0] = &render.grpx[ map[0] ];
			render.mixy[0] = &render.grpy[ map[0] ];
			render.mixand[0] = an[0];
			render.mixtype = 3;
			return;
		}
		// ?e?L?X?g+?O???t?B?b?N2????(type=5, type=6)
		ASSERT((render.mixpage >= 2) && (render.mixpage <= 6));
		render.mixptr[0] = render.textout;
		render.mixshift[0] = 10;
		render.mixx[0] = &render.textx;
		render.mixy[0] = &render.texty;
		render.mixand[0] = 1024 - 1;
		for (i=0; i<render.mixpage; i++) {
			render.mixptr[i + 4] = ptr[i];
			render.mixshift[i + 4] = shift[i];
			render.mixx[i + 4] = &render.grpx[ map[i] ];
			render.mixy[i + 4] = &render.grpy[ map[i] ];
			render.mixand[i + 4] = an[i];
		}
		if (tx < gr) {
			render.mixtype = 5;
		}
		else {
			render.mixtype = 6;
		}
		return;
	}

	// ?X?v???C?g????
	if (!render.texten) {
		// ?e?L?X?g???
		if (render.mixpage == 0) {
			// ?O???t?B?b?N????A?X?v???C?g???(type=1)
			render.mixptr[0] = render.bgspbuf;
			render.mixshift[0] = 9;
			render.mixx[0] = &render.zero;
			render.mixy[0] = &render.zero;
			render.mixand[0] = 512 - 1;
			render.mixtype = 1;
			return;
		}
		if (render.mixpage == 1) {
			// ?X?v???C?g+?O???t?B?b?N1??(type=3)
			if (sp < gr) {
				// ?X?v???C?g?O??
				render.mixptr[0] = render.bgspbuf;
				render.mixshift[0] = 9;
				render.mixx[0] = &render.zero;
				render.mixy[0] = &render.zero;
				render.mixand[0] = 512 - 1;
				render.mixptr[1] = ptr[0];
				render.mixshift[1] = shift[0];
				render.mixx[1] = &render.grpx[ map[0] ];
				render.mixy[1] = &render.grpy[ map[0] ];
				render.mixand[1] = an[0];
				render.mixtype = 3;
				return;
			}
			// ?O???t?B?b?N?O??(sp=gr??s??)
			render.mixptr[1] = render.bgspbuf;
			render.mixshift[1] = 9;
			render.mixx[1] = &render.zero;
			render.mixy[1] = &render.zero;
			render.mixand[1] = 512 - 1;
			render.mixptr[0] = ptr[0];
			render.mixshift[0] = shift[0];
			render.mixx[0] = &render.grpx[ map[0] ];
			render.mixy[0] = &render.grpy[ map[0] ];
			render.mixand[0] = an[0];
			render.mixtype = 3;
			return;
		}
		// ?X?v???C?g+?O???t?B?b?N2????(type=5, type=6)
		ASSERT((render.mixpage >= 2) && (render.mixpage <= 4));
		render.mixptr[0] = render.bgspbuf;
		render.mixshift[0] = 9;
		render.mixx[0] = &render.zero;
		render.mixy[0] = &render.zero;
		render.mixand[0] = 512 - 1;
		for (i=0; i<render.mixpage; i++) {
			render.mixptr[i + 4] = ptr[i];
			render.mixshift[i + 4] = shift[i];
			render.mixx[i + 4] = &render.grpx[ map[i] ];
			render.mixy[i + 4] = &render.grpy[ map[i] ];
			render.mixand[i + 4] = an[i];
		}
		if (sp < gr) {
			render.mixtype = 5;
		}
		else {
			render.mixtype = 6;
		}
		return;
	}

	// ?e?L?X?g????
	if (render.mixpage == 0) {
		// ?O???t?B?b?N????B?e?L?X?g?{?X?v???C?g(type=3)
		if (tx <= sp) {
			// tx=sp??e?L?X?g?O??(LMZ2)
			render.mixptr[0] = render.textout;
			render.mixshift[0] = 10;
			render.mixx[0] = &render.textx;
			render.mixy[0] = &render.texty;
			render.mixand[0] = 1024 - 1;
			render.mixptr[1] = render.bgspbuf;
			render.mixshift[1] = 9;
			render.mixx[1] = &render.zero;
			render.mixy[1] = &render.zero;
			render.mixand[1] = 512 - 1;
			render.mixtype = 3;
			return;
		}
		// ?X?v???C?g?O??
		render.mixptr[1] = render.textout;
		render.mixshift[1] = 10;
		render.mixx[1] = &render.textx;
		render.mixy[1] = &render.texty;
		render.mixand[1] = 1024 - 1;
		render.mixptr[0] = render.bgspbuf;
		render.mixshift[0] = 9;
		render.mixx[0] = &render.zero;
		render.mixy[0] = &render.zero;
		render.mixand[0] = 512 - 1;
		render.mixtype = 3;
		return;
	}

	// ?D???????
	if (tx == 3) tx--;
	if (sp == 3) sp--;
	if (gr == 3) gr--;
	if (tx == sp) {
		// ?K???????????
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
		// ?K???????????
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
		// ?K???????????
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
	ASSERT((tx != gr) && (gr != sp) && (tx != sp));
	ASSERT((tx >= 0) && (tx < 3));
	ASSERT((sp >= 0) && (sp < 3));
	ASSERT((gr >= 0) && (gr < 3));
	render.mixmap[tx] = 0;
	render.mixmap[sp] = 1;
	render.mixmap[gr] = 2;

	if (render.mixpage == 1) {
		// ?e?L?X?g?{?X?v???C?g?{?O???t?B?b?N1??(type=7)
		render.mixptr[0] = render.textout;
		render.mixshift[0] = 10;
		render.mixx[0] = &render.textx;
		render.mixy[0] = &render.texty;
		render.mixand[0] = 1024 - 1;
		render.mixptr[1] = render.bgspbuf;
		render.mixshift[1] = 9;
		render.mixx[1] = &render.zero;
		render.mixy[1] = &render.zero;
		render.mixand[1] = 512 - 1;
		render.mixptr[2] = ptr[0];
		render.mixshift[2] = shift[0];
		render.mixx[2] = &render.grpx[ map[0] ];
		render.mixy[2] = &render.grpy[ map[0] ];
		render.mixand[2] = an[0];
		render.mixtype = 7;
		return;
	}

	// ?e?L?X?g?{?X?v???C?g?{?O???t?B?b?N?Q????(type=8)
	render.mixptr[0] = render.textout;
	render.mixshift[0] = 10;
	render.mixx[0] = &render.textx;
	render.mixy[0] = &render.texty;
	render.mixand[0] = 1024 - 1;
	render.mixptr[1] = render.bgspbuf;
	render.mixshift[1] = 9;
	render.mixx[1] = &render.zero;
	render.mixy[1] = &render.zero;
	render.mixand[1] = 512 - 1;
	for (i=0; i<render.mixpage; i++) {
		render.mixptr[i + 4] = ptr[i];
		render.mixshift[i + 4] = shift[i];
		render.mixx[i + 4] = &render.grpx[ map[i] ];
		render.mixy[i + 4] = &render.grpy[ map[i] ];
		render.mixand[i + 4] = an[i];
	}
	render.mixtype = 8;
}

//---------------------------------------------------------------------------
//
//	?R???g???X?g???
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetContrast(int cont)
{
	// ?V?X?e???|?[?g????_???v?`?F?b?N??s?????A?????????????
	ASSERT(this);
	ASSERT((cont >= 0) && (cont <= 15));

	// ??X??t???OON
	render.contlevel = cont;
	render.contrast = TRUE;
}

//---------------------------------------------------------------------------
//
//	?R???g???X?g?�E��E�
//
//---------------------------------------------------------------------------
int FASTCALL Render::GetContrast() const
{
	ASSERT(this);
	ASSERT((render.contlevel >= 0) && (render.contlevel <= 15));

	return render.contlevel;
}

//---------------------------------------------------------------------------
//
//	?R???g???X?g????
//
//---------------------------------------------------------------------------
void FASTCALL Render::Contrast()
{
	int i;

	// ?|?C???g??u???X?A?t???ODown
	render.palptr = render.palbuf;
	render.palptr += (render.contlevel << 16);
	render.contrast = FALSE;

	// ?p???b?g?t???O??S??Up
	for (i=0; i<0x200; i++) {
		render.palmod[i] = TRUE;
	}
	render.palette = TRUE;
}

//---------------------------------------------------------------------------
//
//	?p???b?g??
//
//---------------------------------------------------------------------------
void FASTCALL Render::MakePalette()
{
	DWORD *p;
	int ratio;
	int i;
	int j;

	ASSERT(palbuf_original);
	ASSERT(palbuf_fast);

	// --- Original palette table ---
	p = palbuf_original;
	for (i=0; i<16; i++) {
		ratio = 256 - ((15 - i) << 4);
		for (j=0; j<0x10000; j++) {
			*p++ = ConvPalette(j, ratio);
		}
	}

	p = palbuf_fast;
	for (i=0; i<16; i++) {
		ratio = 256 - ((15 - i) << 4);
		for (j=0; j<0x10000; j++) {
			*p++ = ConvPalette(j, ratio);
		}
	}
}

//---------------------------------------------------------------------------
//
//	?p???b?g???
//
//---------------------------------------------------------------------------
DWORD FASTCALL Render::ConvPalette(int color, int ratio)
{
	DWORD r;
	DWORD g;
	DWORD b;

	// assert
	ASSERT((color >= 0) && (color < 0x10000));
	ASSERT((ratio >= 0) && (ratio <= 0x100));

	// ?S??R?s?[
	r = (DWORD)color;
	g = (DWORD)color;
	b = (DWORD)color;

	// MSB????G:5?AR:5?AB:5?AI:1????????????
	// ????? R:8 G:8 B:8??DWORD?????Bb31-b24??g????
	r <<= 13;
	r &= 0xf80000;
	g &= 0x00f800;
	b <<= 2;
	b &= 0x0000f8;

	// ?P?x?r?b?g???Up(???f?[?^??0?????A!=0???????????)
	if (color & 1) {
		r |= 0x070000;
		g |= 0x000700;
		b |= 0x000007;
	}

	// ?R???g???X?g??e????????
	b *= ratio;
	b >>= 8;
	g *= ratio;
	g >>= 8;
	g &= 0xff00;
	r *= ratio;
	r >>= 8;
	r &= 0xff0000;

	DWORD out = (DWORD)(r | g | b);
	if (color & 1) {
		out |= REND_PX68K_IBIT;
	}
	return out;
}

//---------------------------------------------------------------------------
//
//	?p???b?g?�E��E�
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetPalette() const
{
	ASSERT(this);
	ASSERT(render.paldata);

	return render.paldata;
}

//---------------------------------------------------------------------------
//
//	?p???b?g????
//
//---------------------------------------------------------------------------
void FASTCALL Render::Palette()
{
	DWORD data;
	BOOL tx;
	BOOL gr;
	BOOL sp;
	int i;
	int j;

	// ?t???OOFF
	tx = FALSE;
	gr = FALSE;
	sp = FALSE;

	// ?O???t?B?b?N
	for (i=0; i<0x100; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// ?O???t?B?b?N??e???A?t???OOFF
			gr = TRUE;
			render.palmod[i] = FALSE;

			// ?????F?????
			if (i == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// 65536?F??????p???b?g?f?[?^???
			j = i >> 1;
			if (i & 1) {
				j += 128;
			}
			render.pal64k[j * 2 + 0] = (BYTE)(data >> 8);
			render.pal64k[j * 2 + 1] = (BYTE)data;
		}
	}

	// ?e?L?X?g???X?v???C?g
	for (i=0x100; i<0x110; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// ?e?L?X?g??e???A?t???OOFF
			tx = TRUE;
			render.palmod[i] = FALSE;

			// ?????F?????
			if (i == 0x100) {
				render.paldata[i] |= REND_COLOR0;
				// 0x100??BG?E?X?v???C?g???K???e??
				sp = TRUE;
			}

			// PCG????
			memset(&render.pcgready[0], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[0] > 0) {
				sp = TRUE;
			}
		}
	}

	// ?X?v???C?g
	for (i=0x110; i<0x200; i++) {
		if (render.palmod[i]) {
			// ?X?v???C?g??e???A?t???OOFF
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];
			render.palmod[i] = FALSE;

			// ?????F?????
			if ((i & 0x00f) == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// PCG????
			memset(&render.pcgready[(i & 0xf0) << 4], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[(i & 0xf0) >> 4] > 0) {
				sp = TRUE;
			}
		}
	}

	// ?O???t?B?b?N?t???O
	if (gr) {
		// ?t???OON
		for (i=0; i<512*4; i++) {
			render.grppal[i] = TRUE;
		}
	}

	// ?e?L?X?g?t???O
	if (tx) {
		for (i=0; i<1024; i++) {
			render.textpal[i] = TRUE;
		}
	}

	// ?X?v???C?g?t???O
	if (sp) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}

	// ?p???b?g?t???OOFF
	render.palette = FALSE;
}

void FASTCALL Render::TextScrl(DWORD x, DWORD y)
{
	int i;

	ASSERT(this);
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// ??r?`?F?b?N
	if ((render.textx == x) && (render.texty == y)) {
		return;
	}

	// ???[?N?X?V
	render.textx = x;
	render.texty = y;

	// ?t???OON
	if (render.texten) {
#if defined(REND_LOG)
		LOG2(Log::Normal, "?e?L?X?g?X?N???[?? x=%d y=%d", x, y);
#endif	// REND_LOG

		for (i=0; i<1024; i++) {
			render.mix[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	?e?L?X?g?R?s?[
//
//---------------------------------------------------------------------------
void FASTCALL Render::TextCopy(DWORD src, DWORD dst, DWORD plane)
{
	ASSERT(this);
	ASSERT((src >= 0) && (src < 256));
	ASSERT((dst >= 0) && (dst < 256));
	ASSERT(plane < 16);

	// ?A?Z???u???T?u
	RendTextCopy(&render.texttv[src << 9],
				 &render.texttv[dst << 9],
				 plane,
				 &render.textflag[dst << 7],
				 &render.textmod[dst << 2]);
}

//---------------------------------------------------------------------------
//
//	?e?L?X?g?o?b?t?@?�E��E�
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetTextBuf() const
{
	ASSERT(this);
	ASSERT(render.textout);

	return render.textout;
}

//---------------------------------------------------------------------------
//
//	?e?L?X?g????
//
//---------------------------------------------------------------------------
void FASTCALL Render::Text(int raster)
{
	int y;

	// assert
	ASSERT((raster >= 0) && (raster < 1024));
	ASSERT(render.texttv);
	ASSERT(render.textflag);
	ASSERT(render.textbuf);
	ASSERT(render.palbuf);

	// ?f?B?Z?[?u????�E��E�??????
	if (!render.texten) {
		return;
	}

	// ?????Y?Z?o
	y = (raster + render.texty) & 0x3ff;

	// ??X?t???O(?????^)
	if (render.textmod[y]) {
		// ?t???O????
		render.textmod[y] = FALSE;
		render.mix[raster] = TRUE;

		// ???????????
		RendTextMem(render.texttv + (y << 7),
					render.textflag + (y << 5),
					render.textbuf + (y << 9));

		// ?????p???b?g???
		RendTextPal(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.textflag + (y << 5),
					render.paldata + 0x100);
	}

	// ?p???b?g(???^)
	if (render.textpal[y]) {
		// ?t???O????
		render.textpal[y] = FALSE;

		// ?????p???b?g???
		RendTextAll(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.paldata + 0x100);
		render.mix[raster] = TRUE;

		// y == 1023???R?s?[????
		if (y == 1023) {
			memcpy(render.textout + (1024 << 10), render.textout + (1023 << 10), sizeof(DWORD) * 1024);
		}
	}
}

//---------------------------------------------------------------------------
//
//	?O???t?B?b?N?o?b?t?@?�E��E�
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetGrpBuf(int index) const
{
	ASSERT(this);
	ASSERT((index >= 0) && (index <= 3));

	ASSERT(render.grpbuf[index]);
	return render.grpbuf[index];
}

//---------------------------------------------------------------------------
//
//	?O???t?B?b?N?X?N???[??
//
//---------------------------------------------------------------------------
void FASTCALL Render::GrpScrl(int block, DWORD x, DWORD y)
{
	BOOL flag;
	int i;

	ASSERT(this);
	ASSERT((block >= 0) && (block <= 3));
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// ??r?`?F?b?N?B??\?????X?V???
	flag = FALSE;
	if ((render.grpx[block] != x) || (render.grpy[block] != y)) {
		render.grpx[block] = x;
		render.grpy[block] = y;
		flag = render.grpen[block];
	}

	// ?t???O????
	if (!flag) {
		return;
	}

#if defined(REND_LOG)
	LOG3(Log::Normal, "?O???t?B?b?N?X?N???[?? block=%d x=%d y=%d", block, x, y);
#endif	// REND_LOG

	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	?O???t?B?b?N????
//
//---------------------------------------------------------------------------
void FASTCALL Render::Grp(int block, int raster)
{
	int i;
	int y;
	int offset;

	ASSERT((block >= 0) && (block <= 3));
	ASSERT((raster >= 0) && (raster < 1024));
	ASSERT(render.grpbuf[block]);
	ASSERT(render.grpgv);

	if (render.grptype == 0) {
		// 1024???[?h??y?[?W0????
		if (!render.grpen[0]) {
			return;
		}
	}
	else {
		// ?????O
		if (!render.grpen[block]) {
			return;
		}
	}

	// ?^?C?v??
	switch (render.grptype) {
		// ?^?C?v0:1024?~1024 16Color
		case 0:
			// ?I?t?Z?b?g?Z?o
			offset = (raster + render.grpy[0]) & 0x3ff;
			y = offset & 0x1ff;

			// ?\?????`?F?b?N
			if ((offset < 512) && (block >= 2)) {
				return;
			}
			if ((offset >= 512) && (block < 2)) {
				return;
			}

			// ?p???b?g?????S?????
			if (render.grppal[y + (block << 9)]) {
				render.grppal[y + (block << 9)] = FALSE;
				render.grpmod[y + (block << 9)] = FALSE;
				for (i=0; i<32; i++) {
					render.grpflag[(y << 5) + (block << 14) + i] = FALSE;
				}
				switch (block) {
					// ??????u???b?N0???E
					case 0:
						if (Rend1024A(render.grpgv + (y << 10),
									render.grpbuf[0] + (offset << 11),
									render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
					case 1:
						break;
					// ????????u???b?N2???E
					case 2:
						if (Rend1024B(render.grpgv + (y << 10),
									render.grpbuf[0] + (offset << 11),
									render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
					case 3:
						break;
				}
				return;
			}

			// ?????O??grpmod????????
			if (!render.grpmod[y + (block << 9)]) {
				return;
			}
			render.grpmod[y + (block << 9)] = FALSE;
			render.mix[raster] = TRUE;
			switch (block) {
				// ?u???b?N0-????
				case 0:
					Rend1024C(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5),
								render.paldata);
					break;
				// ?u???b?N1-?E??
				case 1:
					Rend1024D(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0x4000,
								render.paldata);
					break;
				// ?u???b?N2-????
				case 2:
					Rend1024E(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0x8000,
								render.paldata);
					break;
				// ?u???b?N3-?E??
				case 3:
					Rend1024F(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0xc000,
								render.paldata);
					break;
			}
			return;

		// ?^?C?v1:512?~512 16Color
		case 1:
			switch (block) {
				// ?y?[?W0
				case 0:
					y = (raster + render.grpy[0]) & 0x1ff;
					// ?p???b?g
					if (render.grppal[y]) {
						render.grppal[y] = FALSE;
						render.grpmod[y] = FALSE;
						for (i=0; i<32; i++) {
							render.grpflag[(y << 5) + i] = FALSE;
						}
						if (Rend16A(render.grpgv + (y << 10),
										render.grpbuf[0] + (y << 10),
										render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						return;
					}
					// ???
					if (render.grpmod[y]) {
						render.grpmod[y] = FALSE;
						render.mix[raster] = TRUE;
						Rend16B(render.grpgv + (y << 10),
								render.grpbuf[0] + (y << 10),
								render.grpflag + (y << 5),
								render.paldata);
					}
					return;
				// ?y?[?W1
				case 1:
					y = (raster + render.grpy[1]) & 0x1ff;
					// ?p???b?g
					if (render.grppal[y + 512]) {
						render.grppal[y + 512] = FALSE;
						render.grpmod[y + 512] = FALSE;
						for (i=0; i<32; i++) {
							render.grpflag[(y << 5) + i + 0x4000] = FALSE;
						}
						if (Rend16C(render.grpgv + (y << 10),
										render.grpbuf[1] + (y << 10),
										render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						return;
					}
					// ???
					if (render.grpmod[y + 512]) {
						render.grpmod[y + 512] = FALSE;
						render.mix[raster] = TRUE;
						Rend16D(render.grpgv + (y << 10),
								render.grpbuf[1] + (y << 10),
								render.grpflag + (y << 5) + 0x4000,
								render.paldata);
					}
					return;
				// ?y?[?W2
				case 2:
					y = (raster + render.grpy[2]) & 0x1ff;
					// ?p???b?g
					if (render.grppal[y + 1024]) {
						render.grppal[y + 1024] = FALSE;
						render.grpmod[y + 1024] = FALSE;
						for (i=0; i<32; i++) {
							render.grpflag[(y << 5) + i + 0x8000] = FALSE;
						}
						if (Rend16E(render.grpgv + (y << 10),
										render.grpbuf[2] + (y << 10),
										render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						return;
					}
					// ???
					if (render.grpmod[y + 1024]) {
						render.grpmod[y + 1024] = FALSE;
						render.mix[raster] = TRUE;
						Rend16F(render.grpgv + (y << 10),
								render.grpbuf[2] + (y << 10),
								render.grpflag + (y << 5) + 0x8000,
								render.paldata);
					}
					return;
				// ?y?[?W3
				case 3:
					y = (raster + render.grpy[3]) & 0x1ff;
					// ?p???b?g
					if (render.grppal[y + 1536]) {
						render.grppal[y + 1536] = FALSE;
						render.grpmod[y + 1536] = FALSE;
						for (i=0; i<32; i++) {
							render.grpflag[(y << 5) + i + 0xC000] = FALSE;
						}
						if (Rend16G(render.grpgv + (y << 10),
										render.grpbuf[3] + (y << 10),
										render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						return;
					}
					// ???
					if (render.grpmod[y + 1536]) {
						render.grpmod[y + 1536] = FALSE;
						render.mix[raster] = TRUE;
						Rend16H(render.grpgv + (y << 10),
								render.grpbuf[3] + (y << 10),
								render.grpflag + (y << 5) + 0xC000,
								render.paldata);
					}
					return;
			}
			return;

		// ?^?C?v2:512?~512 256Color
		case 2:
			ASSERT((block == 0) || (block == 2));
			if (block == 0) {
				// ?I?t?Z?b?g?Z?o
				y = (raster + render.grpy[0]) & 0x1ff;

				// ?p???b?g?????S?????
				if (render.grppal[y]) {
					render.grppal[y] = FALSE;
					render.grpmod[y] = FALSE;
					for (i=0; i<32; i++) {
						render.grpflag[(y << 5) + i] = FALSE;
					}
					if (Rend256C(render.grpgv + (y << 10),
									render.grpbuf[0] + (y << 10),
									render.paldata) != 0) {
						render.mix[raster] = TRUE;
					}
					return;
				}

				// ?????O??grpmod????????
				if (!render.grpmod[y]) {
					return;
				}

				render.grpmod[y] = FALSE;
				render.mix[raster] = TRUE;
				Rend256A(render.grpgv + (y << 10),
							render.grpbuf[0] + (y << 10),
							render.grpflag + (y << 5),
							render.paldata);
			}
			else {
				// ?I?t?Z?b?g?Z?o
				y = (raster + render.grpy[2]) & 0x1ff;

				// ?p???b?g?????S?????
				if (render.grppal[0x400 + y]) {
					render.grppal[0x400 + y] = FALSE;
					render.grpmod[0x400 + y] = FALSE;
					for (i=0; i<32; i++) {
						render.grpflag[(y << 5) + i + 0x8000] = FALSE;
					}
					if (Rend256D(render.grpgv + (y << 10),
									render.grpbuf[2] + (y << 10),
									render.paldata) != 0) {
						render.mix[raster] = TRUE;
					}
					return;
				}

				// ?????O??grpmod????????
				if (!render.grpmod[0x400 + y]) {
					return;
				}

				render.grpmod[0x400 + y] = FALSE;
				render.mix[raster] = TRUE;

				// ?????_?????O
				Rend256B(render.grpgv + (y << 10),
							render.grpbuf[2] + (y << 10),
							render.grpflag + 0x8000 + (y << 5),
							render.paldata);
			}
			return;

		// ?^?C?v3:512x512 ????`
		case 3:
		// ?^?C?v4:512x512 65536Color

		case 4:

			ASSERT(block == 0);
			// ?I?t?Z?b?g?Z?o
			y = (raster + render.grpy[0]) & 0x1ff;

			// ?p???b?g?????S?????
			if (render.grppal[y]) {
				render.grppal[y] = FALSE;
				render.grpmod[y] = FALSE;
				for (i=0; i<32; i++) {
					render.grpflag[(y << 5) + i] = FALSE;
				}
				if (Rend64KB(render.grpgv + (y << 10),
								render.grpbuf[0] + (y << 10),
								render.pal64k,
								render.palptr) != 0) {
					render.mix[raster] = TRUE;
				}
				return;
			}

			// ?????O??grpmod????????
			if (!render.grpmod[y]) {
				return;
			}
			render.grpmod[y] = FALSE;
			render.mix[raster] = TRUE;
			Rend64KA(render.grpgv + (y << 10),
						render.grpbuf[0] + (y << 10),
						render.grpflag + (y << 5),
						render.pal64k,
						render.palptr);
			return;
	}
}

//===========================================================================
//
//	?????_??(BG?E?X?v???C?g??)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	?X?v???C?g???W?X?^???Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::SpriteReset()
{
	DWORD addr;
	WORD data;

	// ?X?v???C?g???W?X?^???
	for (addr=0; addr<0x400; addr+=2) {
		data = *(WORD*)(&render.sprmem[addr]);
		SpriteReg(addr, data);
	}
}

//---------------------------------------------------------------------------
//
//	?X?v???C?g???W?X?^??X
//
//---------------------------------------------------------------------------
void FASTCALL Render::SpriteReg(DWORD addr, DWORD data)
{
	BOOL use;
	DWORD reg[4];
	DWORD *next;
	DWORD **ptr;
	int index;
	int i;
	int j;
	int offset;
	DWORD pcgno;

	ASSERT(this);
	ASSERT(addr < 0x400);
	ASSERT((addr & 1) == 0);

	// ?C???f?N?V???O??f?[?^????
	index = (int)(addr >> 3);
	switch ((addr & 7) >> 1) {
		// X,Y(0?`1023)
		case 0:
		case 1:
			data &= 0x3ff;
			break;
		// V,H,PAL,PCG
		case 2:
			data &= 0xcfff;
			break;
		// PRW(0,1,2,3)
		case 3:
			data &= 0x0003;
			break;
	}

	// ptr???(&spptr[index << 9])
	ptr = &render.spptr[index << 9];

	// ???W?X?^??o?b?N?A?b?v
	next = &render.spreg[index << 2];
	reg[0] = next[0];
	reg[1] = next[1];
	reg[2] = next[2];
	reg[3] = next[3];

	// ???W?X?^?????????
	render.spreg[addr >> 1] = data;

	// ????L???????`?F?b?N
	use = TRUE;
	if (next[0] == 0) {
		use = FALSE;
	}
	if (next[0] >= (DWORD)(render.mixlen + 16)) {
		use = FALSE;
	}
	if (next[1] == 0) {
		use = FALSE;
	}
	if (next[1] >= (512 + 16)) {
		use = FALSE;
	}
	if (next[3] == 0) {
		use = FALSE;
	}

	// ???????????A????????????�E��E�??????
	if (!render.spuse[index]) {
		if (!use) {
			return;
		}
	}

	// ??????L??????A??x????
	if (render.spuse[index]) {
		// ????????(PCG)
		pcgno = reg[2] & 0xfff;
		ASSERT(render.pcguse[ pcgno ] > 0);
		render.pcguse[ pcgno ]--;
		pcgno >>= 8;
		ASSERT(render.pcgpal[ pcgno ] > 0);
		render.pcgpal[ pcgno ]--;

		// ????????(?|?C???^)
		for (i=0; i<16; i++) {
			j = (int)(reg[1] - 16 + i);
			if ((j >= 0) && (j < 512)) {
				ptr[j] = NULL;
				render.bgspmod[j] = TRUE;
			}
		}

		// ?????????A??????I??
		if (!use) {
			render.spuse[index] = FALSE;
			return;
		}
	}

	// ?o?^????(?g?p?t???O)
	render.spuse[index] = TRUE;

	// ?o?^????(PCG)
	pcgno = next[2] & 0xfff;
	render.pcguse[ pcgno ]++;
	offset = pcgno << 8;
	pcgno >>= 8;
	render.pcgpal[ pcgno ]++;

	// PCG?A?h???X??v?Z?A?|?C???^?Z?b?g
	if (next[2] & 0x8000) {
		// V???]
		offset += 0xf0;
		for (i=0; i<16; i++) {
			j = (int)(next[1] - 16 + i);
			if ((j >= 0) && (j < 512)) {
				ptr[j] = &render.pcgbuf[offset];
				render.bgspmod[j] = TRUE;
			}
			offset -= 16;
		}
	}
	else {
		// ?m?[?}??
		for (i=0; i<16; i++) {
			j = (int)(next[1] - 16 + i);
			if ((j >= 0) && (j < 512)) {
				ptr[j] = &render.pcgbuf[offset];
				render.bgspmod[j] = TRUE;
			}
			offset += 16;
		}
	}
}

//---------------------------------------------------------------------------
//
//	BG?X?N???[????X
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGScrl(int page, DWORD x, DWORD y)
{
	BOOL flag;
	int i;

	ASSERT((page == 0) || (page == 1));
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// ??r?A??v?????????????
	if ((render.bgx[page] == x) && (render.bgy[page] == y)) {
		return;
	}

	// ?X?V
	render.bgx[page] = x;
	render.bgy[page] = y;

	// 768?~512??�E�p???
	if (!render.bgspflag) {
		return;
	}

	// ?\???????ABGSPMOD??�E�O??
	flag = FALSE;
	if (render.bgdisp[0]) {
		flag = TRUE;
	}
	if (render.bgdisp[1] && !render.bgsize) {
		flag = TRUE;
	}
	if (flag) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	BG?R???g???[????X
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGCtrl(int index, BOOL flag)
{
	int i;
	int j;
	BOOL areaflag[2];
	DWORD *reg;
	WORD *area;
	DWORD pcgno;
	DWORD low;
	DWORD mid;
	DWORD high;

	// ?t???OOFF
	areaflag[0] = FALSE;
	areaflag[1] = FALSE;

	// ?^?C?v??
	switch (index) {
		// BG0 ?\???t???O
		case 0:
			if (render.bgdisp[0] == flag) {
				return;
			}
			render.bgdisp[0] = flag;
			break;

		// BG1 ?\???t???O
		case 1:
			if (render.bgdisp[1] == flag) {
				return;
			}
			render.bgdisp[1] = flag;
			break;

		// BG0 ?G???A??X
		case 2:
			if (render.bgarea[0] == flag) {
				return;
			}
			render.bgarea[0] = flag;
			areaflag[0] = TRUE;
			break;

		// BG1 ?G???A??X
		case 3:
			if (render.bgarea[1] == flag) {
				return;
			}
			render.bgarea[1] = flag;
			areaflag[1] = TRUE;
			break;

		// BG?T?C?Y??X
		case 4:

			if (render.bgsize == flag) {
				return;
			}
			render.bgsize = flag;
			areaflag[0] = TRUE;
			areaflag[1] = TRUE;
			break;

		// ?????(???????)
		default:
			ASSERT(FALSE);
			return;
	}

	// ?t???O????
	for (i=0; i<2; i++) {
		if (areaflag[i]) {
			// ?????g???????render.pcguse??J?b?g
			reg = render.bgreg[i];
			for (j=0; j<(64 * 64); j++) {
				pcgno = reg[j];
				if (pcgno & 0x10000) {
					pcgno &= 0xfff;
					ASSERT(render.pcguse[ pcgno ] > 0);
					render.pcguse[ pcgno ]--;
					pcgno = (pcgno >> 8) & 0x0f;
					ASSERT(render.pcgpal[ pcgno ] > 0);
					render.pcgpal[ pcgno ]--;
				}
			}

			// ?f?[?^?A?h???X??Z?o($EBE000,$EBC000)
			area = (WORD*)render.sprmem;
			area += 0x6000;
			if (render.bgarea[i]) {
				area += 0x1000;
			}

			// 64?~64???[?h?R?s?[?B$10000??r?b?g????0
			if (render.bgsize) {
				// 16x16???????
				for (j=0; j<(64*64); j++) {
					render.bgreg[i][j] = (DWORD)area[j];
				}
			}
			else {
				// 8x8??H?v???K?v?BPCG(0-255)??>>2???A??????bit0,1??bit17,18??
				for (j=0; j<(64*64); j++) {
					low = (DWORD)area[j];
					mid = low;
					high = low;
					low >>= 2;
					low &= (64 - 1);
					mid &= 0xff00;
					high <<= 17;
					high &= 0x60000;
					render.bgreg[i][j] = (DWORD)(low | mid | high);
				}
			}

			// bgall??Z?b?g
			for (j=0; j<64; j++) {
				render.bgall[i][j] = TRUE;
			}
		}
	}

	// ????X???A768?~512??O???bgspmod??�E�O??
	if (render.bgspflag) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	BG????????X
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGMem(DWORD addr, WORD data)
{
	BOOL flag;
	int i;
	int j;
	int index;
	int raster;
	DWORD pcgno;
	DWORD low;
	DWORD mid;
	DWORD high;

	ASSERT((addr >= 0xc000) && (addr < 0x10000));

	// ?y?[?W???[?v
	for (i=0; i<2; i++) {
		// ?Y???y?[?W??f?[?^?G???A???v???????
		flag = FALSE;
		if ((render.bgarea[i] == FALSE) && (addr < 0xe000)) {
			flag = TRUE;
		}
		if (render.bgarea[i] && (addr >= 0xe000)) {
			flag = TRUE;
		}
		if (!flag) {
			continue;
		}

		// ?C???f?b?N?X(<64x64)?A???W?X?^?|?C???^?�E��E�
		index = (int)(addr & 0x1fff);
		index >>= 1;
		ASSERT((index >= 0) && (index < 64*64));
		pcgno = render.bgreg[i][index];

		// ??O??pcguse?????
		if (pcgno & 0x10000) {
			pcgno &= 0xfff;
			ASSERT(render.pcguse[ pcgno ] > 0);
			render.pcguse[ pcgno ]--;
			pcgno = (pcgno >> 8) & 0x0f;
			ASSERT(render.pcgpal[ pcgno ] > 0);
			render.pcgpal[ pcgno ]--;
		}

		// ?R?s?[
		if (render.bgsize) {
			// 16x16???????
			render.bgreg[i][index] = (DWORD)data;
		}
		else {
			// 8x8??H?v???K?v?BPCG(0-255)??>>2???A??????bit0,1??bit17,18??
			low = (DWORD)data;
			mid = low;
			high = low;
			low >>= 2;
			low &= (64 - 1);
			mid &= 0xff00;
			high <<= 17;
			high &= 0x60000;
			render.bgreg[i][index] = (DWORD)(low | mid | high);
		}

		// bgall??�E�O??
		render.bgall[i][index >> 6] = TRUE;

		// ?\???????????I???Bbgsize=1??y?[?W1?????I??
		if (!render.bgspflag || !render.bgdisp[i]) {
			continue;
		}
		if (render.bgsize && (i == 1)) {
			continue;
		}

		// ?X?N???[????u????v?Z???Abgspmod??�E�O??
		index >>= 6;
		if (render.bgsize) {
			// 16x16
			raster = render.bgy[i] + (index << 4);
			for (j=0; j<16; j++) {
				raster &= (1024 - 1);
				if ((raster >= 0) && (raster < 512)) {
					render.bgspmod[raster] = TRUE;
				}
				raster++;
			}
		}
		else {
			// 8x8
			raster = render.bgy[i] + (index << 3);
			for (j=0; j<16; j++) {
				raster &= (512 - 1);
				render.bgspmod[raster] = TRUE;
				raster++;
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//	PCG????????X
//
//---------------------------------------------------------------------------
void FASTCALL Render::PCGMem(DWORD addr)
{
	int index;
	DWORD count;
	int i;

	ASSERT(this);
	ASSERT(addr >= 0x8000);
	ASSERT(addr < 0x10000);
	ASSERT((addr & 1) == 0);

	// ?C???f?b?N?X??o??
	addr &= 0x7fff;
	index = (int)(addr >> 7);
	ASSERT((index >= 0) && (index < 256));

	// render.pcgready?????
	for (i=0; i<16; i++) {
		render.pcgready[index + (i << 8)] = FALSE;
	}

	// render.pcguse??>0???
	count = 0;
	for (i=0; i<16; i++) {
		count += render.pcguse[index + (i << 8)];
	}
	if (count > 0) {
		// ?d????????ABG/?X?v???C?g??????????
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	PCG?o?b?t?@?�E��E�
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetPCGBuf() const
{
	ASSERT(this);
	ASSERT(render.pcgbuf);

	return render.pcgbuf;
}

//---------------------------------------------------------------------------
//
//	BG/?X?v???C?g?o?b?t?@?�E��E�
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetBGSpBuf() const
{
	ASSERT(this);
	ASSERT(render.bgspbuf);

	return render.bgspbuf;
}

//---------------------------------------------------------------------------
//
//	BG/?X?v???C?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGSprite(int raster)
{
	int i;
	DWORD *reg;
	DWORD **ptr;
	DWORD *buf;
	DWORD pcgno;
	DWORD stamp;
	const int bg_hadjust = CalcBGHAdjustPixels(compositor_mode, crtc, sprite);
	const BOOL sprite_visible = sprite->IsDisplay();

	if (raster >= 512) return;
	if (render.mixlen > 512) return;

	stamp = render.fast_bg_stamp[raster];
	if (!render.bgspmod[raster] && (render.fast_bg_done[raster] == stamp)) {
		return;
	}
	render.bgspmod[raster] = FALSE;
	render.mix[raster] = TRUE;

	buf = &render.bgspbuf[raster << 9];
	RendClrSprite(buf, render.paldata[0x100], render.mixlen);
	if (!sprite_visible) {
		RendClrSprite(buf, render.paldata[0x100] & 0x00ffffff, render.mixlen);
	}

	if (sprite_visible) {
		reg = &render.spreg[127 << 2];
		ptr = &render.spptr[127 << 9];
		ptr += raster;
		for (i=127; i>=0; i--) {
			if (render.spuse[i] && (reg[3] == 1) && *ptr) {
				pcgno = reg[2] & 0xfff;
				if (!render.pcgready[pcgno]) {
					ASSERT(render.pcguse[pcgno] > 0);
					render.pcgready[pcgno] = TRUE;
					RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
				}
				const DWORD sprite_x = (DWORD)(((int)reg[0] + bg_hadjust) & 0x03ff);
				if (cmov) {
					RendSpriteC(*ptr, buf, sprite_x, reg[2] & 0x4000);
				}
				else {
					RendSprite(*ptr, buf, sprite_x, reg[2] & 0x4000);
				}
			}
			reg -= 4;
			ptr -= 512;
		}
	}

	if (render.bgdisp[1] && !render.bgsize) {
		BG(1, raster, buf);
	}

	if (sprite_visible) {
		reg = &render.spreg[127 << 2];
		ptr = &render.spptr[127 << 9];
		ptr += raster;
		for (i=127; i>=0; i--) {
			if (render.spuse[i] && (reg[3] == 2) && *ptr) {
				pcgno = reg[2] & 0xfff;
				if (!render.pcgready[pcgno]) {
					ASSERT(render.pcguse[pcgno] > 0);
					render.pcgready[pcgno] = TRUE;
					RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
				}
				const DWORD sprite_x = (DWORD)(((int)reg[0] + bg_hadjust) & 0x03ff);
				if (cmov) {
					RendSpriteC(*ptr, buf, sprite_x, reg[2] & 0x4000);
				}
				else {
					RendSprite(*ptr, buf, sprite_x, reg[2] & 0x4000);
				}
			}
			reg -= 4;
			ptr -= 512;
		}
	}

	if (render.bgdisp[0]) {
		BG(0, raster, buf);
	}

	if (sprite_visible) {
		reg = &render.spreg[127 << 2];
		ptr = &render.spptr[127 << 9];
		ptr += raster;
		for (i=127; i>=0; i--) {
			if (render.spuse[i] && (reg[3] == 3) && *ptr) {
				pcgno = reg[2] & 0xfff;
				if (!render.pcgready[pcgno]) {
					ASSERT(render.pcguse[pcgno] > 0);
					render.pcgready[pcgno] = TRUE;
					RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
				}
				const DWORD sprite_x = (DWORD)(((int)reg[0] + bg_hadjust) & 0x03ff);
				if (cmov) {
					RendSpriteC(*ptr, buf, sprite_x, reg[2] & 0x4000);
				}
				else {
					RendSprite(*ptr, buf, sprite_x, reg[2] & 0x4000);
				}
			}
			reg -= 4;
			ptr -= 512;
		}
	}
	render.fast_bg_done[raster] = stamp;
}

void FASTCALL Render::BG(int page, int raster, DWORD *buf)
{
	int x;
	int y;
	DWORD **ptr;
	int len;
	int rest;
	const BOOL legacy_bg_transparency = !original_bg0_render_enabled;

	ASSERT((page == 0) || (page == 1));
	ASSERT((raster >= 0) && (raster < 512));
	ASSERT(buf);

	const int bg_hadjust = CalcBGHAdjustPixels(compositor_mode, crtc, sprite);

	// y?u???b?N?????o??
	y = render.bgy[page] + raster;
	if (render.bgsize) {
		// 16x16???[?h
		y &= (1024 - 1);
		y >>= 4;
	}
	else {
		// 8x8???[?h
		y &= (512 - 1);
		y >>= 3;
	}
	ASSERT((y >= 0) && (y < 64));

	// bgall??TRUE???A????y?u???b?N???X?f?[?^????
	if (render.bgall[page][y]) {
		render.bgall[page][y] = FALSE;
		BGBlock(page, y);
	}

	// ?\??
	ptr = render.bgptr[page];
	if (!render.bgsize) {
		// 8x8??\??
		x = (render.bgx[page] - bg_hadjust) & (512 - 1);
		ptr += (((render.bgy[page] + raster) & (512 - 1)) << 7);

		// ????????`?F?b?N
		if ((x & 7) == 0) {
			// 8x8?A???????
			x >>= 3;
			if (cmov) {
				RendBG8C(ptr, buf, x, render.mixlen, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
			}
			else {
				RendBG8(ptr, buf, x, render.mixlen, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
			}
			return;
		}

		// ???????[?u???b?N????s
		rest = 8 - (x & 7);
		ASSERT((rest > 0) && (rest < 8));
		RendBG8P(&ptr[(x & 0xfff8) >> 2], buf, (x & 7), rest, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);

		// ?]??????8dot?P????????
		len = render.mixlen - rest;
		x += rest;
		x &= (512 - 1);
		ASSERT((x & 7) == 0);
		if (cmov) {
			RendBG8C(ptr, &buf[rest], (x >> 3), (len & 0xfff8), render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
		}
		else {
			RendBG8(ptr, &buf[rest], (x >> 3), (len & 0xfff8), render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
		}

		// ???
		if (len & 7) {
			x += (len & 0xfff8);
			x &= (512 - 1);
			RendBG8P(&ptr[x >> 2], &buf[rest + (len & 0xfff8)], 0, (len & 7),
				render.pcgready, render.sprmem, render.pcgbuf, render.paldata,
				legacy_bg_transparency);
		}
		return;
	}

	// 16x16??\??
	x = (render.bgx[page] - bg_hadjust) & (1024 - 1);
	ptr += (((render.bgy[page] + raster) & (1024 - 1)) << 7);

	// ????????`?F?b?N
	if ((x & 15) == 0) {
		// 16x16?A???????
		x >>= 4;
		if (cmov) {
			RendBG16C(ptr, buf, x, render.mixlen, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
		}
		else {
			RendBG16(ptr, buf, x, render.mixlen, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
		}
		return;
	}

	// ???????[?u???b?N????s
	rest = 16 - (x & 15);
	ASSERT((rest > 0) && (rest < 16));
	RendBG16P(&ptr[(x & 0xfff0) >> 3], buf, (x & 15), rest, render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);

	// ?]??????16dot?P????????
	len = render.mixlen - rest;
	x += rest;
	x &= (1024 - 1);
	ASSERT((x & 15) == 0);
	if (cmov) {
		RendBG16C(ptr, &buf[rest], (x >> 4), (len & 0xfff0), render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
	}
	else {
		RendBG16(ptr, &buf[rest], (x >> 4), (len & 0xfff0), render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
	}

	// ???
	if (len & 15) {
		x += (len & 0xfff0);
		x &= (1024 - 1);
		x >>= 4;
		RendBG16P(&ptr[x << 1], &buf[rest + (len & 0xfff0)], 0, (len & 15),
			render.pcgready, render.sprmem, render.pcgbuf, render.paldata,
			legacy_bg_transparency);
	}
}

//---------------------------------------------------------------------------
//
//	BG(?u???b?N????)
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGBlock(int page, int y)
{
	int i;
	int j;
	DWORD *reg;
	DWORD **ptr;
	DWORD *pcgbuf;
	DWORD bgdata;
	DWORD pcgno;

	ASSERT((page == 0) || (page == 1));
	ASSERT((y >= 0) && (y < 64));

	// ???W?X?^?|?C???^????
	reg = &render.bgreg[page][y << 6];

	// BG?|?C???^????
	ptr = render.bgptr[page];
	if (render.bgsize) {
		ptr += (y << 11);
	}
	else {
		ptr += (y << 10);
	}

	// ???[?v
	for (i=0; i<64; i++) {
		// ?�E��E�
		bgdata = reg[i];

		// $10000????????????OK
		if (bgdata & 0x10000) {
			ptr += 2;
			continue;
		}

		// $10000??OR
		reg[i] |= 0x10000;

		// pcgno????
		pcgno = bgdata & 0xfff;

		// ?T?C?Y??
		if (render.bgsize) {
			// 16x16
			pcgbuf = &render.pcgbuf[ (pcgno << 8) ];
			if (bgdata & 0x8000) {
				// ?????]
				pcgbuf += 0xf0;
				for (j=0; j<16; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = reinterpret_cast<DWORD*>(static_cast<size_t>(bgdata));
					pcgbuf -= 0x10;
					ptr += 128;
				}
			}
			else {
				// ???
				for (j=0; j<16; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = reinterpret_cast<DWORD*>(static_cast<size_t>(bgdata));
					pcgbuf += 0x10;
					ptr += 128;
				}
			}
			ptr -= 2048;
		}
		else {
			// 8x8?Bbit17,bit18??l??????
			pcgbuf = &render.pcgbuf[ (pcgno << 8) ];
			if (bgdata & 0x20000) {
				pcgbuf += 0x80;
			}
			if (bgdata & 0x40000) {
				pcgbuf += 8;
			}

			if (bgdata & 0x8000) {
				// ?????]
				pcgbuf += 0x70;
				for (j=0; j<8; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = reinterpret_cast<DWORD*>(static_cast<size_t>(bgdata));
					pcgbuf -= 0x10;
					ptr += 128;
				}
			}
			else {
				// ???
				for (j=0; j<8; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = reinterpret_cast<DWORD*>(static_cast<size_t>(bgdata));
					pcgbuf += 0x10;
					ptr += 128;
				}
			}
			ptr -= 1024;
		}

		// ?o?^????(PCG)
		render.pcguse[ pcgno ]++;
		pcgno = (pcgno >> 8) & 0x0f;
		render.pcgpal[ pcgno ]++;

		// ?|?C???^??i???
		ptr += 2;
	}
}

//===========================================================================
//
//	?????_??(??????)
//
//===========================================================================

void FASTCALL Render::Mix(int y)
{
	DWORD *p;
	DWORD *q;
	DWORD *r;
	DWORD *ptr[3];
	int offset;
	DWORD buf[1024];

	if (compositor_mode == compositor_fast) {
		MixFast(y);
		return;
	}

	// ?????w???????????A?????o?b?t?@?????????Ay?I?[?o?[???return
	if ((!render.mix[y]) || (!render.mixbuf)) {
		return;
	}
	if (render.mixheight <= y) {
		return;
	}
	ASSERT(render.mixlen > 0);


	// Original compositor: if half-transparency / special priority is active,
	// use the px68k-style scanline compositor so effects like transparent clouds match.
	 {
		const VC::vc_t *vp = (vc ? vc->GetWorkAddr() : NULL);
		if (vp) {
			const BYTE vr2h = (BYTE)vp->vr2h;
			const BOOL tr_mode = (BOOL)((vr2h & 0x5d) == 0x1d);
			const BOOL dim_mode = (BOOL)((vr2h & 0x5d) == 0x1c);
			const BOOL pri_mode = (BOOL)((vr2h & 0x5c) == 0x14);
			if (transparency_enabled &&
				(vp->hp || vp->exon || vp->gg || vp->gt || vp->ah || vp->vht || tr_mode || dim_mode || pri_mode)) {
				MixFastLine(y, y);
				return;
			}
		}
	}

#if defined(REND_LOG)
	LOG1(Log::Normal, "???? y=%d", y);
#endif	// REND_LOG

	// ?t???OOFF?A?????o?b?t?@?A?h???X??????
	render.mix[y] = FALSE;
	q = &render.mixbuf[render.mixwidth * y];

	switch (render.mixtype) {
		// ?^?C?v0(?\???????)
		case 0:
			RendMix00(q, render.drawflag + (y << 6), render.mixlen);
			return;

		// ?^?C?v1(1????)
		case 1:
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			ASSERT(p);
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			RendMix01(q, p, render.drawflag + (y << 6), render.mixlen);
			return;

		// ?^?C?v2(2??A?J???[0?d?????)
		case 2:
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			ASSERT(p);
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			offset = (*render.mixy[1] + y) & render.mixand[1];
			r = render.mixptr[1];
			ASSERT(r);
			r += (offset << render.mixshift[1]);
			r += (*render.mixx[1] & render.mixand[1]);
			if (cmov) {
				RendMix02C(q, p, r, render.drawflag + (y << 6), render.mixlen);
			}
			else {
				RendMix02(q, p, r, render.drawflag + (y << 6), render.mixlen);
			}
			return;

		// ?^?C?v3(2??A???d?????)
		case 3:
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			ASSERT(p);
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			offset = (*render.mixy[1] + y) & render.mixand[1];
			r = render.mixptr[1];
			ASSERT(r);
			r += (offset << render.mixshift[1]);
			r += (*render.mixx[1] & render.mixand[1]);
			if (cmov) {
				RendMix03C(q, p, r, render.drawflag + (y << 6), render.mixlen);
			}
			else {
				RendMix03(q, p, r, render.drawflag + (y << 6), render.mixlen);
			}
			return;

		// ?^?C?v4(?O???t?B?b?N???3?? or 4??)
		case 4:

			MixGrp(y, buf);
			RendMix01(q, buf, render.drawflag + (y << 6), render.mixlen);
			break;

		// ?^?C?v5(?O???t?B?b?N?{?e?L?X?g?A?e?L?X?g?D????d?????)
		case 5:
			MixGrp(y, buf);
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			ASSERT(p);
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			if (cmov) {
				RendMix03C(q, p, buf, render.drawflag + (y << 6), render.mixlen);
			}
			else {
				RendMix03(q, p, buf, render.drawflag + (y << 6), render.mixlen);
			}
			return;

		// ?^?C?v6(?O???t?B?b?N?{?e?L?X?g?A?O???t?B?b?N?D????d?????)
		case 6:
			MixGrp(y, buf);
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			if (cmov) {
				RendMix03C(q, buf, p, render.drawflag + (y << 6), render.mixlen);
			}
			else {
				RendMix03(q, buf, p, render.drawflag + (y << 6), render.mixlen);
			}
			return;

		// ?^?C?v7(?e?L?X?g?{?X?v???C?g?{?O???t?B?b?N1??)
		case 7:
			offset = (*render.mixy[0] + y) & render.mixand[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (offset << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] & render.mixand[0]);
			offset = (*render.mixy[1] + y) & render.mixand[1];
			ptr[1] = render.mixptr[1];
			ptr[1] += (offset << render.mixshift[1]);
			ptr[1] += (*render.mixx[1] & render.mixand[1]);
			offset = (*render.mixy[2] + y) & render.mixand[2];
			ptr[2] = render.mixptr[2];
			ptr[2] += (offset << render.mixshift[2]);
			ptr[2] += (*render.mixx[2] & render.mixand[2]);
			if (cmov) {
				RendMix04C(q, ptr[render.mixmap[0]], ptr[render.mixmap[1]],
					ptr[render.mixmap[2]], render.drawflag + (y << 6), render.mixlen);
			}
			else {
				RendMix04(q, ptr[render.mixmap[0]], ptr[render.mixmap[1]],
					ptr[render.mixmap[2]], render.drawflag + (y << 6), render.mixlen);
			}
			return;

		// ?^?C?v8(?e?L?X?g+?X?v???C?g+?O???t?B?b?N?Q????)
		case 8:
			MixGrp(y, buf);
			offset = (*render.mixy[0] + y) & render.mixand[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (offset << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] & render.mixand[0]);
			offset = (*render.mixy[1] + y) & render.mixand[1];
			ptr[1] = render.mixptr[1];
			ptr[1] += (offset << render.mixshift[1]);
			ptr[1] += (*render.mixx[1] & render.mixand[1]);
			ptr[2] = buf;
			if (cmov) {
				RendMix04C(q, ptr[render.mixmap[0]], ptr[render.mixmap[1]],
					ptr[render.mixmap[2]], render.drawflag + (y << 6), render.mixlen);
			}
			else {
				RendMix04(q, ptr[render.mixmap[0]], ptr[render.mixmap[1]],                                    	
					ptr[render.mixmap[2]], render.drawflag + (y << 6), render.mixlen);
			}
			return;

		// ?????
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	?O???t?B?b?N????
//
//---------------------------------------------------------------------------
void FASTCALL Render::MixGrp(int y, DWORD *buf)
{
	DWORD *p;
	DWORD *q;
	DWORD *r;
	DWORD *s;
	int offset;

	ASSERT(buf);
	ASSERT((y >= 0) && (y < render.mixheight));

	switch (render.mixpage) {
		// ???(????render.mixpage??z??)
		case 0:
			ASSERT(FALSE);
			return;

		// 1??(????render.mixpage??z??)
		case 1:
			ASSERT(FALSE);
			return;

		// 2??
		case 2:
			offset = (*render.mixy[4] + y) & render.mixand[4];
			p = render.mixptr[4];
			ASSERT(p);
			p += (offset << render.mixshift[4]);
			p += (*render.mixx[4] & render.mixand[4]);
			offset = (*render.mixy[5] + y) & render.mixand[5];
			q = render.mixptr[5];
			ASSERT(q);
			q += (offset << render.mixshift[5]);
			q += (*render.mixx[5] & render.mixand[5]);

			if (cmov) {
				RendGrp02C(buf, p, q, render.mixlen);
			}
			else {
				RendGrp02(buf, p, q, render.mixlen);
			}
			break;

		// 3??
		case 3:
			offset = (*render.mixy[4] + y) & render.mixand[4];
			p = render.mixptr[4];
			ASSERT(p);
			p += (offset << render.mixshift[4]);
			p += (*render.mixx[4] & render.mixand[4]);

			offset = (*render.mixy[5] + y) & render.mixand[5];
			q = render.mixptr[5];
			ASSERT(q);
			q += (offset << render.mixshift[5]);
			q += (*render.mixx[5] & render.mixand[5]);

			offset = (*render.mixy[6] + y) & render.mixand[6];
			r = render.mixptr[6];
			ASSERT(r);
			r += (offset << render.mixshift[6]);
			r += (*render.mixx[6] & render.mixand[6]);

			if (cmov) {
				RendGrp03C(buf, p, q, r, render.mixlen);
			}
			else {
				RendGrp03(buf, p, q, r, render.mixlen);
			}
			break;

		// 4??
		case 4:

			offset = (*render.mixy[4] + y) & render.mixand[4];
			p = render.mixptr[4];
			ASSERT(p);
			p += (offset << render.mixshift[4]);
			p += (*render.mixx[4] & render.mixand[4]);

			offset = (*render.mixy[5] + y) & render.mixand[5];
			q = render.mixptr[5];
			ASSERT(q);
			q += (offset << render.mixshift[5]);
			q += (*render.mixx[5] & render.mixand[5]);

			offset = (*render.mixy[6] + y) & render.mixand[6];
			r = render.mixptr[6];
			ASSERT(r);
			r += (offset << render.mixshift[6]);
			r += (*render.mixx[6] & render.mixand[6]);

			offset = (*render.mixy[7] + y) & render.mixand[7];
			s = render.mixptr[7];
			ASSERT(s);
			s += (offset << render.mixshift[7]);
			s += (*render.mixx[7] & render.mixand[7]);

			RendGrp04(buf, p, q, r, s, render.mixlen);
			return;

		// ?????
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	?????o?b?t?@?�E��E�
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetMixBuf() const
{
	ASSERT(this);

	// NULL????????
	return render.mixbuf;
}

//---------------------------------------------------------------------------
//
//	?????_?????O
//
//---------------------------------------------------------------------------
void FASTCALL Render::Process()
{
	int i;

	// ?s??????????????s?v
	if (render.first >= render.last) {
		return;
	}

	if (compositor_mode == compositor_fast) {
		ProcessFast();
		return;
	}

	// VC
	if (render.vc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?r?f?I????");
#endif	// RENDER_LOG
		Video();
	}

	// ?R???g???X?g
	if (render.contrast) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?R???g???X?g????");
#endif	// RENDER_LOG
		Contrast();
	}

	// ?p???b?g
	if (render.palette) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?p???b?g????");
#endif	// RENDER_LOG
		Palette();
	}

	// first==0??A?X?v???C?g??\??ON/OFF?????
	if (render.first == 0) {
		if (sprite->IsDisplay()) {
			if (!render.bgspdisp) {
				// ?X?v???C?gCPU??Video
				for (i=0; i<512; i++) {
					render.bgspmod[i] = TRUE;
				}
				render.bgspdisp = TRUE;
			}
		}
		else {
			if (render.bgspdisp) {
				// ?X?v???C?gVideo??CPU
				for (i=0; i<512; i++) {
					render.bgspmod[i] = TRUE;
				}
				render.bgspdisp = FALSE;
			}
		}
	}

	// ????x2???
	if ((render.v_mul == 2) && !render.lowres) {
		// I/O????g?�E��E�????A?c??????????????????
		for (i=render.first; i<render.last; i++) {
			if ((i & 1) == 0) {
				Text(i >> 1);
				Grp(0, i >> 1);
				Grp(1, i >> 1);
				Grp(2, i >> 1);
				Grp(3, i >> 1);
				BGSprite(i >> 1);
				Mix(i >> 1);
			}
		}
		// ?X?V
		render.first = render.last;
		return;
	}

	// ?C???^???[?X???
	if ((render.v_mul == 0) && render.lowres) {
		// ????E??????????(???@??????)
		for (i=render.first; i<render.last; i++) {
			// ?e?L?X?g?E?O???t?B?b?N
			Text((i << 1) + 0);
			Text((i << 1) + 1);
			Grp(0, (i << 1) + 0);
			Grp(0, (i << 1) + 1);
			Grp(1, (i << 1) + 0);
			Grp(1, (i << 1) + 1);
			Grp(2, (i << 1) + 0);
			Grp(2, (i << 1) + 1);
			Grp(3, (i << 1) + 0);
			Grp(3, (i << 1) + 1);
			BGSprite((i << 1) + 0);
			BGSprite((i << 1) + 1);
			Mix((i << 1) + 0);
			Mix((i << 1) + 1);
		}
		// ?X?V
		render.first = render.last;
		return;
	}

	// ????[?v
	for (i=render.first; i<render.last; i++) {
		Text(i);
		Grp(0, i);
		Grp(1, i);
		Grp(2, i);
		Grp(3, i);
		BGSprite(i);
		Mix(i);
	}
	// ?X?V
	render.first = render.last;
}









