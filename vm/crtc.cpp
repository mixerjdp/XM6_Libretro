//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI (ytanaka@ipc-tokai.or.jp)
//	[ CRTC(VICON) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "tvram.h"
#include "mfp.h"
#include "sprite.h"
#include "render.h"
#include "schedule.h"
#include "cpu.h"
#include "gvram.h"
#include "printer.h"
#include "fileio.h"
#include "crtc.h"
#include "px68k_crtc_port.h"
#include "config.h"

//===========================================================================
//
//	CRTC
//
//===========================================================================
//#define CRTC_LOG

namespace {
BOOL g_alt_raster_timing = FALSE;
const WORD kFastClearMask[16] = {
	0xffff, 0xfff0, 0xff0f, 0xff00,
	0xf0ff, 0xf0f0, 0xf00f, 0xf000,
	0x0fff, 0x0ff0, 0x0f0f, 0x0f00,
	0x00ff, 0x00f0, 0x000f, 0x0000
};
}

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CRTC::CRTC(VM *p) : MemDevice(p)
{
	// Initialize device ID
	dev.id = MAKEID('C', 'R', 'T', 'C');
	dev.desc = "CRTC (VICON)";

	// Start address, end address
	memdev.first = 0xe80000;
	memdev.last = 0xe81fff;

	// Other members
	tvram = NULL;
	gvram = NULL;
	sprite = NULL;
	mfp = NULL;
	render = NULL;
	printer = NULL;
	memset(&px68k_state_view, 0, sizeof(px68k_state_view));
	px68k_fastclrline = 0;
	px68k_fastclrmask = 0;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Init()
{
	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Get text VRAM
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);

	// Get graphic VRAM
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);

	// Get sprite controller
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);

	// Get MFP
	mfp = (MFP*)vm->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(mfp);

	// Get renderer
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// Get printer
	printer = (Printer*)vm->SearchDevice(MAKEID('P', 'R', 'N', ' '));
	ASSERT(printer);

	// Event setup
	event.SetDevice(this);
	event.SetDesc("H-Sync");
	event.SetTime(0);
	scheduler->AddEvent(&event);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::Cleanup()
{
	ASSERT(this);

	// To base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::Reset()
{
	int i;

	ASSERT(this);
	LOG0(Log::Normal, "Reset");

	// Clear registers
	memset(crtc.reg, 0, sizeof(crtc.reg));
	for (i=0; i<18; i++) {
		crtc.reg[i] = ResetTable[i];
	}
	for (i=0; i<8; i++) {
		crtc.reg[i + 0x28] = ResetTable[i + 18];
	}

	// Resolution
	crtc.hrl = FALSE;
	crtc.lowres = FALSE;
	crtc.textres = TRUE;
	crtc.changed = FALSE;

	// Raster
	crtc.raster_count = 0;
	crtc.raster_int = 0;
	crtc.raster_copy = FALSE;
	crtc.raster_exec = FALSE;
	crtc.fast_clr = 0;
	px68k_fastclrline = 0;
	px68k_fastclrmask = 0;
	px68k_crtc_mode = 0;

	// Horizontal
	crtc.h_sync = 31745;
	crtc.h_pulse = 3450;
	crtc.h_back = 4140;
	crtc.h_front = 2070;
	crtc.h_dots = 768;
	crtc.h_mul = 1;
	crtc.hd = 2;

	// Vertical
	crtc.v_sync = 568;
	crtc.v_pulse = 6;
	crtc.v_back = 35;
	crtc.v_front = 15;
	crtc.v_dots = 512;
	crtc.v_mul = 1;
	crtc.vd = 1;

	// Events
	crtc.ns = 0;
	crtc.hus = 0;
	crtc.v_synccnt = 1;
	crtc.v_blankcnt = 1;
	crtc.h_disp = TRUE;
	crtc.v_disp = TRUE;
	crtc.v_blank = TRUE;
	crtc.v_count = 0;
	crtc.v_scan = 0;

	// Non-display variables
	crtc.h_disptime = 0;
	crtc.h_synctime = 0;
	crtc.v_cycletime = 0;
	crtc.v_blanktime = 0;
	crtc.v_backtime = 0;
	crtc.v_synctime = 0;

	// Memory mode
	crtc.tmem = FALSE;
	crtc.gmem = TRUE;
	crtc.siz = 0;
	crtc.col = 3;

	// Scroll
	crtc.text_scrlx = 0;
	crtc.text_scrly = 0;
	for (i=0; i<4; i++) {
		crtc.grp_scrlx[i] = 0;
		crtc.grp_scrly[i] = 0;
	}

	// H-Sync event setup (31.5us)
	event.SetTime(63);
	NotifyTextScrollChanged(crtc.text_scrlx, crtc.text_scrly);
	NotifyGrpScrollChanged(0, crtc.grp_scrlx[0], crtc.grp_scrly[0]);
	NotifyGrpScrollChanged(1, crtc.grp_scrlx[1], crtc.grp_scrly[1]);
	NotifyGrpScrollChanged(2, crtc.grp_scrlx[2], crtc.grp_scrly[2]);
	NotifyGrpScrollChanged(3, crtc.grp_scrlx[3], crtc.grp_scrly[3]);
	SyncPx68kState();
}

//---------------------------------------------------------------------------
//
//	CRTC reset data
//
//---------------------------------------------------------------------------
const BYTE CRTC::ResetTable[] = {
	0x00, 0x89, 0x00, 0x0e, 0x00, 0x1c, 0x00, 0x7c,
	0x02, 0x37, 0x00, 0x05, 0x00, 0x28, 0x02, 0x28,
	0x00, 0x1b,
	0x0b, 0x16, 0x00, 0x33, 0x7a, 0x7b, 0x00, 0x00
};

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Save(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "Save");

	// Save size
	sz = sizeof(crtc_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save entity
	if (!fio->Write(&crtc, (int)sz)) {
		return FALSE;
	}

	// Save event
	if (!event.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Load(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "Load");

	// Load size
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(crtc_t)) {
		return FALSE;
	}

	// Load entity
	if (!fio->Read(&crtc, (int)sz)) {
		return FALSE;
	}

	// Load event
	if (!event.Load(fio, ver)) {
		return FALSE;
	}

	// Notify renderer
	NotifyTextScrollChanged(crtc.text_scrlx, crtc.text_scrly);
	NotifyGrpScrollChanged(0, crtc.grp_scrlx[0], crtc.grp_scrly[0]);
	NotifyGrpScrollChanged(1, crtc.grp_scrlx[1], crtc.grp_scrly[1]);
	NotifyGrpScrollChanged(2, crtc.grp_scrlx[2], crtc.grp_scrly[2]);
	NotifyGrpScrollChanged(3, crtc.grp_scrlx[3], crtc.grp_scrly[3]);
	if (UsePx68kRasterTiming() && (crtc.fast_clr != 0)) {
		BeginPx68kFastClear();
	}
	else {
		px68k_fastclrline = 0;
		px68k_fastclrmask = 0;
	}
	px68k_crtc_mode = 0;
	NotifyScreenChanged();
	NotifyPx68kStateView();
	SyncPx68kState();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	g_alt_raster_timing = config->alt_raster;
	LOG0(Log::Normal, "Apply configuration");
}

BOOL FASTCALL CRTC::IsPx68kVideoEngine() const
{
	return (render && (render->GetCompositorMode() == Render::compositor_fast));
}

BOOL FASTCALL CRTC::UseAlternateRasterTiming() const
{
	return (!IsPx68kVideoEngine() && g_alt_raster_timing);
}

BOOL FASTCALL CRTC::UsePx68kRasterTiming() const
{
	return IsPx68kVideoEngine();
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL CRTC::ReadByte(DWORD addr)
{
	BYTE data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Loop at $800 boundary
	addr &= 0x7ff;

	// Wait
	scheduler->Wait(1);

	// $E80000-$E803FF : Register area
	if (addr < 0x400) {
		if (UsePx68kRasterTiming() && render) {
			return render->CRTCRegRead(memdev.first + addr);
		}
		addr &= 0x3f;
		if (addr >= 0x30) {
			return 0xff;
		}

		// Only R20, R21 can be read. Others return $00
		if ((addr < 40) || (addr > 43)) {
			return 0;
		}

		// Read (invert odd/even)
		addr ^= 1;
		return crtc.reg[addr];
	}

	// $E80480-$E804FF : Internal port
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		if (UsePx68kRasterTiming() && render) {
			return render->CRTCRegRead(memdev.first + addr);
		}
		// Even bytes are 0
		if ((addr & 1) == 0) {
			return 0;
		}

		if (UsePx68kRasterTiming()) {
			data = px68k_crtc_mode;
			if (crtc.fast_clr) {
				data |= 0x02;
			}
			else {
				data &= 0xfd;
			}
			return data;
		}

		// Odd byte is raster copy, graphic clear reset
		data = 0;
		if (crtc.raster_copy) {
			data |= 0x08;
		}
		if (crtc.fast_clr == 2) {
			data |= 0x02;
		}
		return data;
	}

	LOG1(Log::Warning, "Illegal address read $%06X", memdev.first + addr);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::WriteByte(DWORD addr, DWORD data)
{
	int reg;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Loop at $800 boundary
	addr &= 0x7ff;

	// Wait
	scheduler->Wait(1);

	// $E80000-$E803FF : Register area
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return;
		}

		// Write (invert odd/even)
		addr ^= 1;
		if (crtc.reg[addr] == data) {
			return;
		}
		crtc.reg[addr] = (BYTE)data;
		NotifyMarkAllTextDirty();
		if (render) {
			render->CRTCRegWrite(0xE80000 + addr, (BYTE)data);
		}

	// GVRAM address display
	if (addr == 0x29) {
		if (render && (render->GetCompositorMode() == Render::compositor_original)) {
			musashi_flush_timeslice();
			}
			if (data & 0x10) {
				crtc.tmem = TRUE;
			}
			else {
				crtc.tmem = FALSE;
			}
			if (data & 0x08) {
				crtc.gmem = TRUE;
			}
			else {
				crtc.gmem = FALSE;
			}
			crtc.siz = (data & 4) >> 2;
			crtc.col = (data & 3);

			// Notify graphic VRAM
			gvram->SetType(data & 0x0f);
			NotifyModeChanged((BYTE)(crtc.reg[0x29] & 0x1f));
			NotifyGeometryChanged();
			NotifyTimingChanged();
			SyncPx68kState();
			return;
		}

		// Resolution change
		if ((addr <= 15) || (addr == 40)) {
			if (render && (render->GetCompositorMode() == Render::compositor_original)) {
				musashi_flush_timeslice();
			}
			// Sprite connection/disconnection is done immediately (OS-9/68000)
			if (addr == 0x28) {
				if ((crtc.reg[0x28] & 3) >= 2) {
					sprite->Connect(FALSE);
				}
				else {
					sprite->Connect(TRUE);
				}
			}

			// Recalc at next timing
			crtc.changed = TRUE;
			if ((addr == 4) || (addr == 5) || (addr == 6) || (addr == 7) ||
				(addr == 12) || (addr == 13) || (addr == 14) || (addr == 15) ||
				(addr == 40)) {
				NotifyGeometryChanged();
			}
			if ((addr == 8) || (addr == 9)) {
				NotifyTimingChanged();
			}
			SyncPx68kState();
			return;
		}

		// Raster interrupt
		if ((addr == 18) || (addr == 19)) {
			if (render && (render->GetCompositorMode() == Render::compositor_original)) {
				musashi_flush_timeslice();
			}
			crtc.raster_int = (crtc.reg[19] << 8) + crtc.reg[18];
			crtc.raster_int &= 0x3ff;
			if (UseAlternateRasterTiming() || UsePx68kRasterTiming()) {
				CheckRaster();
			}
			SyncPx68kState();
			return;
		}

		// Text scroll
		if ((addr >= 20) && (addr <= 23)) {
			if (render && (render->GetCompositorMode() == Render::compositor_original)) {
				musashi_flush_timeslice();
			}
			crtc.text_scrlx = (crtc.reg[21] << 8) + crtc.reg[20];
			crtc.text_scrlx &= 0x3ff;
			crtc.text_scrly = (crtc.reg[23] << 8) + crtc.reg[22];
			crtc.text_scrly &= 0x3ff;
			NotifyTextScrollChanged(crtc.text_scrlx, crtc.text_scrly);
			SyncPx68kState();

#if defined(CRTC_LOG)
			LOG2(Log::Normal, "Text scroll x=%d y=%d", crtc.text_scrlx, crtc.text_scrly);
#endif	// CRTC_LOG
			return;
		}

		// Graphic scroll
		if ((addr >= 24) && (addr <= 39)) {
			if (render && (render->GetCompositorMode() == Render::compositor_original)) {
				musashi_flush_timeslice();
			}
			reg = addr & ~3;
			addr -= 24;
			addr >>= 2;
			ASSERT(addr <= 3);
			crtc.grp_scrlx[addr] = (crtc.reg[reg+1] << 8) + crtc.reg[reg+0];
			crtc.grp_scrly[addr] = (crtc.reg[reg+3] << 8) + crtc.reg[reg+2];
			if (addr == 0) {
				crtc.grp_scrlx[addr] &= 0x3ff;
				crtc.grp_scrly[addr] &= 0x3ff;
			}
			else {
				crtc.grp_scrlx[addr] &= 0x1ff;
				crtc.grp_scrly[addr] &= 0x1ff;
			}
			NotifyGrpScrollChanged(addr, crtc.grp_scrlx[addr], crtc.grp_scrly[addr]);
			SyncPx68kState();
			return;
		}

		// Text VRAM
		if ((addr >= 42) && (addr <= 47)) {
			if (render && (render->GetCompositorMode() == Render::compositor_original)) {
				musashi_flush_timeslice();
			}
			TextVRAM();
			if (UsePx68kRasterTiming() && (px68k_crtc_mode & 0x08) && (addr == 45)) {
				tvram->RasterCopy();
				crtc.raster_copy = FALSE;
				crtc.raster_exec = FALSE;
			}
			SyncPx68kState();
		}
		SyncPx68kState();
		return;
	}

	// $E80480-$E804FF : Internal port
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// Even bytes are ignored
		if ((addr & 1) == 0) {
			return;
		}

		if (render && (render->GetCompositorMode() == Render::compositor_original)) {
			musashi_flush_timeslice();
		}

		if (UsePx68kRasterTiming()) {
			px68k_crtc_mode = (BYTE)(data | (px68k_crtc_mode & 0x02));
			if (render) {
				render->CRTCRegWrite(memdev.first + addr, (BYTE)data);
			}
			if (px68k_crtc_mode & 0x08) {
				crtc.raster_copy = TRUE;
				TextVRAM();
				tvram->RasterCopy();
				crtc.raster_copy = FALSE;
				crtc.raster_exec = FALSE;
			}
			else {
				crtc.raster_copy = FALSE;
			}
			if (px68k_crtc_mode & 0x02) {
				BeginPx68kFastClear();
			}
			SyncPx68kState();
			return;
		}

		// Odd byte is raster copy, fast clear reset
		if (data & 0x08) {
			crtc.raster_copy = TRUE;
			if (UsePx68kRasterTiming()) {
				if (render) {
					render->CRTCRegWrite(memdev.first + addr, (BYTE)data);
				}
				TextVRAM();
				tvram->RasterCopy();
				crtc.raster_copy = FALSE;
				crtc.raster_exec = FALSE;
			}
		}
		else {
			crtc.raster_copy = FALSE;
		}
		if (data & 0x02) {
			// Raster copy and fast clear (X68030'90)
			if ((crtc.fast_clr == 0) && !crtc.raster_copy) {
#if defined(CRTC_LOG)
				LOG0(Log::Normal, "Graphic clear fast enable");
#endif	// CRTC_LOG
				crtc.fast_clr = 1;
				if (UsePx68kRasterTiming()) {
					BeginPx68kFastClear();
				}
			}
#if defined(CRTC_LOG)
			else {
				LOG1(Log::Normal, "Graphic clear fast state State=%d", crtc.fast_clr);
			}
#endif	//CRTC_LOG
		}
		SyncPx68kState();
		return;
	}

	LOG2(Log::Warning, "Illegal address write $%06X <- $%02X",
							memdev.first + addr, data);
	SyncPx68kState();
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL CRTC::ReadOnly(DWORD addr) const
{
	BYTE data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Loop at $800 boundary
	addr &= 0x7ff;

	// $E80000-$E803FF : Register area
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return 0xff;
		}

		// Read (invert odd/even)
		addr ^= 1;
		return crtc.reg[addr];
	}

	// $E80480-$E804FF : Internal port
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// Even bytes are 0
		if ((addr & 1) == 0) {
			return 0;
		}

		if (UsePx68kRasterTiming()) {
			data = px68k_crtc_mode;
			if (crtc.fast_clr) {
				data |= 0x02;
			}
			else {
				data &= 0xfd;
			}
			return data;
		}

		// Odd byte is raster copy, graphic clear reset
		data = 0;
		if (crtc.raster_copy) {
			data |= 0x08;
		}
		if (crtc.fast_clr == 2) {
			data |= 0x02;
		}
		return data;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Get internal data
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::GetCRTC(crtc_t *buffer) const
{
	ASSERT(buffer);

	// Copy internal data
	*buffer = crtc;
}

