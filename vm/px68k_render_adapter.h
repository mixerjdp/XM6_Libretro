//---------------------------------------------------------------------------
//
//	PX68k Render Adapter
//
//	This module adapts the Px68kVideoEngine to the XM6 Render system.
//	It bridges the XM6 device model (CRTC, VC, GVRAM, TVRAM, Sprite)
//	with the standalone PX68k video engine implementation.
//
//---------------------------------------------------------------------------

#if !defined(px68k_render_adapter_h)
#define px68k_render_adapter_h

#include "os.h"
#include "px68k_video_engine.h"

class Render;
class CRTC;
class VC;
class GVRAM;
class TVRAM;
class Sprite;

//===========================================================================
//
//	PX68k Render Adapter
//
//===========================================================================

class Px68kRenderAdapter
{
public:
	Px68kRenderAdapter();
	~Px68kRenderAdapter();

	// Initialization
	BOOL Init();
	void Cleanup();
	void Reset();

	// Frame processing - called from Render's fast pipeline methods
	void StartFrame(Render *owner);
	void EndFrame(Render *owner);
	void HSync(Render *owner, int raster);
	void SetCRTC(Render *owner);
	void SetVC(Render *owner);
	void Process(Render *owner);

	// Sync XM6 device state to PX68k engine
	void SyncCRTCState(const CRTC *crtc);
	void SyncVCState(const VC *vc);
	void SyncGVRAMState(const GVRAM *gvram);
	void SyncTVRAMState(const TVRAM *tvram);
	void SyncSpriteState(const Sprite *sprite);
	void SyncPaletteState(const VC *vc);

	// Output access
	WORD* GetScreenBuffer() const;
	DWORD GetScreenWidth() const;
	DWORD GetScreenHeight() const;

	// Mode selection
	void SetDebugText(BOOL enable) { debug_text_ = enable; }
	void SetDebugGrp(BOOL enable) { debug_grp_ = enable; }
	void SetDebugSp(BOOL enable) { debug_sp_ = enable; }

private:
	// Internal helpers
	void SyncAllState(Render *owner);
	void DrawScanline(int visible_vline);
	void SyncDynamicState(Render *owner);

	// The actual PX68k video engine
	Px68kVideoEngine *engine_;

	// Cached device pointers
	const CRTC *crtc_;
	const VC *vc_;
	const GVRAM *gvram_;
	const TVRAM *tvram_;
	const Sprite *sprite_;

	// Debug flags
	BOOL debug_text_;
	BOOL debug_grp_;
	BOOL debug_sp_;

	// Current scanline
	int current_raster_;

	// Dirty flags
	BOOL crtc_dirty_;
	BOOL vc_dirty_;
	BOOL palette_dirty_;
};

#endif	// px68k_render_adapter_h
