//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ?o?h?D(ytanaka@ipc-tokai.or.jp)
//	[ oo?_o ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "config.h"
#include "vm.h"
#include "crtc.h"
#include "vc.h"
#include "tvram.h"
#include "gvram.h"
#include "sprite.h"
#include "rend_soft.h"
#include "graphic_engine.h"
#include "render.h"
#include "px68k_render_adapter.h"

BOOL FASTCALL IsCMOV(void);

//===========================================================================
//
//	oo?_o
//
//===========================================================================
//#define REND_LOG

//---------------------------------------------------------------------------
//
//	oo`
//
//---------------------------------------------------------------------------
#define REND_COLOR0		0x80000000		// ?Jo?[0?to?O(rend_asm.asmog?p)
#define REND_FAST_IBIT	0x40000000		// Fast render Ibit marker (XRGB8888: top byte ignored)

static DWORD FASTCALL ConvFast565IToXrgb8888(WORD color)
{
	const DWORD r5 = (DWORD)((color >> 11) & 0x1f);
	const DWORD g5 = (DWORD)((color >> 6) & 0x1f);
	const DWORD b5 = (DWORD)(color & 0x1f);
	const DWORD r8 = (r5 << 3) | (r5 >> 2);
	const DWORD g8 = (g5 << 3) | (g5 >> 2);
	const DWORD b8 = (b5 << 3) | (b5 >> 2);
	DWORD out = (r8 << 16) | (g8 << 8) | b8;

	if (color & 0x20) {
		out |= REND_FAST_IBIT;
	}
	return out;
}