//---------------------------------------------------------------------------
//
//	Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Callback(Event* /*ev*/)
{
	ASSERT(this);

	// HSync and HDisp are exclusive
	if (crtc.h_disp) {
		HSync();
	}
	else {
		HDisp();
	}
	SyncPx68kState();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	H-SYNC start
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HSync()
{
	int hus;

	ASSERT(this);

	// Notify printer (ignore busy)
	ASSERT(printer);
	printer->HSync();

	// V-SYNC count
	crtc.v_synccnt--;
	if (crtc.v_synccnt == 0) {
		VSync();
	}

	// V-BLANK count
	crtc.v_blankcnt--;
	if (crtc.v_blankcnt == 0) {
		VBlank();
	}

	// Set time until next timing (H-DISP start)
	crtc.ns += crtc.h_pulse;
	hus = Ns2Hus(crtc.ns);
	hus -= crtc.hus;
	event.SetTime(hus);
	crtc.hus += hus;

	// Adjust (every 40ms)
	if (crtc.hus >= 80000) {
		crtc.hus -= 80000;
		ASSERT(crtc.ns >= 40000000);
		crtc.ns -= 40000000;
	}

	// Set flag
	crtc.h_disp = FALSE;

	// GPIP set
	mfp->SetGPIP(7, 1);

	// Raster interrupt (Alt timing)
	if (UsePx68kRasterTiming()) {
		crtc.raster_count = (crtc.v_scan >= 0) ? (crtc.v_scan & 0x3ff) : 0;
		CheckRaster();
	}
	else if (UseAlternateRasterTiming()) {
		CheckRaster();
	}

	// Rendering
	crtc.v_scan++;
	if (!crtc.v_blank) {
		// Renderer sync
		render->HSync(crtc.v_scan);
	}

	// Text screen raster copy
	if (!UsePx68kRasterTiming() && crtc.raster_copy && crtc.raster_exec) {
		tvram->RasterCopy();
		crtc.raster_exec = FALSE;
	}

	// Graphic screen fast clear
	if (!UsePx68kRasterTiming() && crtc.fast_clr == 2) {
		gvram->FastClr(&crtc);
	}

	if (UsePx68kRasterTiming()) {
		// PX68k timing follows the scanline index directly.
	}
	else if (UseAlternateRasterTiming()) {
		crtc.raster_count++;
	}
	SyncPx68kState();
}

//---------------------------------------------------------------------------
//
//	H-DISP start
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HDisp()
{
	int ns;
	int hus;

	ASSERT(this);

	// Raster interrupt (Original timing)
	if (UsePx68kRasterTiming()) {
		// PX68k timing checks on the scanline transition in HSync.
	}
	else if (!UseAlternateRasterTiming()) {
		CheckRaster();
		crtc.raster_count++;
	}
	else {
		CheckRaster();
	}

	// Set time until next timing (H-SYNC start)
	ns = crtc.h_sync - crtc.h_pulse;
	ASSERT(ns > 0);
	crtc.ns += ns;
	hus = Ns2Hus(crtc.ns);
	hus -= crtc.hus;
	event.SetTime(hus);
	crtc.hus += hus;

	// Set flag
	crtc.h_disp = TRUE;

	// GPIP set
	mfp->SetGPIP(7,0);

	// Raster copy start
	crtc.raster_exec = TRUE;
	SyncPx68kState();
}

//---------------------------------------------------------------------------
//
//	V-SYNC start (also starts V-DISP)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::VSync()
{
	ASSERT(this);

	// V-SYNC not ended yet
	if (!crtc.v_disp) {
		// Set flag
		crtc.v_disp = TRUE;

		// Set counter
		crtc.v_synccnt = (crtc.v_sync - crtc.v_pulse);
		return;
	}

	// If resolution changed, recalc now
	if (crtc.changed) {
		ReCalc();
	}

	// Set time until V-SYNC end
	crtc.v_synccnt = crtc.v_pulse;

	// V-BLANK state and counter setup
	if (crtc.v_front < 0) {
		// Not yet displayed (minus)
		crtc.v_blank = FALSE;
		crtc.v_blankcnt = (-crtc.v_front) + 1;
	}
	else {
		// Already in blank (normal)
		crtc.v_blank = TRUE;
		crtc.v_blankcnt = (crtc.v_pulse + crtc.v_back + 1);
	}

	// Set flag
	crtc.v_disp = FALSE;

	// Reset raster counter
	crtc.raster_count = 0;
}

//---------------------------------------------------------------------------
//
//	Recalculate
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::ReCalc()
{
	int dc;
	int over;
	WORD *p;

	ASSERT(this);
	ASSERT(crtc.changed);

	// If CRTC register 0 is not zero, recalc (Mac compatible)
	if (crtc.reg[0x0] != 0) {
#if defined(CRTC_LOG)
		LOG0(Log::Normal, "Recalculate");
#endif	// CRTC_LOG

		// Get dot clock
		dc = Get8DotClock();

		// Horizontal (all in ns units)
		crtc.h_sync = (crtc.reg[0x0] + 1) * dc / 100;
		crtc.h_pulse = (crtc.reg[0x02] + 1) * dc / 100;
		crtc.h_back = (crtc.reg[0x04] + 5 - crtc.reg[0x02] - 1) * dc / 100;
		crtc.h_front = (crtc.reg[0x0] + 1 - crtc.reg[0x06] - 5) * dc / 100;

		// Vertical (all in H-Sync units)
		p = (WORD *)crtc.reg;
		crtc.v_sync = ((p[4] & 0x3ff) + 1);
		crtc.v_pulse = ((p[5] & 0x3ff) + 1);
		crtc.v_back = ((p[6] & 0x3ff) + 1) - crtc.v_pulse;
		crtc.v_front = crtc.v_sync - ((p[7] & 0x3ff) + 1);

		// If V-FRONT is negative, 1 is subtracted (interlace, scan doubler)
		if (crtc.v_front < 0) {
			over = -crtc.v_front;
			over -= crtc.v_back;
			if (over >= crtc.v_pulse) {
				crtc.v_front = -1;
			}
		}

		// Dot count
		crtc.h_dots = (crtc.reg[0x0] + 1);
		crtc.h_dots -= (crtc.reg[0x02] + 1);
		crtc.h_dots -= (crtc.reg[0x04] + 5 - crtc.reg[0x02] - 1);
		crtc.h_dots -= (crtc.reg[0x0] + 1 - crtc.reg[0x06] - 5);
		crtc.h_dots *= 8;
		crtc.v_dots = crtc.v_sync - crtc.v_pulse - crtc.v_back - crtc.v_front;
	}

	// Horizontal settings (normal)
	crtc.hd = (crtc.reg[0x28] & 3);
	if (crtc.hd == 3) {
		LOG0(Log::Warning, "High dot 50MHz mode (CompactXVI)");
	}
	if (crtc.hd == 0) {
		crtc.h_mul = 2;
	}
	else {
		crtc.h_mul = 1;
	}

	// If crtc.hd is 2 or more, sprites are disabled
	if (crtc.hd >= 2) {
		// 768x512 or VGA mode (no sprite)
		sprite->Connect(FALSE);
		crtc.textres = TRUE;
	}
	else {
		// 256x256 or 512x512 mode (sprite enabled)
		sprite->Connect(TRUE);
		crtc.textres = FALSE;
	}

	// Vertical settings (normal)
	crtc.vd = (crtc.reg[0x28] >> 2) & 3;
	if (crtc.reg[0x28] & 0x10) {
		// 31kHz
		crtc.lowres = FALSE;
		if (crtc.vd == 3) {
			// Interlace x1024dot mode
			crtc.v_mul = 0;
		}
		else {
			// Interlace, normal 512 mode (x1), normal 256dot mode (x2)
			crtc.v_mul = 2 - crtc.vd;
		}
	}
	else {
		// 15kHz
		crtc.lowres = TRUE;
		if (crtc.vd == 0) {
			// Normal 256dot mode (x2)
			crtc.v_mul = 2;
		}
		else {
			// Interlace 512dot mode (x1)
			crtc.v_mul = 0;
		}
	}

	// Notify renderer
	render->SetCRTC();

	// Clear flag
	crtc.changed = FALSE;
	SyncPx68kState();
}


//---------------------------------------------------------------------------
//
//	V-BLANK start (also starts V-SCREEN)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::VBlank()
{
	ASSERT(this);

	// If not displayed, start blank
	if (!crtc.v_blank) {
		// Set blank counter
		crtc.v_blankcnt = crtc.v_pulse + crtc.v_back + crtc.v_front;
		ASSERT((crtc.v_front < 0) || ((int)crtc.v_synccnt == crtc.v_front));

		// Flag
		crtc.v_blank = TRUE;

		// GPIP
		mfp->EventCount(0, 0);
		mfp->SetGPIP(4, 0);

		// Graphic clear
		if (UsePx68kRasterTiming()) {
			if (px68k_crtc_mode & 0x02) {
				if (crtc.fast_clr) {
					crtc.fast_clr--;
					if (!crtc.fast_clr) {
						px68k_crtc_mode &= 0xfd;
						px68k_fastclrline = 0;
						px68k_fastclrmask = 0;
					}
				}
			else {
				crtc.fast_clr = (crtc.reg[0x29] & 0x10) ? 1 : 2;
				BeginPx68kFastClear();
				if (render) {
					render->GVRAMFastClear();
				}
			}
			}
		}
		else if (crtc.fast_clr == 2) {
#if defined(CRTC_LOG)
			LOG0(Log::Normal, "Graphic clear end");
#endif	// CRTC_LOG
			crtc.fast_clr = 0;
			px68k_fastclrline = 0;
			px68k_fastclrmask = 0;
		}

		// Renderer display end
		render->EndFrame();
		crtc.v_scan = crtc.v_dots + 1;
		SyncPx68kState();
		return;
	}

	// Set non-display counter
	crtc.v_blankcnt = crtc.v_sync;
	crtc.v_blankcnt -= (crtc.v_pulse + crtc.v_back + crtc.v_front);

	// Flag
	crtc.v_blank = FALSE;

	// GPIP
	mfp->EventCount(0, 1);
	mfp->SetGPIP(4, 1);

	// Graphic clear start
	if (!UsePx68kRasterTiming() && crtc.fast_clr == 1) {
#if defined(CRTC_LOG)
		LOG1(Log::Normal, "Graphic clear start data=%02X", crtc.reg[42]);
#endif	// CRTC_LOG
		crtc.fast_clr = 2;
		gvram->FastSet((DWORD)crtc.reg[42]);
		gvram->FastClr(&crtc);
	}

	// Renderer display start, counter up
	crtc.v_scan = 0;
	render->StartFrame();
	crtc.v_count++;
	SyncPx68kState();
}

//---------------------------------------------------------------------------
//
//	Get display frequency
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::GetHVHz(DWORD *h, DWORD *v) const
{
	DWORD d;
	DWORD t;

	// assert
	ASSERT(h);
	ASSERT(v);

	// Check
	if ((crtc.h_sync == 0) || (crtc.v_sync < 100)) {
		// NO SIGNAL
		*h = 0;
		*v = 0;
		return;
	}

	// ex. 31.5kHz = 3150
	d = 100 * 1000 * 1000;
	d /= crtc.h_sync;
	*h = d;

	// ex. 55.46Hz = 5546
	t = crtc.v_sync;
    t *= crtc.h_sync;
	t /= 100;
	d = 1000 * 1000 * 1000;
	d /= t;
	*v = d;
}

//---------------------------------------------------------------------------
//
//	Get 8 dot clock (~100)
//
//---------------------------------------------------------------------------
int FASTCALL CRTC::Get8DotClock() const
{
	int hf;
	int hd;
	int index;

	ASSERT(this);

	// Get HF, HD from CRTC R20
	hf = (crtc.reg[0x28] >> 4) & 1;
	hd = (crtc.reg[0x28] & 3);

	// Create index
	index = hf * 4 + hd;
	if (crtc.hrl) {
		index += 8;
	}

	return DotClockTable[index];
}

//---------------------------------------------------------------------------
//
//	8 dot clock table
//	(HRL,HF,HD combined values. 0.01ns units)
//
//---------------------------------------------------------------------------
const int CRTC::DotClockTable[16] = {
	// HRL=0
	164678, 82339, 164678, 164678,
	69013, 34507, 23004, 31778,
	// HRL=1
	164678, 82339, 164678, 164678,
	92017, 46009, 23004, 31778
};

//---------------------------------------------------------------------------
//
//	Set HRL
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::SetHRL(BOOL flag)
{
	if (crtc.hrl != flag) {
		// Recalc at next timing
		crtc.hrl = flag;
		crtc.changed = TRUE;
		NotifyGeometryChanged();
		NotifyTimingChanged();
		SyncPx68kState();
	}
}

//---------------------------------------------------------------------------
//
//	Get HRL
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::GetHRL() const
{
	return crtc.hrl;
}

//---------------------------------------------------------------------------
//
//	Raster interrupt check
//	Not supported in interlace mode
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::CheckRaster()
{
	BOOL hit;

	hit = (((DWORD)crtc.raster_count & 0x3ff) == ((DWORD)crtc.raster_int & 0x3ff));

	if (hit) {
		// Match
		mfp->SetGPIP(6, 0);
#if defined(CRTC_LOG)
		LOG2(Log::Normal, "Raster interrupt hit raster=%d scan=%d", crtc.raster_count, crtc.v_scan);
#endif	// CRTC_LOG
	}
	else {
		// No match
		mfp->SetGPIP(6, 1);
	}
}

//---------------------------------------------------------------------------
//
//	Text VRAM setup
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::TextVRAM()
{
	DWORD b;
	DWORD w;

	// Access, multi
	if (crtc.reg[43] & 1) {
		b = (DWORD)crtc.reg[42];
		b >>= 4;

		// b4 is multi flag
		b |= 0x10;
		tvram->SetMulti(b);
	}
	else {
		tvram->SetMulti(0);
	}

	// Access mask
	if (crtc.reg[43] & 2) {
		w = (DWORD)crtc.reg[47];
		w <<= 8;
		w |= (DWORD)crtc.reg[46];
		tvram->SetMask(w);
	}
	else {
		tvram->SetMask(0);
	}

	// Raster copy
	tvram->SetCopyRaster((DWORD)crtc.reg[45], (DWORD)crtc.reg[44],
						(DWORD)(crtc.reg[42] & 0x0f));
	if (UsePx68kRasterTiming() && crtc.raster_copy && crtc.raster_exec) {
		tvram->RasterCopy();
		crtc.raster_copy = FALSE;
		crtc.raster_exec = FALSE;
	}
	NotifyMarkAllTextDirty();
	SyncPx68kState();
}

const Px68kCrtcHost* FASTCALL CRTC::GetPx68kHost() const
{
	if (!render) {
		return NULL;
	}
	return render->GetPx68kCrtcHost();
}

void FASTCALL CRTC::NotifyPx68kStateView()
{
	const Px68kCrtcHost *host = GetPx68kHost();

	if (host && host->ApplyStateView) {
		host->ApplyStateView(host->ctx, &px68k_state_view);
	}
}

void FASTCALL CRTC::NotifyMarkAllTextDirty()
{
	const Px68kCrtcHost *host = GetPx68kHost();
	Render::render_t *work;
	int i;

	if (host && host->MarkAllTextDirty) {
		host->MarkAllTextDirty(host->ctx);
		return;
	}

	if (!render) {
		return;
	}

	work = render->GetWorkAddr();
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

void FASTCALL CRTC::NotifyMarkTextDirtyLine(DWORD line)
{
	const Px68kCrtcHost *host = GetPx68kHost();
	Render::render_t *work;

	line &= 0x3ff;
	if (host && host->MarkTextDirtyLine) {
		host->MarkTextDirtyLine(host->ctx, line);
		return;
	}

	if (!render) {
		return;
	}

	work = render->GetWorkAddr();
	if (!work) {
		return;
	}

	work->textmod[line] = TRUE;
	work->mix[line] = TRUE;
	work->draw[line] = TRUE;
}

void FASTCALL CRTC::NotifyTextScrollChanged(DWORD x, DWORD y)
{
	const Px68kCrtcHost *host = GetPx68kHost();

	if (host && host->TextScrollChanged) {
		host->TextScrollChanged(host->ctx, x, y);
		return;
	}

	if (render) {
		render->TextScrl(x, y);
	}
}

void FASTCALL CRTC::NotifyGrpScrollChanged(int block, DWORD x, DWORD y)
{
	const Px68kCrtcHost *host = GetPx68kHost();

	if (host && host->GrpScrollChanged) {
		host->GrpScrollChanged(host->ctx, block, x, y);
		return;
	}

	if (render) {
		render->GrpScrl(block, x, y);
	}
}

void FASTCALL CRTC::NotifyScreenChanged()
{
	const Px68kCrtcHost *host = GetPx68kHost();

	if (host && host->ScreenChanged) {
		host->ScreenChanged(host->ctx);
		return;
	}

	if (render) {
		render->SetCRTC();
	}
}

void FASTCALL CRTC::NotifyGeometryChanged()
{
	const Px68kCrtcHost *host = GetPx68kHost();

	if (host && host->GeometryChanged) {
		host->GeometryChanged(host->ctx);
		return;
	}

	NotifyScreenChanged();
}

void FASTCALL CRTC::NotifyTimingChanged()
{
	const Px68kCrtcHost *host = GetPx68kHost();

	if (host && host->TimingChanged) {
		host->TimingChanged(host->ctx);
		return;
	}

	if (render) {
		render->SetVC();
	}
}

void FASTCALL CRTC::NotifyModeChanged(BYTE mode)
{
	const Px68kCrtcHost *host = GetPx68kHost();

	if (host && host->ModeChanged) {
		host->ModeChanged(host->ctx, mode);
		return;
	}

	NotifyScreenChanged();
}

void FASTCALL CRTC::SyncPx68kState()
{
	int i;
	const DWORD px68k_hstart = (DWORD)(((crtc.reg[0x04] << 8) | crtc.reg[0x05]) & 1023);
	const DWORD px68k_hend = (DWORD)(((crtc.reg[0x06] << 8) | crtc.reg[0x07]) & 1023);
	DWORD px68k_vstart = (DWORD)(((crtc.reg[0x0c] << 8) | crtc.reg[0x0d]) & 1023);
	DWORD px68k_vend = (DWORD)(((crtc.reg[0x0e] << 8) | crtc.reg[0x0f]) & 1023);

	memset(&px68k_state_view, 0, sizeof(px68k_state_view));

	for (i = 0; i < 48; i++) {
		px68k_state_view.state.regs[i] = crtc.reg[i];
	}

	px68k_state_view.state.mode = px68k_crtc_mode;
	px68k_state_view.state.hrl = crtc.hrl;
	px68k_state_view.state.lowres = crtc.lowres;
	px68k_state_view.state.textres = crtc.textres;
	px68k_state_view.state.changed = crtc.changed;
	px68k_state_view.state.h_disp = crtc.h_disp;
	px68k_state_view.state.v_disp = crtc.v_disp;
	px68k_state_view.state.v_blank = crtc.v_blank;
	px68k_state_view.state.v_count = crtc.v_count;
	px68k_state_view.state.raster_count = crtc.raster_count;
	if (px68k_hend > px68k_hstart) {
		px68k_state_view.state.textdotx = (px68k_hend - px68k_hstart) * 8;
	}
	else if (crtc.h_dots > 0) {
		// px68k keeps the previous TextDotX when an intermediate CRTC write
		// produces HEND <= HSTART. Use XM6's resolved visible width instead
		// of exposing a zero-width frame to the px68k compositor.
		px68k_state_view.state.textdotx = (DWORD)crtc.h_dots;
	}
	px68k_state_view.state.textdoty = 0;
	if (px68k_vend > px68k_vstart) {
		px68k_state_view.state.textdoty = (DWORD)(px68k_vend - px68k_vstart);
		if ((crtc.reg[0x29] & 0x14) == 0x10) {
			px68k_state_view.state.textdoty >>= 1;
		}
		else if ((crtc.reg[0x29] & 0x14) == 0x04) {
			px68k_state_view.state.textdoty <<= 1;
		}
	}
	px68k_state_view.state.vstart = (WORD)px68k_vstart;
	px68k_state_view.state.vend = (WORD)px68k_vend;
	if ((crtc.v_mul == 2) && !crtc.lowres && (crtc.v_dots > 0)) {
		const WORD effective_vstart = (WORD)((crtc.v_back + 5) & 0x3ffu);
		const WORD effective_vend = (WORD)((effective_vstart + crtc.v_dots) & 0x3ffu);
		px68k_state_view.state.textdoty = (DWORD)(crtc.v_dots >> 1);
		px68k_vstart = effective_vstart;
		px68k_vend = effective_vend;
	}
	px68k_state_view.state.hstart = (WORD)px68k_hstart;
	px68k_state_view.state.hend = (WORD)px68k_hend;
	px68k_state_view.state.h_sync = (DWORD)crtc.h_sync;
	px68k_state_view.state.h_pulse = (DWORD)crtc.h_pulse;
	px68k_state_view.state.h_back = (DWORD)crtc.h_back;
	px68k_state_view.state.h_front = (DWORD)crtc.h_front;
	px68k_state_view.state.v_sync = (DWORD)crtc.v_sync;
	px68k_state_view.state.v_pulse = (DWORD)crtc.v_pulse;
	px68k_state_view.state.v_back = (DWORD)crtc.v_back;
	px68k_state_view.state.v_front = (DWORD)crtc.v_front;
	px68k_state_view.state.ns = crtc.ns;
	px68k_state_view.state.hus = crtc.hus;
	px68k_state_view.state.v_synccnt = crtc.v_synccnt;
	px68k_state_view.state.v_blankcnt = crtc.v_blankcnt;
	px68k_state_view.state.textscrollx = crtc.text_scrlx;
	px68k_state_view.state.textscrolly = crtc.text_scrly;
	for (i = 0; i < 4; i++) {
		px68k_state_view.state.grphscrollx[i] = crtc.grp_scrlx[i];
		px68k_state_view.state.grphscrolly[i] = crtc.grp_scrly[i];
	}
	px68k_state_view.state.fastclr = (BYTE)crtc.fast_clr;
	px68k_state_view.state.dispscan = (BYTE)((crtc.v_scan >= 0) ? crtc.v_scan : 0);
	px68k_state_view.state.fastclrline = (DWORD)px68k_fastclrline;
	px68k_state_view.state.fastclrmask = px68k_fastclrmask;
	px68k_state_view.state.intline = (WORD)crtc.raster_int;
	if ((crtc.reg[0x29] & 0x14) == 0x10) {
		px68k_state_view.state.vstep = 1;
	}
	else if ((crtc.reg[0x29] & 0x14) == 0x04) {
		px68k_state_view.state.vstep = 4;
	}
	else {
	px68k_state_view.state.vstep = 2;
	}
	px68k_state_view.state.visible_vline = 0xffffffffu;
	if ((crtc.v_scan >= (int)px68k_vstart) &&
	    (crtc.v_scan < (int)px68k_vend)) {
		px68k_state_view.state.visible_vline = (DWORD)(((DWORD)(crtc.v_scan - (int)px68k_vstart) * (DWORD)px68k_state_view.state.vstep) / 2u);
	}
	else if ((crtc.v_mul == 2) && !crtc.lowres &&
	         (crtc.v_scan >= 0) && (crtc.v_scan <= crtc.v_dots)) {
		const DWORD line = (DWORD)((crtc.v_scan > 0) ? (crtc.v_scan - 1) : 0);
		px68k_state_view.state.visible_vline = (DWORD)((line * (DWORD)px68k_state_view.state.vstep) / 2u);
	}
	if ((px68k_state_view.state.visible_vline != 0xffffffffu) &&
	    (crtc.v_mul == 2) && !crtc.lowres) {
		px68k_state_view.state.visible_vline >>= 1;
	}
	px68k_state_view.state.hsync_clk = crtc.h_sync;
	px68k_state_view.state.hd = crtc.hd;
	px68k_state_view.state.vd = crtc.vd;
	px68k_state_view.state.rcflag[0] = crtc.raster_copy ? 1 : 0;
	px68k_state_view.state.rcflag[1] = crtc.raster_exec ? 1 : 0;
	px68k_state_view.state.vcreg0[0] = crtc.reg[0x28];
	px68k_state_view.state.vcreg0[1] = crtc.reg[0x29];
	px68k_state_view.state.vcreg1[0] = crtc.reg[0x2a];
	px68k_state_view.state.vcreg1[1] = crtc.reg[0x2b];
	px68k_state_view.state.vcreg2[0] = crtc.reg[0x2c];
	px68k_state_view.state.vcreg2[1] = crtc.reg[0x2d];

	px68k_state_view.timing_view.valid = 1;
	px68k_state_view.timing_view.crtc_vsync_high = (crtc.reg[0x29] & 0x10) ? 1 : 0;
	px68k_state_view.timing_view.crtc_vline_total = (DWORD)crtc.v_sync;
	px68k_state_view.timing_view.crtc_vstart = px68k_state_view.state.vstart;
	px68k_state_view.timing_view.crtc_vend = px68k_state_view.state.vend;
	px68k_state_view.timing_view.crtc_intline = px68k_state_view.state.intline;
	px68k_state_view.timing_view.crtc_vstep = px68k_state_view.state.vstep;
	px68k_state_view.timing_view.crtc_mode = px68k_state_view.state.mode;
	px68k_state_view.timing_view.crtc_fastclr = px68k_state_view.state.fastclr;

	NotifyPx68kStateView();
}

void FASTCALL CRTC::BeginPx68kFastClear()
{
	px68k_fastclrline = (DWORD)((crtc.v_scan >= 0) ? crtc.v_scan : 0);
	px68k_fastclrmask = kFastClearMask[crtc.reg[0x2b] & 15];
}