static int FASTCALL CalcBGHAdjustPixels(int compositor_mode, const CRTC *crtc, const Sprite *sprite)
{
	(void)compositor_mode;
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

static void FASTCALL Px68kCrtcHostApplyStateView(void *ctx, const Px68kCrtcStateView *view)
{
	Render *owner = (Render*)ctx;

	if (!owner || !view) {
		return;
	}

	owner->CachePx68kStateView(view);
}

static void FASTCALL Px68kCrtcHostMarkAllTextDirty(void *ctx)
{
	Render *owner = (Render*)ctx;
	Render::render_t *work;
	int i;

	if (!owner) {
		return;
	}

	work = owner->GetWorkAddr();
	if (!work) {
		return;
	}

	if (work->textmod) {
		for (i = 0; i < 1024; i++) {
			work->textmod[i] = TRUE;
			work->mix[i] = TRUE;
			work->draw[i] = TRUE;
		}
	}
	if (work->textflag) {
		for (i = 0; i < 1024 * 32; i++) {
			work->textflag[i] = TRUE;
		}
	}
	work->palette = TRUE;
	work->vc = TRUE;
	work->crtc = TRUE;
}

static void FASTCALL Px68kCrtcHostMarkTextDirtyLine(void *ctx, DWORD line)
{
	Render *owner = (Render*)ctx;
	Render::render_t *work;

	if (!owner) {
		return;
	}

	work = owner->GetWorkAddr();
	if (!work) {
		return;
	}

	line &= 0x3ff;
	work->textmod[line] = TRUE;
	work->mix[line] = TRUE;
	work->draw[line] = TRUE;
}

static void FASTCALL Px68kCrtcHostScreenChanged(void *ctx)
{
	Render *owner = (Render*)ctx;

	if (!owner) {
		return;
	}

	owner->SetCRTC();
}

static void FASTCALL Px68kCrtcHostGeometryChanged(void *ctx)
{
	Render *owner = (Render*)ctx;

	if (!owner) {
		return;
	}

	owner->SetCRTC();
}

static void FASTCALL Px68kCrtcHostTimingChanged(void *ctx)
{
	Render *owner = (Render*)ctx;

	if (!owner) {
		return;
	}

	owner->SetVC();
}

static void FASTCALL Px68kCrtcHostModeChanged(void *ctx, BYTE /*mode*/)
{
	Render *owner = (Render*)ctx;

	if (!owner) {
		return;
	}

	owner->SetCRTC();
}

static void FASTCALL Px68kCrtcHostTextScrollChanged(void *ctx, DWORD x, DWORD y)
{
	Render *owner = (Render*)ctx;

	if (!owner) {
		return;
	}

	owner->TextScrl(x, y);
}

static void FASTCALL Px68kCrtcHostGrpScrollChanged(void *ctx, int block, DWORD x, DWORD y)
{
	Render *owner = (Render*)ctx;

	if (!owner) {
		return;
	}

	owner->GrpScrl(block, x, y);
}

class XmVideoSnapshotAdapter
{
public:
	struct frame_t {
		CRTC::crtc_t crtc_storage;
		Px68kCrtcStateView px68k_crtc_storage;
		VC::vc_t vc_storage;
		Sprite::sprite_t sprite_storage;
		DWORD palette_storage[0x200];
		const Render::render_t *work;
		const CRTC::crtc_t *crtc_work;
		const Px68kCrtcStateView *px68k_crtc;
		const VC::vc_t *vc_work;
		const Sprite::sprite_t *sprite_work;
		const DWORD *palette;
		const DWORD *text_buf;
		const DWORD *grp_buf[4];
		const DWORD *bgsp_buf;
		const DWORD *mix_buf;
		unsigned frame_index;
		int compositor_mode;
		BOOL render_fast_dummy_enabled;
		BOOL render_active;
		BOOL render_enabled;
		BOOL render_ready;
		BOOL render_vc_dirty;
		BOOL render_crtc_dirty;
		BOOL render_palette_dirty;
		BOOL render_text_enabled;
		BOOL render_bgsp_enabled;
		int first;
		int last;
		int width;
		int height;
		int h_mul;
		int v_mul;
		int mixlen;
		int mixtype;
		int mixpage;
		DWORD textx;
		DWORD texty;
		DWORD grpx[4];
		DWORD grpy[4];
		DWORD bgx[2];
		DWORD bgy[2];
	};

	struct line_t {
		const CRTC::crtc_t *crtc_work;
		const Px68kCrtcStateView *px68k_crtc;
		const VC::vc_t *vc_work;
		const Sprite::sprite_t *sprite_work;
		unsigned frame_index;
		int raster;
		BOOL valid;
		BOOL mix_dirty;
		BOOL draw_dirty;
		BOOL render_vc_dirty;
		BOOL render_crtc_dirty;
		BOOL render_palette_dirty;
		BOOL render_text_enabled;
		BOOL render_bgsp_enabled;
		int first;
		int last;
		int mixlen;
		int mixtype;
		int mixpage;
	};

	XmVideoSnapshotAdapter()
	{
		Reset();
	}

	void Reset()
	{
		memset(&frame, 0, sizeof(frame));
		memset(&line, 0, sizeof(line));
		memset(&frame.crtc_storage, 0, sizeof(frame.crtc_storage));
		memset(&frame.px68k_crtc_storage, 0, sizeof(frame.px68k_crtc_storage));
		memset(&frame.vc_storage, 0, sizeof(frame.vc_storage));
		memset(&frame.sprite_storage, 0, sizeof(frame.sprite_storage));
		memset(&frame.palette_storage, 0, sizeof(frame.palette_storage));
		frame.crtc_work = NULL;
		frame.px68k_crtc = NULL;
		frame.vc_work = NULL;
		frame.sprite_work = NULL;
		frame.palette = NULL;
		frame.text_buf = NULL;
		frame.bgsp_buf = NULL;
		frame.mix_buf = NULL;
		for (int i = 0; i < 4; i++) {
			frame.grp_buf[i] = NULL;
			frame.grpx[i] = 0;
			frame.grpy[i] = 0;
		}
		for (int i = 0; i < 2; i++) {
			frame.bgx[i] = 0;
			frame.bgy[i] = 0;
		}
		frame.frame_index = 0;
		line.frame_index = 0;
		line.raster = -1;
		line.crtc_work = NULL;
		line.px68k_crtc = NULL;
		line.vc_work = NULL;
		line.sprite_work = NULL;
		frame_index = 0;
		frame_valid = FALSE;
		line_valid = FALSE;
	}

	void BeginFrame(const Render *owner)
	{
		const Render::render_t *work;
		const CRTC *crtc;
		const VC *vc;
		const Sprite *sprite;

		if (!owner) {
			return;
		}

		work = owner->GetWorkAddr();
		crtc = owner->GetCRTCDevice();
		vc = owner->GetVCDevice();
		sprite = owner->GetSpriteDevice();
		memset(&frame, 0, sizeof(frame));
		memset(&frame.crtc_storage, 0, sizeof(frame.crtc_storage));
		memset(&frame.px68k_crtc_storage, 0, sizeof(frame.px68k_crtc_storage));
		memset(&frame.vc_storage, 0, sizeof(frame.vc_storage));
		memset(&frame.sprite_storage, 0, sizeof(frame.sprite_storage));
		memset(&frame.palette_storage, 0, sizeof(frame.palette_storage));
		frame.work = work;
		if (crtc) {
			frame.crtc_storage = *crtc->GetWorkAddr();
			frame.px68k_crtc_storage = *crtc->GetPx68kStateView();
			frame.crtc_work = &frame.crtc_storage;
			frame.px68k_crtc = &frame.px68k_crtc_storage;
		}
		else {
			frame.crtc_work = NULL;
			frame.px68k_crtc = NULL;
		}
		if (vc) {
			frame.vc_storage = *vc->GetWorkAddr();
			frame.vc_work = &frame.vc_storage;
		}
		else {
			frame.vc_work = NULL;
		}
		if (owner->GetPalette()) {
			memcpy(frame.palette_storage, owner->GetPalette(), sizeof(frame.palette_storage));
			frame.palette = frame.palette_storage;
		}
		else {
			frame.palette = NULL;
		}
		if (sprite) {
			sprite->GetSprite(&frame.sprite_storage);
			frame.sprite_work = &frame.sprite_storage;
		}
		else {
			memset(&frame.sprite_storage, 0, sizeof(frame.sprite_storage));
			frame.sprite_work = NULL;
		}
		frame.text_buf = owner->GetTextBuf();
		for (int i = 0; i < 4; i++) {
			frame.grp_buf[i] = owner->GetGrpBuf(i);
		}
		frame.bgsp_buf = owner->GetBGSpBuf();
		frame.mix_buf = owner->GetMixBuf();
		frame.frame_index = ++frame_index;
		frame.compositor_mode = owner->GetCompositorMode();
		frame.render_fast_dummy_enabled = owner->IsRenderFastDummyEnabled();
		frame.render_active = owner->IsActive();
		frame.render_enabled = work ? work->enable : FALSE;
		frame.render_ready = work ? (work->count > 0) : FALSE;
		frame.render_vc_dirty = work ? work->vc : FALSE;
		frame.render_crtc_dirty = work ? work->crtc : FALSE;
		frame.render_palette_dirty = work ? work->palette : FALSE;
		frame.render_text_enabled = work ? work->texten : FALSE;
		frame.render_bgsp_enabled = work ? work->bgspflag : FALSE;
		frame.first = work ? work->first : 0;
		frame.last = work ? work->last : 0;
		frame.width = work ? work->width : 0;
		frame.height = work ? work->height : 0;
		frame.h_mul = work ? work->h_mul : 1;
		frame.v_mul = work ? work->v_mul : 1;
		frame.mixlen = work ? work->mixlen : 0;
		frame.mixtype = work ? work->mixtype : 0;
		frame.mixpage = work ? work->mixpage : 0;
		frame.textx = work ? work->textx : 0;
		frame.texty = work ? work->texty : 0;
		for (int i = 0; i < 4; i++) {
			frame.grpx[i] = work ? work->grpx[i] : 0;
			frame.grpy[i] = work ? work->grpy[i] : 0;
		}
		for (int i = 0; i < 2; i++) {
			frame.bgx[i] = work ? work->bgx[i] : 0;
			frame.bgy[i] = work ? work->bgy[i] : 0;
		}
		frame_valid = TRUE;
		line_valid = FALSE;
	}

	void CaptureLine(const Render *owner, int raster)
	{
		const Render::render_t *work;

		if (!owner) {
			return;
		}
		if (!frame_valid) {
			BeginFrame(owner);
		}
		work = owner->GetWorkAddr();
		line.frame_index = frame.frame_index;
		line.raster = raster;
		line.crtc_work = frame.crtc_work;
		line.px68k_crtc = frame.px68k_crtc;
		line.vc_work = frame.vc_work;
		line.sprite_work = frame.sprite_work;
		line.valid = TRUE;
		line.mix_dirty = (work && (raster >= 0) && (raster < 1024)) ? work->mix[raster] : FALSE;
		line.draw_dirty = (work && (raster >= 0) && (raster < 1024)) ? work->draw[raster] : FALSE;
		line.render_vc_dirty = work ? work->vc : FALSE;
		line.render_crtc_dirty = work ? work->crtc : FALSE;
		line.render_palette_dirty = work ? work->palette : FALSE;
		line.render_text_enabled = work ? work->texten : FALSE;
		line.render_bgsp_enabled = work ? work->bgspflag : FALSE;
		line.first = work ? work->first : 0;
		line.last = work ? work->last : 0;
		line.mixlen = work ? work->mixlen : 0;
		line.mixtype = work ? work->mixtype : 0;
		line.mixpage = work ? work->mixpage : 0;
		line_valid = TRUE;
	}

	void EndFrame()
	{
		line_valid = FALSE;
	}

private:
	frame_t frame;
	line_t line;
	unsigned frame_index;
	BOOL frame_valid;
	BOOL line_valid;
};

//---------------------------------------------------------------------------
//
//	?Ro?X?go?N?^
//
//---------------------------------------------------------------------------
Render::Render(VM *p) : Device(p)
{
	// ?f?o?C?XIDooo?
	dev.id = MAKEID('R', 'E', 'N', 'D');
	dev.desc = "Renderer";

	// ?f?o?C?X?|?Co?^
	crtc = NULL;
	vc = NULL;
	tvram = NULL;
	gvram = NULL;
	sprite = NULL;
	backend = NULL;
	backend_original = NULL;
	backend_px68k = NULL;
	video_snapshot_adapter = NULL;
	px68k_adapter = NULL;
	palbuf_original = NULL;
	palbuf_fast = NULL;
	render_target = NULL;
	fast_fallback_count = 0;
	transparency_enabled = TRUE;
	original_bg0_render_enabled = TRUE;
	compositor_mode = compositor_original;
	compositor_mode_pending = compositor_original;
	compositor_mode_switch_pending = FALSE;
	render_fast_dummy_enabled = FALSE;
	memset(&px68k_crtc_host, 0, sizeof(px68k_crtc_host));
	memset(&px68k_crtc_state_cache, 0, sizeof(px68k_crtc_state_cache));
	px68k_crtc_host.ctx = this;
	px68k_crtc_host.ApplyStateView = Px68kCrtcHostApplyStateView;
	px68k_crtc_host.MarkAllTextDirty = Px68kCrtcHostMarkAllTextDirty;
	px68k_crtc_host.MarkTextDirtyLine = Px68kCrtcHostMarkTextDirtyLine;
	px68k_crtc_host.ScreenChanged = Px68kCrtcHostScreenChanged;
	px68k_crtc_host.GeometryChanged = Px68kCrtcHostGeometryChanged;
	px68k_crtc_host.TimingChanged = Px68kCrtcHostTimingChanged;
	px68k_crtc_host.ModeChanged = Px68kCrtcHostModeChanged;
	px68k_crtc_host.TextScrollChanged = Px68kCrtcHostTextScrollChanged;
	px68k_crtc_host.GrpScrollChanged = Px68kCrtcHostGrpScrollChanged;
	render.fast_stamp_counter = 1;
	memset(render.fast_mix_stamp, 0, sizeof(render.fast_mix_stamp));
	memset(render.fast_mix_done, 0, sizeof(render.fast_mix_done));
	memset(render.fast_bg_stamp, 0, sizeof(render.fast_bg_stamp));
	memset(render.fast_bg_done, 0, sizeof(render.fast_bg_done));

	// o?[?N?Go?Aooo(CRTC)
	render.crtc = FALSE;
	render.width = 768;
	render.h_mul = 1;
	render.height = 512;
	render.v_mul = 1;

	// o?[?N?Go?Aooo(?po?b?g)
	render.palbuf = NULL;
	render.palptr = NULL;
	render.palvc = NULL;

	// o?[?N?Go?Aooo(?e?L?X?g)
	render.textflag = NULL;
	render.texttv = NULL;
	render.textbuf = NULL;
	render.textout = NULL;

	// o?[?N?Go?Aooo(?Oo?t?B?b?N)
	render.grpflag = NULL;
	render.grpgv = NULL;
	render.grpbuf[0] = NULL;
	render.grpbuf[1] = NULL;
	render.grpbuf[2] = NULL;
	render.grpbuf[3] = NULL;

	// o?[?N?Go?Aooo(PCG,?X?vo?C?g,BG)
	render.pcgbuf = NULL;
	render.spptr = NULL;
	render.bgspbuf = NULL;
	render.zero = NULL;
	render.bgptr[0] = NULL;
	render.bgptr[1] = NULL;

	// o?[?N?Go?Aooo(oo)
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

	// o?[?N?Go?Aooo(?`o)
	render.drawflag = NULL;

	// oo?
	cmov = FALSE;
}

//---------------------------------------------------------------------------
//
//	ooo
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Init()
{
	int i;

	ASSERT(this);

	// o{?No?X
	if (!Device::Init()) {
		return FALSE;
	}

	// CRTC?ï¿½Eï¿½ï¿½Eï¿½
	crtc = (CRTC*)vm->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
	ASSERT(crtc);

	// VC?ï¿½Eï¿½ï¿½Eï¿½
	vc = (VC*)vm->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(vc);

	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);

	// ?po?b?g?o?b?t?@?mo(4MB x2: original + fast render compatible)
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

	// ?e?L?X?gVRAM?o?b?t?@?mo(4.7MB)
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

	// ?Oo?t?B?b?NVRAM?o?b?t?@?mo(8.2MB)
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

	// PCG?o?b?t?@?mo(4MB)
	try {
		render.pcgbuf = new DWORD[ 16 * 256 * 16 * 16 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.pcgbuf) {
		return FALSE;
	}

	// ?X?vo?C?g?|?Co?^?mo(256KB)
	try {
		render.spptr = new DWORD*[ 128 * 512 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.spptr) {
		return FALSE;
	}

	// BG?|?Co?^?mo(768KB)
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

	// BG/?X?vo?C?g?o?b?t?@?mo(1MB)
	try {
		render.bgspbuf = new DWORD[ 512 * 512 + 16];	// +16obo[?u
	}
	catch (...) {
		return FALSE;
	}
	if (!render.bgspbuf) {
		return FALSE;
	}

	// ?`oto?O?o?b?t?@?mo(256KB)
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

	// ?po?b?go
	MakePalette();

	// oooo[?N?Go?A
	render.contlevel = 0;
	cmov = ::IsCMOV();

	try {
		backend_original = new OriginalGraphicEngine();
		backend_px68k = new Px68kGraphicEngine();
		video_snapshot_adapter = new XmVideoSnapshotAdapter();
		px68k_adapter = new Px68kRenderAdapter();
	}
	catch (...) {
		if (backend_original) {
			delete backend_original;
			backend_original = NULL;
		}
		if (backend_px68k) {
			delete backend_px68k;
			backend_px68k = NULL;
		}
		if (video_snapshot_adapter) {
			delete video_snapshot_adapter;
			video_snapshot_adapter = NULL;
		}
		if (px68k_adapter) {
			delete px68k_adapter;
			px68k_adapter = NULL;
		}
		return FALSE;
	}
	if (!backend_original) {
		return FALSE;
	}
	if (!backend_px68k) {
		return FALSE;
	}
	if (!video_snapshot_adapter) {
		return FALSE;
	}
	if (!px68k_adapter) {
		return FALSE;
	}
	if (!px68k_adapter->Init()) {
		delete px68k_adapter;
		px68k_adapter = NULL;
		return FALSE;
	}
	if (!SetCompositorMode(render_fast_dummy_enabled ? compositor_fast : compositor_original)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	?No?[o?A?b?v
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
	if (backend_px68k) {
		delete backend_px68k;
		backend_px68k = NULL;
	}
	if (video_snapshot_adapter) {
		delete video_snapshot_adapter;
		video_snapshot_adapter = NULL;
	}
	if (px68k_adapter) {
		px68k_adapter->Cleanup();
		delete px68k_adapter;
		px68k_adapter = NULL;
	}
	backend = NULL;
	memset(&px68k_crtc_host, 0, sizeof(px68k_crtc_host));
	render_fast_dummy_enabled = FALSE;
	compositor_mode_switch_pending = FALSE;

	// ?`oto?O
	if (render.drawflag) {
		delete[] render.drawflag;
		render.drawflag = NULL;
	}

	// BG/?X?vo?C?g?o?b?t?@
	if (render.bgspbuf) {
		delete[] render.bgspbuf;
		render.bgspbuf = NULL;
	}

	// BG?|?Co?^
	if (render.bgptr[0]) {
		delete[] render.bgptr[0];
		render.bgptr[0] = NULL;
	}
	if (render.bgptr[1]) {
		delete[] render.bgptr[1];
		render.bgptr[1] = NULL;
	}

	// ?X?vo?C?g?|?Co?^
	if (render.spptr) {
		delete[] render.spptr;
		render.spptr = NULL;
	}

	// PCG?o?b?t?@
	if (render.pcgbuf) {
		delete[] render.pcgbuf;
		render.pcgbuf = NULL;
	}

	// ?Oo?t?B?b?NVRAM?o?b?t?@
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

	// ?po?b?g?o?b?t?@ (original + fast)
	if (palbuf_original) {
		delete[] palbuf_original;
		palbuf_original = NULL;
	}
	if (palbuf_fast) {
		delete[] palbuf_fast;
		palbuf_fast = NULL;
	}
	render.palbuf = NULL;
	tvram = NULL;
	gvram = NULL;
	sprite = NULL;
	crtc = NULL;
	vc = NULL;
	render_target = NULL;

	// o{?No?Xo
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	o?Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::Reset()
{
	int i;
	int j;
	int k;
	DWORD **ptr;

	ASSERT(this);
	LOG0(Log::Normal, "???Z?b?g");

	// ?r?f?I?Ro?go?[oo?|?Co?^?ï¿½Eï¿½ï¿½Eï¿½
	ASSERT(vc);
	render.palvc = (const WORD*)vc->GetPalette();

	// ?e?L?X?gVRAMo?|?Co?^?ï¿½Eï¿½ï¿½Eï¿½
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);
	render.texttv = tvram->GetTVRAM();

	// ?Oo?t?B?b?NVRAMo?|?Co?^?ï¿½Eï¿½ï¿½Eï¿½
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);
	render.grpgv = gvram->GetGVRAM();

	// ?X?vo?C?g?Ro?go?[oo?|?Co?^?ï¿½Eï¿½ï¿½Eï¿½
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);
	render.sprmem = sprite->GetPCG() - 0x8000;

	// o?[?N?Go?Aooo
	render.first = 0;
	render.last = 0;
	render.enable = TRUE;
	render.act = TRUE;
	render.count = 2;
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
	compositor_mode_switch_pending = FALSE;
	compositor_mode_pending = compositor_mode;
	if (video_snapshot_adapter) {
		video_snapshot_adapter->Reset();
	}

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

	// o?[?N?Go?Aooo(crtc, vc)
	render.crtc = FALSE;
	render.vc = FALSE;

	// o?[?N?Go?Aooo(?Ro?go?X?g)
	render.contrast = FALSE;

	// o?[?N?Go?Aooo(?po?b?g)
	render.palette = FALSE;
	render.palptr = render.palbuf;

	// o?[?N?Go?Aooo(?e?L?X?g)
	render.texten = FALSE;
	render.textx = 0;
	render.texty = 0;

	// o?[?N?Go?Aooo(?Oo?t?B?b?N)
	for (i=0; i<4; i++) {
		render.grpen[i] = FALSE;
		render.grpx[i] = 0;
		render.grpy[i] = 0;
	}
	render.grptype = 4;

	// o?[?N?Go?Aooo(PCG)
	// o?Z?b?goo?BG,Spriteooo?\oooo?PCGoog?p
	memset(render.pcgready, 0, sizeof(render.pcgready));
	memset(render.pcguse, 0, sizeof(render.pcguse));
	memset(render.pcgpal, 0, sizeof(render.pcgpal));

	// o?[?N?Go?Aooo(?X?vo?C?g)
	memset(render.spptr, 0, sizeof(DWORD*) * 128 * 512);
	memset(render.spreg, 0, sizeof(render.spreg));
	memset(render.spuse, 0, sizeof(render.spuse));

	// o?[?N?Go?Aooo(BG)
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

	// o?[?N?Go?Aooo(BG/?X?vo?C?g)
	render.bgspflag = FALSE;
	render.bgspdisp = FALSE;
	memset(render.bgspmod, 0, sizeof(render.bgspmod));

	// BGooooooo?(oo?0000)
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

	// o?[?N?Go?Aooo(oo)
	render.mixtype = 0;
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
//	o?[?h
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
//	o?K?p
//
//---------------------------------------------------------------------------
void FASTCALL Render::ApplyCfg(const Config *config)
{
	ASSERT(config);
	LOG0(Log::Normal, "???K?p");

	SetRenderFastDummyEnabled(config->render_fast_dummy);
}

//---------------------------------------------------------------------------
//
//	PX68k Video Engine switch
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::SetRenderFastDummyEnabled(BOOL enable)
{
	render_fast_dummy_enabled = enable ? TRUE : FALSE;
	if (!SetCompositorMode(render_fast_dummy_enabled ? compositor_fast : compositor_original)) {
		return FALSE;
	}
	return render_fast_dummy_enabled;
}

BOOL FASTCALL Render::EnsurePx68kFrame()
{
	if (!render_fast_dummy_enabled || !px68k_adapter || render.act) {
		return FALSE;
	}

	px68k_adapter->StartFrame(this);
	px68k_adapter->DrawFrame(this);
	px68k_adapter->EndFrame(this);
	return TRUE;
}

BOOL FASTCALL Render::GetPx68kScreen(const WORD **out_pixels, int *out_width, int *out_height, int *out_stride) const
{
	const WORD *pixels;
	int width;
	int height;

	if (!render_fast_dummy_enabled || !px68k_adapter || !out_pixels || !out_width || !out_height || !out_stride) {
		return FALSE;
	}

	pixels = px68k_adapter->GetScreenBuffer();
	width = (int)px68k_adapter->GetScreenWidth();
	height = (int)px68k_adapter->GetScreenHeight();
	if (!pixels) {
		return FALSE;
	}
	if (width <= 0) {
		width = render.width;
	}
	if (height <= 0) {
		height = render.height;
	}
	if (width <= 0) {
		width = 768;
	}
	if (height <= 0) {
		height = 512;
	}
	if (width > PX68K_FULLSCREEN_WIDTH) {
		width = PX68K_FULLSCREEN_WIDTH;
	}
	if (height > PX68K_FULLSCREEN_HEIGHT) {
		height = PX68K_FULLSCREEN_HEIGHT;
	}

	*out_pixels = pixels;
	*out_width = width;
	*out_height = height;
	*out_stride = PX68K_FULLSCREEN_WIDTH;
	return TRUE;
}

BOOL FASTCALL Render::SetCompositorMode(int mode)
{
	if ((mode != compositor_original) && (mode != compositor_fast)) {
		return FALSE;
	}
	if ((mode == compositor_original) && !backend_original) {
		return FALSE;
	}
	if ((mode == compositor_fast) && !backend_px68k) {
		return FALSE;
	}

	render_fast_dummy_enabled = (mode == compositor_fast);
	if (mode == compositor_mode) {
		compositor_mode_pending = mode;
		compositor_mode_switch_pending = FALSE;
		return TRUE;
	}
	if (!render.act) {
		ApplyCompositorMode(mode);
		return TRUE;
	}

	compositor_mode_pending = mode;
	compositor_mode_switch_pending = TRUE;
	return TRUE;
}

void FASTCALL Render::ApplyCompositorMode(int mode)
{
	switch (mode) {
	case compositor_original:
		ASSERT(backend_original);
		if (!backend_original) {
			return;
		}
		backend = backend_original;
		compositor_mode = mode;
		render_fast_dummy_enabled = FALSE;
		break;

	case compositor_fast:
		ASSERT(backend_px68k);
		if (!backend_px68k) {
			return;
		}
		backend = backend_px68k;
		compositor_mode = mode;
		render_fast_dummy_enabled = TRUE;
		break;

	default:
		return;
	}

	compositor_mode_pending = compositor_mode;
	compositor_mode_switch_pending = FALSE;
	InvalidateAll();
}

void FASTCALL Render::ApplyPendingCompositorMode()
{
	if (!compositor_mode_switch_pending) {
		return;
	}
	ApplyCompositorMode(compositor_mode_pending);
}

void FASTCALL Render::BeginVideoSnapshotFrame()
{
	if (video_snapshot_adapter) {
		video_snapshot_adapter->BeginFrame(this);
	}
}

void FASTCALL Render::CaptureVideoSnapshotLine(int raster)
{
	if (video_snapshot_adapter) {
		video_snapshot_adapter->CaptureLine(this, raster);
	}
}

void FASTCALL Render::EndVideoSnapshotFrame()
{
	if (video_snapshot_adapter) {
		video_snapshot_adapter->EndFrame();
	}
}

void FASTCALL Render::ComposeVideo()
{
	if (backend) {
		backend->Video(this);
		return;
	}

	Video();
}

//---------------------------------------------------------------------------
//
//	?to?[o?J?n
//
//---------------------------------------------------------------------------
void FASTCALL Render::StartFrame()
{
	ApplyPendingCompositorMode();

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
		if (backend) {
			backend->Process(this);
		}
		else {
			Process();
		}
	}
}

void FASTCALL Render::StartFrameOriginal()
{
	CRTC::crtc_t crtcdata;
	int i;

	ASSERT(this);

	// ooto?[ooX?L?b?voo
	if ((render.count != 0) || !render.enable) {
		render.act = FALSE;
		return;
	}

	// ooto?[oooo_oo?Ooo
	render.act = TRUE;

	// o?X?^oNo?A
	render.first = 0;
	render.last = -1;

	// CRTC?to?Ooo?
	if (render.crtc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "CRTC????");
#endif	// REND_LOG

		// ?f?[?^?ï¿½Eï¿½ï¿½Eï¿½
		crtc->GetCRTC(&crtcdata);

		// h_dots?Av_dotso0oo?
		if ((crtcdata.h_dots == 0) || (crtcdata.v_dots == 0)) {
			return;
		}

		// ooR?s?[
		render.width = crtcdata.h_dots;
		render.h_mul = crtcdata.h_mul;
		render.height = crtcdata.v_dots;
		render.v_mul = crtcdata.v_mul;
		render.lowres = crtcdata.lowres;
		if ((render.v_mul == 2) && !render.lowres) {
			render.height >>= 1;
		}

		// oo?o?b?t?@ooooo?
		render.mixlen = render.width;
		if (render.mixwidth < render.width) {
			render.mixlen = render.mixwidth;
		}

		// ?X?vo?C?go?Z?b?g(mixlenooooo)
		SpriteReset();

		// ?So?Cooo
		for (i=0; i<1024; i++) {
			render.mix[i] = TRUE;
		}

		// ?I?t
		render.crtc = FALSE;
	}

	BeginVideoSnapshotFrame();
}

//---------------------------------------------------------------------------
//
//	?to?[o?Io
//
//---------------------------------------------------------------------------
void FASTCALL Render::EndFrameOriginal()
{
	ASSERT(this);

	// oooï¿½Eï¿½ï¿½Eï¿½ooo
	if (!render.act) {
		return;
	}

	// oooooX?^oo?
	if (render.last > 0) {
		render.last = render.height;
		if (backend) {
			backend->Process(this);
		}
		else {
			Process();
		}
	}

	// ?J?Eo?gUp
	render.count++;
	EndVideoSnapshotFrame();

	// ooo
	render.act = FALSE;
}

//---------------------------------------------------------------------------
//
//	oo?o?b?t?@?Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetMixBuf(DWORD *buf, int width, int height)
{
	int i;

	ASSERT(this);
	ASSERT(width >= 0);
	ASSERT(height >= 0);

	// o?
	render.mixbuf = buf;
	render.mixwidth = width;
	render.mixheight = height;

	// oo?o?b?t?@ooooo?
	render.mixlen = render.width;
	if (render.mixwidth < render.width) {
		render.mixlen = render.mixwidth;
	}

	// ooooo?wo
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
	ASSERT(this);

	// ?to?OONo?
	render.crtc = TRUE;
	render.vc = TRUE;

}

//---------------------------------------------------------------------------
//
//	VC?Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetVCOriginal()
{
	ASSERT(this);

	// ?to?OONo?
	render.vc = TRUE;

}

//---------------------------------------------------------------------------
//
//	VCoo
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

	// VC?to?Oo~o
	render.vc = FALSE;

	// ?to?OON
	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}

	// VC?f?[?^?ACRTC?f?[?^oï¿½Eï¿½ï¿½Eï¿½
	p = vc->GetWorkAddr();
	q = crtc->GetWorkAddr();

	// ?e?L?X?g?C?l?[?uo
	if (p->ton && !q->tmem) {
		render.texten = TRUE;
	}
	else {
		render.texten = FALSE;
	}

	// ?Oo?t?B?b?N?^?C?v
	type = 0;
	if (!p->siz) {
		type = (int)(p->col + 1);
	}
	if (type != render.grptype) {
		render.grptype = type;
		for (i=0; i<512*4; i++) {
			render.grppal[i] = TRUE;
		}
	}

	// ?Oo?t?B?b?N?C?l?[?uo?A?Dox?}?b?v
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

	// ?Oo?t?B?b?N?o?b?t?@oZ?b?g
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

	// ?Doo?ï¿½Eï¿½ï¿½Eï¿½
	tx = p->tx;
	sp = p->sp;
	gr = p->gr;

	// ?^?C?vooo
	render.mixtype = 0;

	// BG/?X?vo?C?g?\ooo
	if ((q->hd >= 2) || (!p->son)) {
		if (render.bgspflag) {
			// BG/?X?vo?C?g?\oON->OFF
			render.bgspflag = FALSE;
			for (i=0; i<512; i++) {
				render.bgspmod[i] = TRUE;
			}
			render.bgspdisp = sprite->IsDisplay();
		}
	}
	else {
		if (!render.bgspflag) {
			// BG/?X?vo?C?g?\oOFF->ON
			render.bgspflag = TRUE;
			for (i=0; i<512; i++) {
				render.bgspmod[i] = TRUE;
			}
			render.bgspdisp = sprite->IsDisplay();
		}
	}

	// o?(q->hd >= 2oo?X?vo?C?goo)
	if ((q->hd >= 2) || (!p->son)) {
		// ?X?vo?C?go?
		if (!render.texten) {
			// ?e?L?X?go?
			if (render.mixpage == 0) {
				// ?Oo?t?B?b?No?(type=0)
				render.mixtype = 0;
				return;
			}
			if (render.mixpage == 1) {
				// ?Oo?t?B?b?N1oo(type=1)
				render.mixptr[0] = ptr[0];
				render.mixshift[0] = shift[0];
				render.mixx[0] = &render.grpx[ map[0] ];
				render.mixy[0] = &render.grpy[ map[0] ];
				render.mixand[0] = an[0];
				render.mixtype = 1;
				return;
			}
			if (render.mixpage == 2) {
				// ?Oo?t?B?b?N2oo(type=2)
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
			// ?Oo?t?B?b?N3ooo(type=4)
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
		// ?e?L?X?goo
		if (render.mixpage == 0) {
			// ?Oo?t?B?b?NooB?e?L?X?go?(type=1)
			render.mixptr[0] = render.textout;
			render.mixshift[0] = 10;
			render.mixx[0] = &render.textx;
			render.mixy[0] = &render.texty;
			render.mixand[0] = 1024 - 1;
			render.mixtype = 1;
				return;
		}
		if (render.mixpage == 1) {
			// ?e?L?X?g+?Oo?t?B?b?N1o
			if (tx < gr) {
				// ?e?L?X?g?Oo(type=3)
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
			// ?Oo?t?B?b?N?Oo(type=3,tx=groOo?t?B?b?N?OoAo?II)
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
		// ?e?L?X?g+?Oo?t?B?b?N2oo(type=5, type=6)
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

	// ?X?vo?C?goo
	if (!render.texten) {
		// ?e?L?X?go?
		if (render.mixpage == 0) {
			// ?Oo?t?B?b?NooA?X?vo?C?go?(type=1)
			render.mixptr[0] = render.bgspbuf;
			render.mixshift[0] = 9;
			render.mixx[0] = &render.zero;
			render.mixy[0] = &render.zero;
			render.mixand[0] = 512 - 1;
			render.mixtype = 1;
			return;
		}
		if (render.mixpage == 1) {
			// ?X?vo?C?g+?Oo?t?B?b?N1o(type=3)
			if (sp < gr) {
				// ?X?vo?C?g?Oo
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
			// ?Oo?t?B?b?N?Oo(sp=groso)
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
		// ?X?vo?C?g+?Oo?t?B?b?N2oo(type=5, type=6)
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

	// ?e?L?X?goo
	if (render.mixpage == 0) {
		// ?Oo?t?B?b?NooB?e?L?X?g?{?X?vo?C?g(type=3)
		if (tx <= sp) {
			// tx=spoe?L?X?g?Oo(LMZ2)
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
		// ?X?vo?C?g?Oo
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

	// ?Dooo?
	if (tx == 3) tx--;
	if (sp == 3) sp--;
	if (gr == 3) gr--;
	if (tx == sp) {
		// ?Kooooo?
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
		// ?Kooooo?
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
		// ?Kooooo?
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
		// ?e?L?X?g?{?X?vo?C?g?{?Oo?t?B?b?N1o(type=7)
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

	// ?e?L?X?g?{?X?vo?C?g?{?Oo?t?B?b?N?Qoo(type=8)
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
//	?Ro?go?X?go?
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetContrast(int cont)
{
	// ?V?X?eo?|?[?goo_o?v?`?F?b?Nosoo?Aoooooo?
	ASSERT(this);
	ASSERT((cont >= 0) && (cont <= 15));

	// oXoto?OON
	render.contlevel = cont;
	render.contrast = TRUE;
}

//---------------------------------------------------------------------------
//
//	?Ro?go?X?g?ï¿½Eï¿½ï¿½Eï¿½
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
//	?Ro?go?X?goo
//
//---------------------------------------------------------------------------
void FASTCALL Render::Contrast()
{
	int i;

	// ?|?Co?gouo?X?A?to?ODown
	render.palptr = render.palbuf;
	render.palptr += (render.contlevel << 16);
	render.contrast = FALSE;

	// ?po?b?g?to?OoSoUp
	for (i=0; i<0x200; i++) {
		render.palmod[i] = TRUE;
	}
	render.palette = TRUE;
}

//---------------------------------------------------------------------------
//
//	?po?b?go
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
//	?po?b?go?
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

	// ?SoR?s?[
	r = (DWORD)color;
	g = (DWORD)color;
	b = (DWORD)color;

	// MSBooG:5?AR:5?AB:5?AI:1oooooo
	// oo? R:8 G:8 B:8oDWORDoo?Bb31-b24ogoo
	r <<= 13;
	r &= 0xf80000;
	g &= 0x00f800;
	b <<= 2;
	b &= 0x0000f8;

	// ?P?x?r?b?go?Up(o?f?[?^o0oo?A!=0ooooo?)
	if (color & 1) {
		r |= 0x070000;
		g |= 0x000700;
		b |= 0x000007;
	}

	// ?Ro?go?X?goeoooo
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
		out |= REND_FAST_IBIT;
	}
	return out;
}

//---------------------------------------------------------------------------
//
//	?po?b?g?ï¿½Eï¿½ï¿½Eï¿½
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
//	PX68k CRTC host bridge
//
//---------------------------------------------------------------------------
const Px68kCrtcHost* FASTCALL Render::GetPx68kCrtcHost() const
{
	ASSERT(this);

	if (compositor_mode != compositor_fast) {
		return NULL;
	}

	return &px68k_crtc_host;
}

//---------------------------------------------------------------------------
//
//	PX68k CRTC state cache
//
//---------------------------------------------------------------------------
void FASTCALL Render::CachePx68kStateView(const Px68kCrtcStateView *view)
{
	ASSERT(this);
	if (!view) {
		return;
	}

	memcpy(&px68k_crtc_state_cache, view, sizeof(px68k_crtc_state_cache));
}

//---------------------------------------------------------------------------
//
//	?po?b?goo
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

	// ?to?OOFF
	tx = FALSE;
	gr = FALSE;
	sp = FALSE;

	// ?Oo?t?B?b?N
	for (i=0; i<0x100; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// ?Oo?t?B?b?Noeo?A?to?OOFF
			gr = TRUE;
			render.palmod[i] = FALSE;

			// oo?Foo?
			if (i == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// 65536?Fooopo?b?g?f?[?^o?
			j = i >> 1;
			if (i & 1) {
				j += 128;
			}
			render.pal64k[j * 2 + 0] = (BYTE)(data >> 8);
			render.pal64k[j * 2 + 1] = (BYTE)data;
		}
	}

	// ?e?L?X?go?X?vo?C?g
	for (i=0x100; i<0x110; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// ?e?L?X?goeo?A?to?OOFF
			tx = TRUE;
			render.palmod[i] = FALSE;

			// oo?Foo?
			if (i == 0x100) {
				render.paldata[i] |= REND_COLOR0;
				// 0x100oBG?E?X?vo?C?go?Ko?eo
				sp = TRUE;
			}

			// PCGoo
			memset(&render.pcgready[0], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[0] > 0) {
				sp = TRUE;
			}
		}
	}

	// ?X?vo?C?g
	for (i=0x110; i<0x200; i++) {
		if (render.palmod[i]) {
			// ?X?vo?C?goeo?A?to?OOFF
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];
			render.palmod[i] = FALSE;

			// oo?Foo?
			if ((i & 0x00f) == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// PCGoo
			memset(&render.pcgready[(i & 0xf0) << 4], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[(i & 0xf0) >> 4] > 0) {
				sp = TRUE;
			}
		}
	}

	// ?Oo?t?B?b?N?to?O
	if (gr) {
		// ?to?OON
		for (i=0; i<512*4; i++) {
			render.grppal[i] = TRUE;
		}
	}

	// ?e?L?X?g?to?O
	if (tx) {
		for (i=0; i<1024; i++) {
			render.textpal[i] = TRUE;
		}
	}

	// ?X?vo?C?g?to?O
	if (sp) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}

	// ?po?b?g?to?OOFF
	render.palette = FALSE;
}

void FASTCALL Render::TextScrl(DWORD x, DWORD y)
{
	int i;

	ASSERT(this);
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// or?`?F?b?N
	if ((render.textx == x) && (render.texty == y)) {
		return;
	}

	// o?[?N?X?V
	render.textx = x;
	render.texty = y;

	// ?to?OON
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

	// ?A?Zo?uo?T?u
	RendTextCopy(&render.texttv[src << 9],
				 &render.texttv[dst << 9],
				 plane,
				 &render.textflag[dst << 7],
				 &render.textmod[dst << 2]);
}

//---------------------------------------------------------------------------
//
//	?e?L?X?g?o?b?t?@?ï¿½Eï¿½ï¿½Eï¿½
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
//	?e?L?X?goo
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

	// ?f?B?Z?[?uooï¿½Eï¿½ï¿½Eï¿½ooo
	if (!render.texten) {
		return;
	}

	// oo?Y?Z?o
	y = (raster + render.texty) & 0x3ff;

	// oX?to?O(oo?^)
	if (render.textmod[y]) {
		// ?to?Ooo
		render.textmod[y] = FALSE;
		render.mix[raster] = TRUE;

		// ooooo?
		RendTextMem(render.texttv + (y << 7),
					render.textflag + (y << 5),
					render.textbuf + (y << 9));

		// oo?po?b?go?
		RendTextPal(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.textflag + (y << 5),
					render.paldata + 0x100);
	}

	// ?po?b?g(o?^)
	if (render.textpal[y]) {
		// ?to?Ooo
		render.textpal[y] = FALSE;

		// oo?po?b?go?
		RendTextAll(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.paldata + 0x100);
		render.mix[raster] = TRUE;

		// y == 1023o?R?s?[oo
		if (y == 1023) {
			memcpy(render.textout + (1024 << 10), render.textout + (1023 << 10), sizeof(DWORD) * 1024);
		}
	}
}

//---------------------------------------------------------------------------
//
//	?Oo?t?B?b?N?o?b?t?@?ï¿½Eï¿½ï¿½Eï¿½
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
//	?Oo?t?B?b?N?X?No?[o
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

	// or?`?F?b?N?Bo\oo?X?Vo?
	flag = FALSE;
	if ((render.grpx[block] != x) || (render.grpy[block] != y)) {
		render.grpx[block] = x;
		render.grpy[block] = y;
		flag = render.grpen[block];
	}

	// ?to?Ooo
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
//	?Oo?t?B?b?Noo
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
		// 1024o?[?hoy?[?W0oo
		if (!render.grpen[0]) {
			return;
		}
	}
	else {
		// oo?O
		if (!render.grpen[block]) {
			return;
		}
	}

	// ?^?C?vo
	switch (render.grptype) {
		// ?^?C?v0:1024?~1024 16Color
		case 0:
			// ?I?t?Z?b?g?Z?o
			offset = (raster + render.grpy[0]) & 0x3ff;
			y = offset & 0x1ff;

			// ?\oo?`?F?b?N
			if ((offset < 512) && (block >= 2)) {
				return;
			}
			if ((offset >= 512) && (block < 2)) {
				return;
			}

			// ?po?b?goo?Soo?
			if (render.grppal[y + (block << 9)]) {
				render.grppal[y + (block << 9)] = FALSE;
				render.grpmod[y + (block << 9)] = FALSE;
				for (i=0; i<32; i++) {
					render.grpflag[(y << 5) + (block << 14) + i] = FALSE;
				}
				switch (block) {
					// ooouo?b?N0o?E
					case 0:
						if (Rend1024A(render.grpgv + (y << 10),
									render.grpbuf[0] + (offset << 11),
									render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
					case 1:
						break;
					// oooouo?b?N2o?E
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

			// oo?Oogrpmodoooo
			if (!render.grpmod[y + (block << 9)]) {
				return;
			}
			render.grpmod[y + (block << 9)] = FALSE;
			render.mix[raster] = TRUE;
			switch (block) {
				// ?uo?b?N0-oo
				case 0:
					Rend1024C(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5),
								render.paldata);
					break;
				// ?uo?b?N1-?Eo
				case 1:
					Rend1024D(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0x4000,
								render.paldata);
					break;
				// ?uo?b?N2-oo
				case 2:
					Rend1024E(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0x8000,
								render.paldata);
					break;
				// ?uo?b?N3-?Eo
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
					// ?po?b?g
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
					// o?
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
					// ?po?b?g
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
					// o?
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
					// ?po?b?g
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
					// o?
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
					// ?po?b?g
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
					// o?
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

				// ?po?b?goo?Soo?
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

				// oo?Oogrpmodoooo
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

				// ?po?b?goo?Soo?
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

				// oo?Oogrpmodoooo
				if (!render.grpmod[0x400 + y]) {
					return;
				}

				render.grpmod[0x400 + y] = FALSE;
				render.mix[raster] = TRUE;

				// oo?_oo?O
				Rend256B(render.grpgv + (y << 10),
							render.grpbuf[2] + (y << 10),
							render.grpflag + 0x8000 + (y << 5),
							render.paldata);
			}
			return;

		// ?^?C?v3:512x512 oo`
		case 3:
		// ?^?C?v4:512x512 65536Color

		case 4:

			ASSERT(block == 0);
			// ?I?t?Z?b?g?Z?o
			y = (raster + render.grpy[0]) & 0x1ff;

			// ?po?b?goo?Soo?
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

			// oo?Oogrpmodoooo
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
//	oo?_o(BG?E?X?vo?C?go)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	?X?vo?C?go?W?X?^o?Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::SpriteReset()
{
	DWORD addr;
	WORD data;

	// ?X?vo?C?go?W?X?^o?
	for (addr=0; addr<0x400; addr+=2) {
		data = *(WORD*)(&render.sprmem[addr]);
		SpriteReg(addr, data);
	}
}

//---------------------------------------------------------------------------
//
//	?X?vo?C?go?W?X?^oX
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

	// ?Co?f?N?Vo?Oof?[?^oo
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

	// ptro?(&spptr[index << 9])
	ptr = &render.spptr[index << 9];

	// o?W?X?^oo?b?N?A?b?v
	next = &render.spreg[index << 2];
	reg[0] = next[0];
	reg[1] = next[1];
	reg[2] = next[2];
	reg[3] = next[3];

	// o?W?X?^oooo?
	render.spreg[addr >> 1] = data;

	// ooLooo?`?F?b?N
	use = TRUE;
	if (next[1] == 0) {
		use = FALSE;
	}
	if (next[1] >= (512 + 16)) {
		use = FALSE;
	}
	if (next[3] == 0) {
		use = FALSE;
	}

	// ooooo?Aooooooï¿½Eï¿½ï¿½Eï¿½ooo
	if (!render.spuse[index]) {
		if (!use) {
			return;
		}
	}

	// oooLoooAoxoo
	if (render.spuse[index]) {
		// oooo(PCG)
		pcgno = reg[2] & 0xfff;
		ASSERT(render.pcguse[ pcgno ] > 0);
		render.pcguse[ pcgno ]--;
		pcgno >>= 8;
		ASSERT(render.pcgpal[ pcgno ] > 0);
		render.pcgpal[ pcgno ]--;

		// oooo(?|?Co?^)
		for (i=0; i<16; i++) {
			j = (int)(reg[1] - 16 + i);
			if ((j >= 0) && (j < 512)) {
				ptr[j] = NULL;
				render.bgspmod[j] = TRUE;
			}
		}

		// oooo?AoooIo
		if (!use) {
			render.spuse[index] = FALSE;
			return;
		}
	}

	// ?o?^oo(?g?p?to?O)
	render.spuse[index] = TRUE;

	// ?o?^oo(PCG)
	pcgno = next[2] & 0xfff;
	render.pcguse[ pcgno ]++;
	offset = pcgno << 8;
	pcgno >>= 8;
	render.pcgpal[ pcgno ]++;

	// PCG?A?ho?Xov?Z?A?|?Co?^?Z?b?g
	if (next[2] & 0x8000) {
		// Vo?]
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
		// ?m?[?}o
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
//	BG?X?No?[ooX
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGScrl(int page, DWORD x, DWORD y)
{
	BOOL flag;
	int i;

	ASSERT((page == 0) || (page == 1));
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// or?Aovoooooo?
	if ((render.bgx[page] == x) && (render.bgy[page] == y)) {
		return;
	}

	// ?X?V
	render.bgx[page] = x;
	render.bgy[page] = y;

	// 768?~512oï¿½Eï¿½po?
	if (!render.bgspflag) {
		return;
	}

	// ?\ooo?ABGSPMODoï¿½Eï¿½Oo
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
//	BG?Ro?go?[ooX
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

	// ?to?OOFF
	areaflag[0] = FALSE;
	areaflag[1] = FALSE;

	// ?^?C?vo
	switch (index) {
		// BG0 ?\o?to?O
		case 0:
			if (render.bgdisp[0] == flag) {
				return;
			}
			render.bgdisp[0] = flag;
			break;

		// BG1 ?\o?to?O
		case 1:
			if (render.bgdisp[1] == flag) {
				return;
			}
			render.bgdisp[1] = flag;
			break;

		// BG0 ?Go?AoX
		case 2:
			if (render.bgarea[0] == flag) {
				return;
			}
			render.bgarea[0] = flag;
			areaflag[0] = TRUE;
			break;

		// BG1 ?Go?AoX
		case 3:
			if (render.bgarea[1] == flag) {
				return;
			}
			render.bgarea[1] = flag;
			areaflag[1] = TRUE;
			break;

		// BG?T?C?YoX
		case 4:

			if (render.bgsize == flag) {
				return;
			}
			render.bgsize = flag;
			areaflag[0] = TRUE;
			areaflag[1] = TRUE;
			break;

		// oo?(ooo?)
		default:
			ASSERT(FALSE);
			return;
	}

	// ?to?Ooo
	for (i=0; i<2; i++) {
		if (areaflag[i]) {
			// oo?gooo?render.pcguseoJ?b?g
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

			// ?f?[?^?A?ho?XoZ?o($EBE000,$EBC000)
			area = (WORD*)render.sprmem;
			area += 0x6000;
			if (render.bgarea[i]) {
				area += 0x1000;
			}

			// 64?~64o?[?h?R?s?[?B$10000or?b?goo0
			if (render.bgsize) {
				// 16x16ooo?
				for (j=0; j<(64*64); j++) {
					render.bgreg[i][j] = (DWORD)area[j];
				}
			}
			else {
				// 8x8oH?vo?K?v?BPCG(0-255)o>>2o?Aooobit0,1obit17,18o
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

			// bgalloZ?b?g
			for (j=0; j<64; j++) {
				render.bgall[i][j] = TRUE;
			}
		}
	}

	// ooXo?A768?~512oOo?bgspmodoï¿½Eï¿½Oo
	if (render.bgspflag) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	BGooooX
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

	// ?y?[?Wo?[?v
	for (i=0; i<2; i++) {
		// ?Yo?y?[?Wof?[?^?Go?Ao?vooo?
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

		// ?Co?f?b?N?X(<64x64)?Ao?W?X?^?|?Co?^?ï¿½Eï¿½ï¿½Eï¿½
		index = (int)(addr & 0x1fff);
		index >>= 1;
		ASSERT((index >= 0) && (index < 64*64));
		pcgno = render.bgreg[i][index];

		// oOopcguseoo?
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
			// 16x16ooo?
			render.bgreg[i][index] = (DWORD)data;
		}
		else {
			// 8x8oH?vo?K?v?BPCG(0-255)o>>2o?Aooobit0,1obit17,18o
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

		// bgalloï¿½Eï¿½Oo
		render.bgall[i][index >> 6] = TRUE;

		// ?\ooooo?Io?Bbgsize=1oy?[?W1oo?Io
		if (!render.bgspflag || !render.bgdisp[i]) {
			continue;
		}
		if (render.bgsize && (i == 1)) {
			continue;
		}

		// ?X?No?[oouoov?Zo?Abgspmodoï¿½Eï¿½Oo
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
//	PCGooooX
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

	// ?Co?f?b?N?Xooo
	addr &= 0x7fff;
	index = (int)(addr >> 7);
	ASSERT((index >= 0) && (index < 256));

	// render.pcgreadyoo?
	for (i=0; i<16; i++) {
		render.pcgready[index + (i << 8)] = FALSE;
	}

	// render.pcguseo>0o?
	count = 0;
	for (i=0; i<16; i++) {
		count += render.pcguse[index + (i << 8)];
	}
	if (count > 0) {
		// ?dooooABG/?X?vo?C?gooooo
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}
}

void FASTCALL Render::SpriteBGWrite(DWORD addr, BYTE data)
{
	if (px68k_adapter) {
		px68k_adapter->BGWrite(addr, data);
	}
}

BYTE FASTCALL Render::TVRAMRead(DWORD addr)
{
	if (px68k_adapter) {
		return px68k_adapter->TVRAMRead(addr);
	}
	return 0xff;
}

void FASTCALL Render::TVRAMWrite(DWORD addr, BYTE data)
{
	if (px68k_adapter) {
		px68k_adapter->TVRAMWrite(addr, data);
	}
}

BYTE FASTCALL Render::GVRAMRead(DWORD addr)
{
	if (px68k_adapter) {
		return px68k_adapter->GVRAMRead(addr);
	}
	return 0xff;
}

void FASTCALL Render::GVRAMWrite(DWORD addr, BYTE data)
{
	if (px68k_adapter) {
		px68k_adapter->GVRAMWrite(addr, data);
	}
}

BYTE FASTCALL Render::BGRead(DWORD addr)
{
	if (px68k_adapter) {
		return px68k_adapter->BGRead(addr);
	}
	return 0xff;
}

void FASTCALL Render::GVRAMFastClear()
{
	if (px68k_adapter) {
		px68k_adapter->GVRAMFastClear();
	}
}

void FASTCALL Render::CRTCRegWrite(DWORD addr, BYTE data)
{
	if (px68k_adapter) {
		px68k_adapter->CRTCRegWrite(addr, data);
	}
}

BYTE FASTCALL Render::CRTCRegRead(DWORD addr)
{
	if (px68k_adapter) {
		return px68k_adapter->CRTCRegRead(addr);
	}
	return 0xff;
}

BYTE FASTCALL Render::VCtrlRead(DWORD addr)
{
	if (px68k_adapter) {
		return px68k_adapter->VCtrlRead(addr);
	}
	return 0xff;
}

void FASTCALL Render::VCtrlWrite(DWORD addr, BYTE data)
{
	if (px68k_adapter) {
		px68k_adapter->VCtrlWrite(addr, data);
	}
}

//---------------------------------------------------------------------------
//
//	PCG?o?b?t?@?ï¿½Eï¿½ï¿½Eï¿½
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
//	BG/?X?vo?C?g?o?b?t?@?ï¿½Eï¿½ï¿½Eï¿½
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
//	BG/?X?vo?C?g
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
	const int bg_hadjust = CalcBGHAdjustPixels(0, crtc, sprite);
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

	const int bg_hadjust = CalcBGHAdjustPixels(0, crtc, sprite);

	// y?uo?b?Noo?oo
	y = render.bgy[page] + raster;
	if (render.bgsize) {
		// 16x16o?[?h
		y &= (1024 - 1);
		y >>= 4;
	}
	else {
		// 8x8o?[?h
		y &= (512 - 1);
		y >>= 3;
	}
	ASSERT((y >= 0) && (y < 64));

	// bgalloTRUEo?Aooy?uo?b?No?X?f?[?^oo
	if (render.bgall[page][y]) {
		render.bgall[page][y] = FALSE;
		BGBlock(page, y);
	}

	// ?\o
	ptr = render.bgptr[page];
	if (!render.bgsize) {
		// 8x8o\o
		x = (render.bgx[page] - bg_hadjust) & (512 - 1);
		ptr += (((render.bgy[page] + raster) & (512 - 1)) << 7);

		// oooo`?F?b?N
		if ((x & 7) == 0) {
			// 8x8?Aooo?
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

		// ooo?[?uo?b?Noos
		rest = 8 - (x & 7);
		ASSERT((rest > 0) && (rest < 8));
		RendBG8P(&ptr[(x & 0xfff8) >> 2], buf, (x & 7), rest, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);

		// ?]ooo8dot?Poooo
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

		// o?
		if (len & 7) {
			x += (len & 0xfff8);
			x &= (512 - 1);
			RendBG8P(&ptr[x >> 2], &buf[rest + (len & 0xfff8)], 0, (len & 7),
				render.pcgready, render.sprmem, render.pcgbuf, render.paldata,
				legacy_bg_transparency);
		}
		return;
	}

	// 16x16o\o
	x = (render.bgx[page] - bg_hadjust) & (1024 - 1);
	ptr += (((render.bgy[page] + raster) & (1024 - 1)) << 7);

	// oooo`?F?b?N
	if ((x & 15) == 0) {
		// 16x16?Aooo?
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

	// ooo?[?uo?b?Noos
	rest = 16 - (x & 15);
	ASSERT((rest > 0) && (rest < 16));
	RendBG16P(&ptr[(x & 0xfff0) >> 3], buf, (x & 15), rest, render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);

	// ?]ooo16dot?Poooo
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

	// o?
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
//	BG(?uo?b?Noo)
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

	// o?W?X?^?|?Co?^oo
	reg = &render.bgreg[page][y << 6];

	// BG?|?Co?^oo
	ptr = render.bgptr[page];
	if (render.bgsize) {
		ptr += (y << 11);
	}
	else {
		ptr += (y << 10);
	}

	// o?[?v
	for (i=0; i<64; i++) {
		// ?ï¿½Eï¿½ï¿½Eï¿½
		bgdata = reg[i];

		// $10000ooooooOK
		if (bgdata & 0x10000) {
			ptr += 2;
			continue;
		}

		// $10000oOR
		reg[i] |= 0x10000;

		// pcgnooo
		pcgno = bgdata & 0xfff;

		// ?T?C?Yo
		if (render.bgsize) {
			// 16x16
			pcgbuf = &render.pcgbuf[ (pcgno << 8) ];
			if (bgdata & 0x8000) {
				// oo?]
				pcgbuf += 0xf0;
				for (j=0; j<16; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = reinterpret_cast<DWORD*>(static_cast<size_t>(bgdata));
					pcgbuf -= 0x10;
					ptr += 128;
				}
			}
			else {
				// o?
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
			// 8x8?Bbit17,bit18olooo
			pcgbuf = &render.pcgbuf[ (pcgno << 8) ];
			if (bgdata & 0x20000) {
				pcgbuf += 0x80;
			}
			if (bgdata & 0x40000) {
				pcgbuf += 8;
			}

			if (bgdata & 0x8000) {
				// oo?]
				pcgbuf += 0x70;
				for (j=0; j<8; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = reinterpret_cast<DWORD*>(static_cast<size_t>(bgdata));
					pcgbuf -= 0x10;
					ptr += 128;
				}
			}
			else {
				// o?
				for (j=0; j<8; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = reinterpret_cast<DWORD*>(static_cast<size_t>(bgdata));
					pcgbuf += 0x10;
					ptr += 128;
				}
			}
			ptr -= 1024;
		}

		// ?o?^oo(PCG)
		render.pcguse[ pcgno ]++;
		pcgno = (pcgno >> 8) & 0x0f;
		render.pcgpal[ pcgno ]++;

		// ?|?Co?^oio?
		ptr += 2;
	}
}

//===========================================================================
//
//	oo?_o(ooo)
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

	// oo?wooooo?Aoo?o?b?t?@oooo?Ay?I?[?o?[o?return
	if ((!render.mix[y]) || (!render.mixbuf)) {
		return;
	}
	if (render.mixheight <= y) {
		return;
	}
	ASSERT(render.mixlen > 0);


#if defined(REND_LOG)
	LOG1(Log::Normal, "???? y=%d", y);
#endif	// REND_LOG

	// ?to?OOFF?Aoo?o?b?t?@?A?ho?Xooo
	render.mix[y] = FALSE;
	q = &render.mixbuf[render.mixwidth * y];

	switch (render.mixtype) {
		// ?^?C?v0(?\ooo?)
		case 0:
			RendMix00(q, render.drawflag + (y << 6), render.mixlen);
			return;

		// ?^?C?v1(1oo)
		case 1:
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			ASSERT(p);
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			RendMix01(q, p, render.drawflag + (y << 6), render.mixlen);
			return;

		// ?^?C?v2(2oA?Jo?[0?doo?)
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

		// ?^?C?v3(2oAo?doo?)
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

		// ?^?C?v4(?Oo?t?B?b?No?3o or 4o)
		case 4:

			MixGrp(y, buf);
			RendMix01(q, buf, render.drawflag + (y << 6), render.mixlen);
			break;

		// ?^?C?v5(?Oo?t?B?b?N?{?e?L?X?g?A?e?L?X?g?Doodoo?)
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

		// ?^?C?v6(?Oo?t?B?b?N?{?e?L?X?g?A?Oo?t?B?b?N?Doodoo?)
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

		// ?^?C?v7(?e?L?X?g?{?X?vo?C?g?{?Oo?t?B?b?N1o)
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

		// ?^?C?v8(?e?L?X?g+?X?vo?C?g+?Oo?t?B?b?N?Qoo)
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

		// oo?
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	?Oo?t?B?b?Noo
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
		// o?(oorender.mixpageozo)
		case 0:
			ASSERT(FALSE);
			return;

		// 1o(oorender.mixpageozo)
		case 1:
			ASSERT(FALSE);
			return;

		// 2o
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

		// 3o
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

		// 4o
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

		// oo?
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	oo?o?b?t?@?ï¿½Eï¿½ï¿½Eï¿½
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetMixBuf() const
{
	ASSERT(this);

	// NULLoooo
	return render.mixbuf;
}

//---------------------------------------------------------------------------
//
//	oo?_oo?O
//
//---------------------------------------------------------------------------
void FASTCALL Render::Process()
{
	if (backend) {
		backend->Process(this);
		return;
	}

	int i;

	// ?sooooooos?v
	if (render.first >= render.last) {
		return;
	}

	// VC
	if (render.vc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?r?f?I????");
#endif	// RENDER_LOG
		ComposeVideo();
	}

	// ?Ro?go?X?g
	if (render.contrast) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?R???g???X?g????");
#endif	// RENDER_LOG
		Contrast();
	}

	// ?po?b?g
	if (render.palette) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?p???b?g????");
#endif	// RENDER_LOG
		Palette();
	}

	// first==0oA?X?vo?C?go\oON/OFFoo?
	if (render.first == 0) {
		if (sprite->IsDisplay()) {
			if (!render.bgspdisp) {
				// ?X?vo?C?gCPUoVideo
				for (i=0; i<512; i++) {
					render.bgspmod[i] = TRUE;
				}
				render.bgspdisp = TRUE;
			}
		}
		else {
			if (render.bgspdisp) {
				// ?X?vo?C?gVideooCPU
				for (i=0; i<512; i++) {
					render.bgspmod[i] = TRUE;
				}
				render.bgspdisp = FALSE;
			}
		}
	}

	// oox2o?
	if ((render.v_mul == 2) && !render.lowres) {
		// I/Ooog?ï¿½Eï¿½ï¿½Eï¿½ooA?cooooooooo
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

	// ?Co?^o?[?Xo?
	if ((render.v_mul == 0) && render.lowres) {
		// ooEooooo(o?@ooo)
		for (i=render.first; i<render.last; i++) {
			// ?e?L?X?g?E?Oo?t?B?b?N
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

	// oo[?v
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

//===========================================================================
//
//	PX68k Video Engine Fast Pipeline Methods
//
//===========================================================================

void FASTCALL Render::StartFrameFast()
{
	CRTC::crtc_t crtcdata;
	BOOL geometry_synced = FALSE;

	ASSERT(this);

	// Skip if not ready
	if ((render.count != 0) || !render.enable) {
		render.act = FALSE;
		return;
	}

	// Mark frame as active
	render.act = TRUE;
	render.first = 0;
	render.last = -1;

	// Keep geometry in sync with active CRTC mode.
	if (crtc) {
		crtc->GetCRTC(&crtcdata);
		if ((crtcdata.h_dots > 0) && (crtcdata.v_dots > 0)) {
			render.width = crtcdata.h_dots;
			render.h_mul = crtcdata.h_mul;
			render.height = crtcdata.v_dots;
			render.v_mul = crtcdata.v_mul;
			render.lowres = crtcdata.lowres;
			if ((render.v_mul == 2) && !render.lowres) {
				render.height >>= 1;
			}
			render.mixlen = render.width;
			if (render.mixwidth < render.width) {
				render.mixlen = render.mixwidth;
			}
			render.crtc = FALSE;
			geometry_synced = TRUE;
		}
	}

	// Don't stall frame production on transient CRTC geometry glitches.
	if (!geometry_synced) {
		if ((render.width <= 0) || (render.height <= 0)) {
			render.act = FALSE;
			fast_fallback_count++;
			return;
		}
		render.mixlen = render.width;
		if (render.mixwidth < render.width) {
			render.mixlen = render.mixwidth;
		}
	}

	// Initialize PX68k adapter if available
	if (px68k_adapter) {
		px68k_adapter->StartFrame(this);
	}

	BeginVideoSnapshotFrame();
}

void FASTCALL Render::EndFrameFast()
{
	int copy_w;
	int copy_h;
	int y;

	ASSERT(this);

	if (!render.act) {
		return;
	}

	// Process remaining scanlines
	if (render.last > 0) {
		render.last = render.height;
		ProcessFast();
	}

	// End frame in adapter
	if (px68k_adapter) {
		px68k_adapter->EndFrame(this);
	}

	// Copy PX68k 565I output into the canonical 32-bit mix buffer expected
	// by xm6_video_poll/libretro.
	if (px68k_adapter && render.mixbuf && (render.mixwidth > 0) && (render.mixheight > 0)) {
		const WORD *src = px68k_adapter->GetScreenBuffer();
		const int src_w = (int)px68k_adapter->GetScreenWidth();
		const int src_h = (int)px68k_adapter->GetScreenHeight();

		if (src && (src_w > 0) && (src_h > 0)) {
			render.width = src_w;
			render.height = src_h;
			render.mixlen = render.width;
			if (render.mixwidth < render.width) {
				render.mixlen = render.mixwidth;
			}

			copy_w = src_w;
			if (copy_w > render.mixwidth) {
				copy_w = render.mixwidth;
			}
			copy_h = src_h;
			if (copy_h > render.mixheight) {
				copy_h = render.mixheight;
			}

			for (y = 0; y < copy_h; y++) {
				const WORD *src_row = &src[y * PX68K_FULLSCREEN_WIDTH];
				DWORD *dst_row = &render.mixbuf[y * render.mixwidth];
				int x;
				for (x = 0; x < copy_w; x++) {
					dst_row[x] = ConvFast565IToXrgb8888(src_row[x]);
				}
				if (copy_w < render.mixwidth) {
					memset(&dst_row[copy_w], 0, (size_t)(render.mixwidth - copy_w) * sizeof(DWORD));
				}
				if (y < 1024) {
					render.mix[y] = FALSE;
				}
			}
			for (; y < render.mixheight; y++) {
				memset(&render.mixbuf[y * render.mixwidth], 0, (size_t)render.mixwidth * sizeof(DWORD));
				if (y < 1024) {
					render.mix[y] = FALSE;
				}
			}
		}
		else {
			fast_fallback_count++;
		}
	}

	render.count++;
	EndVideoSnapshotFrame();
	render.act = FALSE;
}

void FASTCALL Render::HSyncFast(int raster)
{
	ASSERT(this);

	render.last = raster;

	if (!render.act) {
		return;
	}

	// Process through adapter
	if (px68k_adapter) {
		px68k_adapter->HSync(this, raster);
	}
}

void FASTCALL Render::SetCRTCFast()
{
	ASSERT(this);

	render.crtc = TRUE;
	render.vc = TRUE;

	if (px68k_adapter) {
		px68k_adapter->SetCRTC(this);
	}
}

void FASTCALL Render::SetVCFast()
{
	ASSERT(this);

	render.vc = TRUE;

	if (px68k_adapter) {
		px68k_adapter->SetVC(this);
	}
}

void FASTCALL Render::VideoFastPX68K()
{
	ASSERT(this);

	// Clear VC dirty flag
	render.vc = FALSE;

	// Mark all lines dirty
	for (int i = 0; i < 1024; i++) {
		render.mix[i] = TRUE;
	}

	// Sync state through adapter
	if (px68k_adapter) {
		px68k_adapter->SetVC(this);
	}
}

void FASTCALL Render::PaletteFastPX68K()
{
	ASSERT(this);

	if (px68k_adapter) {
		px68k_adapter->SetVC(this);
	}
	render.palette = FALSE;
}

void FASTCALL Render::TextFastPX68K(int raster)
{
	ASSERT(this);
	ASSERT((raster >= 0) && (raster < 1024));

	// Text rendering is handled by the adapter during HSync
	(void)raster;
}

void FASTCALL Render::ProcessFast()
{
	ASSERT(this);

	if (render.first >= render.last) {
		return;
	}

	// Process through adapter
	if (px68k_adapter) {
		px68k_adapter->Process(this);
	}

	render.first = render.last;
}

void FASTCALL Render::MixFast(int y)
{
	ASSERT(this);
	ASSERT((y >= 0) && (y < render.mixheight));

	// Mixing is handled by the adapter
	(void)y;
}

void FASTCALL Render::MixFastLine(int dst_y, int src_y)
{
	ASSERT(this);

	// Line mixing is handled by the adapter
	(void)dst_y;
	(void)src_y;
}

void FASTCALL Render::FastMixGrp(int y, DWORD *grp, DWORD *grp_sp, DWORD *grp_sp2,
	BOOL *grp_sp_tr, BOOL *gon, BOOL *tron, BOOL *pron)
{
	ASSERT(this);

	// Fast graphic mixing - handled by adapter
	(void)y;
	(void)grp;
	(void)grp_sp;
	(void)grp_sp2;
	(void)grp_sp_tr;
	(void)gon;
	(void)tron;
	(void)pron;
}







//===========================================================================
//
//	Fast Vertical Probe Snapshot
//
//===========================================================================

void FASTCALL Render::GetFastVerticalProbeSnapshot(fast_vertical_probe_snapshot_t *out) const
{
	if (!out) {
		return;
	}

	// Return a zeroed/empty snapshot as a stub
	// The full implementation would capture vertical timing state
	memset(out, 0, sizeof(fast_vertical_probe_snapshot_t));
	out->valid = FALSE;
	out->width = render.width;
	out->height = render.height;
	out->mixwidth = render.mixwidth;
	out->mixheight = render.mixheight;
	out->mixpage = render.mixpage;
	out->mixtype = render.mixtype;
	out->lowres = render.lowres ? TRUE : FALSE;
	out->bgspflag = render.bgspflag ? TRUE : FALSE;
	out->bgspdisp = render.bgspdisp ? TRUE : FALSE;
	out->sample_count = 0;
}
