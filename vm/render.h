//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001,2002 ï¿½Eï¿½oï¿½Eï¿½hï¿½Eï¿½D(ytanaka@ipc-tokai.or.jp)
//	[ ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½_ï¿½Eï¿½ï¿½Eï¿½ ]
//
//---------------------------------------------------------------------------

#if !defined(render_h)
#define render_h

#include "device.h"
#include "graphic_engine.h"
#include "px68k_crtc_port.h"
#include "render_interfaces.h"

class CRTC;
class GVRAM;
class Sprite;
class TVRAM;
class VC;
class XmVideoSnapshotAdapter;
class Px68kRenderAdapter;

//===========================================================================
//
//	ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½_ï¿½Eï¿½ï¿½Eï¿½
//
//===========================================================================
class Render : public Device, public IVideoStateView, public IPaletteResolver
{
public:
	enum compositor_mode_t {
		compositor_original = 0,
		compositor_fast = 1,
	};

	typedef struct {
		BOOL valid;
		int dst_y;
		int src_y;
		DWORD px68k_vline;
		int sprite_raster;
		int bg_raster;
		int layer_raster;
		int bg_vline;
		int vline_bg;
		BOOL visible;
		BOOL bg_on;
		BOOL bg_opaq;
		BOOL sprite_enabled;
		BOOL bgspflag;
		BOOL bgspdisp;
		BOOL gon;
		BOOL tron;
		BOOL pron;
		BOOL ton;
		BYTE vr2h;
		BYTE vr2l;
		int vscan;
		int vdots;
		DWORD vcount;
		BOOL vblank;
		int rcount;
		int vstep;
		int mixlen;
		BOOL lowres;
		int vmul;
	} fast_vertical_probe_sample_t;

	typedef struct {
		BOOL valid;
		int width;
		int height;
		int mixwidth;
		int mixheight;
		int mixpage;
		int mixtype;
		BOOL lowres;
		BOOL bgspflag;
		BOOL bgspdisp;
		int sample_count;
		fast_vertical_probe_sample_t samples[6];
	} fast_vertical_probe_snapshot_t;

	// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½fï¿½Eï¿½[ï¿½Eï¿½^ï¿½Eï¿½ï¿½Eï¿½`
	typedef struct {
		// ï¿½Eï¿½Sï¿½Eï¿½Ìï¿½ï¿½Eï¿½ï¿½Eï¿½
		BOOL act;						// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Ä‚ï¿½ï¿½Eï¿½é‚©
		BOOL enable;					// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½
		int count;						// ï¿½Eï¿½Xï¿½Eï¿½Pï¿½Eï¿½Wï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Aï¿½Eï¿½gï¿½Eï¿½Jï¿½Eï¿½Eï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^
		BOOL ready;						// ï¿½Eï¿½`ï¿½Eï¿½æ€ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Å‚ï¿½ï¿½Eï¿½Ä‚ï¿½ï¿½Eï¿½é‚©
		int first;						// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Xï¿½Eï¿½^
		int last;						// ï¿½Eï¿½\ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Iï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Xï¿½Eï¿½^

		// CRTC
		BOOL crtc;						// CRTCï¿½Eï¿½ÏXï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		int width;						// Xï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½hï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½(256ï¿½Eï¿½`)
		int h_mul;						// Xï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½{ï¿½Eï¿½ï¿½Eï¿½(1,2)
		int height;						// Yï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½hï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½(256ï¿½Eï¿½`)
		int v_mul;						// Yï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½{ï¿½Eï¿½ï¿½Eï¿½(0,1,2)
		BOOL lowres;					// 15kHzï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O

		// VC
		BOOL vc;						// VCï¿½Eï¿½ÏXï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O

		// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½
		BOOL mix[1024];					// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O(ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½)
		DWORD *mixbuf;					// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@
		DWORD *mixptr[8];				// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^
		DWORD mixshift[8];				// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^ï¿½Eï¿½ï¿½Eï¿½Yï¿½Eï¿½Vï¿½Eï¿½tï¿½Eï¿½g
		DWORD *mixx[8];					// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^ï¿½Eï¿½ï¿½Eï¿½Xï¿½Eï¿½Xï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^
		DWORD *mixy[8];					// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^ï¿½Eï¿½ï¿½Eï¿½Yï¿½Eï¿½Xï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^
		DWORD mixand[8];				// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^ï¿½Eï¿½ÌƒXï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½ANDï¿½Eï¿½l
		int mixmap[3];					// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½}ï¿½Eï¿½bï¿½Eï¿½v
		int mixtype;					// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^ï¿½Eï¿½Cï¿½Eï¿½v
		int mixpage;					// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½yï¿½Eï¿½[ï¿½Eï¿½Wï¿½Eï¿½ï¿½Eï¿½
		int mixwidth;					// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@ï¿½Eï¿½ï¿½Eï¿½
		int mixheight;					// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½
		int mixlen;						// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½(xï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½)

		// ï¿½Eï¿½`ï¿½Eï¿½ï¿½Eï¿½
		BOOL draw[1024];				// ï¿½Eï¿½`ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O(ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½)
		BOOL *drawflag;					// ï¿½Eï¿½`ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O(16dot)

		// ï¿½Eï¿½Rï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Xï¿½Eï¿½g
		BOOL contrast;					// ï¿½Eï¿½Rï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½ÏXï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		int contlevel;					// ï¿½Eï¿½Rï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Xï¿½Eï¿½g

		// ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½g
		BOOL palette;					// ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½ÏXï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		BOOL palmod[0x200];				// ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½ÏXï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		DWORD *palbuf;					// ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@
		DWORD *palptr;					// ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^
		const WORD *palvc;				// ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gVCï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^
		DWORD paldata[0x200];			// ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½fï¿½Eï¿½[ï¿½Eï¿½^
		BYTE pal64k[0x200];				// ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½fï¿½Eï¿½[ï¿½Eï¿½^ï¿½Eï¿½ÏŒ`

		// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gVRAM
		BOOL texten;					// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½\ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		BOOL textpal[1024];				// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		BOOL textmod[1024];				// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½Xï¿½Eï¿½Vï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O(ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½)
		BOOL *textflag;					// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½Xï¿½Eï¿½Vï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O(32dot)
		BYTE *textbuf;					// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@(ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½O)
		DWORD *textout;					// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@(ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½)
		const BYTE *texttv;				// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gTVRAMï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^
		DWORD textx;					// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½Xï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½X
		DWORD texty;					// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½Xï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½Y

		// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½NVRAM
		int grptype;					// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½^ï¿½Eï¿½Cï¿½Eï¿½v(0ï¿½Eï¿½`4)
		BOOL grpen[4];					// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½uï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½\ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		BOOL grppal[2048];				// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		BOOL grpmod[2048];				// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½Xï¿½Eï¿½Vï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O(ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½)
		BOOL *grpflag;					// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½Xï¿½Eï¿½Vï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O(16dot)
		DWORD *grpbuf[4];				// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½uï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@
		const BYTE* grpgv;				// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½NGVRAMï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^
		DWORD grpx[4];					// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½uï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½Xï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½X
		DWORD grpy[4];					// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½uï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½Xï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½Y

		// PCG
		BOOL pcgready[256 * 16];		// PCGï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½OKï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		DWORD pcguse[256 * 16];			// PCGï¿½Eï¿½gï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Jï¿½Eï¿½Eï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½g
		DWORD pcgpal[16];				// PCGï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½gï¿½Eï¿½pï¿½Eï¿½Jï¿½Eï¿½Eï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½g
		DWORD *pcgbuf;					// PCGï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@
		const BYTE* sprmem;				// ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½

		// ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½g
		DWORD **spptr;					// ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½gï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^ï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@
		DWORD spreg[0x200];				// ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Wï¿½Eï¿½Xï¿½Eï¿½^ï¿½Eï¿½Û‘ï¿½
		BOOL spuse[128];				// ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½gï¿½Eï¿½gï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O

		// BG
		DWORD bgreg[2][64 * 64];		// BGï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Wï¿½Eï¿½Xï¿½Eï¿½^ï¿½Eï¿½{ï¿½Eï¿½ÏXï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O($10000)
		BOOL bgall[2][64];				// BGï¿½Eï¿½ÏXï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O(ï¿½Eï¿½uï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½Pï¿½Eï¿½ï¿½Eï¿½)
		BOOL bgdisp[2];					// BGï¿½Eï¿½\ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		BOOL bgarea[2];					// BGï¿½Eï¿½\ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½A
		BOOL bgsize;					// BGï¿½Eï¿½\ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Tï¿½Eï¿½Cï¿½Eï¿½Y(16dot=TRUE)
		DWORD **bgptr[2];				// BGï¿½Eï¿½|ï¿½Eï¿½Cï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½^+ï¿½Eï¿½fï¿½Eï¿½[ï¿½Eï¿½^
		BOOL bgmod[2][1024];			// BGï¿½Eï¿½Xï¿½Eï¿½Vï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		DWORD bgx[2];					// BGï¿½Eï¿½Xï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½(X)
		DWORD bgy[2];					// BGï¿½Eï¿½Xï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½(Y)

		// BG/ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½
		BOOL bgspflag;					// BG/ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½gï¿½Eï¿½\ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		BOOL bgspdisp;					// BG/ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½gCPU/Videoï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		BOOL bgspmod[512];				// BG/ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½gï¿½Eï¿½Xï¿½Eï¿½Vï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
		DWORD *bgspbuf;					// BG/ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½gï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@
		DWORD fast_bg_linebuf[1024];
		WORD fast_bg_pribuf[1024];
		BYTE fast_text_trflag[1024];
		DWORD fast_grp_linebuf[1024];
		DWORD fast_grp_linebuf_sp[1024];
		DWORD fast_grp_linebuf_sp2[1024];
		BOOL fast_grp_linebuf_sp_tr[1024];
		DWORD fast_stamp_counter;
		DWORD fast_mix_stamp[1024];
		DWORD fast_mix_done[1024];
		DWORD fast_bg_stamp[512];
		DWORD fast_bg_done[512];
		DWORD zero;						// ï¿½Eï¿½Xï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½_ï¿½Eï¿½~ï¿½Eï¿½[(0)
	} render_t;

	// ï¿½Eï¿½ï¿½Eï¿½{ï¿½Eï¿½tï¿½Eï¿½@ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Nï¿½Eï¿½Vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½
	Render(VM *p);
										// ï¿½Eï¿½Rï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Nï¿½Eï¿½^
	BOOL FASTCALL Init();
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½
	void FASTCALL Cleanup();
										// ï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Aï¿½Eï¿½bï¿½Eï¿½v
	void FASTCALL Reset();
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Zï¿½Eï¿½bï¿½Eï¿½g
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// ï¿½Eï¿½Zï¿½Eï¿½[ï¿½Eï¿½u
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½h
	void FASTCALL ApplyCfg(const Config *config);
										// ï¿½Eï¿½Ý’ï¿½Kï¿½Eï¿½p

	// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½API(ï¿½Eï¿½Rï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½)
	void FASTCALL EnableAct(BOOL enable){ render.enable = enable; }
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½
	BOOL FASTCALL IsActive() const		{ return render.act; }
										// ï¿½Eï¿½Aï¿½Eï¿½Nï¿½Eï¿½eï¿½Eï¿½Bï¿½Eï¿½uï¿½Eï¿½ï¿½Eï¿½
	BOOL FASTCALL IsReady() const		{ return (BOOL)(render.count > 0); }
	void FASTCALL Complete()			{ render.count = 0; }
	void FASTCALL SetTransparencyEnabled(BOOL enabled)	{ transparency_enabled = enabled ? TRUE : FALSE; }
	BOOL FASTCALL IsTransparencyEnabled() const		{ return transparency_enabled; }
	void FASTCALL SetOriginalBG0RenderEnabled(BOOL enabled)	{ original_bg0_render_enabled = enabled ? TRUE : FALSE; }
	BOOL FASTCALL IsOriginalBG0RenderEnabled() const		{ return original_bg0_render_enabled; }
	BOOL FASTCALL SetCompositorMode(int mode);
	int FASTCALL GetCompositorMode() const		{ return compositor_mode; }
	DWORD FASTCALL GetFastFallbackCount() const	{ return fast_fallback_count; }
	void FASTCALL GetFastVerticalProbeSnapshot(fast_vertical_probe_snapshot_t *out) const;
	BOOL FASTCALL SetRenderFastDummyEnabled(BOOL enable);
	BOOL FASTCALL IsRenderFastDummyEnabled() const	{ return render_fast_dummy_enabled; }
	const IVideoStateView* FASTCALL GetVideoStateView() const { return this; }
	const IPaletteResolver* FASTCALL GetPaletteResolver() const { return this; }
	const Px68kCrtcHost* FASTCALL GetPx68kCrtcHost() const;
	void FASTCALL CachePx68kStateView(const Px68kCrtcStateView *view);
	void FASTCALL SetRenderTarget(IRenderTarget *target) { render_target = target; }
	IRenderTarget* FASTCALL GetRenderTarget() const { return render_target; }
	void FASTCALL BeginVideoSnapshotFrame();
	void FASTCALL CaptureVideoSnapshotLine(int raster);
	void FASTCALL EndVideoSnapshotFrame();
	void FASTCALL ComposeVideo();
	virtual void FASTCALL StartFrame();
										// ï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Jï¿½Eï¿½n(V-DISP)
	virtual void FASTCALL EndFrame();
										// ï¿½Eï¿½tï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Iï¿½Eï¿½ï¿½Eï¿½(V-BLANK)
	virtual void FASTCALL HSync(int raster);
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½(rasterï¿½Eï¿½Ü‚ÅIï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½)
	void FASTCALL SetMixBuf(DWORD *buf, int width, int height);
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@ï¿½Eï¿½wï¿½Eï¿½ï¿½Eï¿½
	render_t* FASTCALL GetWorkAddr() 	{ return &render; }
	const render_t* FASTCALL GetWorkAddr() const { return &render; }
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½Nï¿½Eï¿½Aï¿½Eï¿½hï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Xï¿½Eï¿½æ“¾

	// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½API(ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½)
	virtual void FASTCALL SetCRTC();
										// CRTCï¿½Eï¿½Zï¿½Eï¿½bï¿½Eï¿½g
	virtual void FASTCALL SetVC();
										// VCï¿½Eï¿½Zï¿½Eï¿½bï¿½Eï¿½g
	void FASTCALL ApplyPendingCompositorMode();
	void FASTCALL ApplyCompositorMode(int mode);
	void FASTCALL ForceRecompose();
	void FASTCALL SetContrast(int cont);
										// ï¿½Eï¿½Rï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½Ý’ï¿½
	int FASTCALL GetContrast() const;
										// ï¿½Eï¿½Rï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½æ“¾
	void FASTCALL SetPalette(int index);
										// ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½Ý’ï¿½
	const DWORD* FASTCALL GetPalette() const;
										// ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@ï¿½Eï¿½æ“¾
	void FASTCALL TextMem(DWORD addr);
										// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gVRAMï¿½Eï¿½ÏX
	void FASTCALL TextScrl(DWORD x, DWORD y);
										// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½Xï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ÏX
	void FASTCALL TextCopy(DWORD src, DWORD dst, DWORD plane);
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Xï¿½Eï¿½^ï¿½Eï¿½Rï¿½Eï¿½sï¿½Eï¿½[
	void FASTCALL GrpMem(DWORD addr, DWORD block);
										// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½NVRAMï¿½Eï¿½ÏX
	void FASTCALL GrpAll(DWORD line, DWORD block);
										// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½NVRAMï¿½Eï¿½ÏX
	void FASTCALL GrpScrl(int block, DWORD x, DWORD y);
										// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½Xï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ÏX
	void FASTCALL SpriteReg(DWORD addr, DWORD data);
										// ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Wï¿½Eï¿½Xï¿½Eï¿½^ï¿½Eï¿½ÏX
	void FASTCALL BGScrl(int page, DWORD x, DWORD y);
										// BGï¿½Eï¿½Xï¿½Eï¿½Nï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ÏX
	void FASTCALL BGCtrl(int index, BOOL flag);
										// BGï¿½Eï¿½Rï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½[ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ÏX
	void FASTCALL BGMem(DWORD addr, WORD data);
										// BGï¿½Eï¿½ÏX
	void FASTCALL PCGMem(DWORD addr);
										// PCGï¿½Eï¿½ÏX

	const DWORD* FASTCALL GetTextBuf() const;
										// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@ï¿½Eï¿½æ“¾
	const DWORD* FASTCALL GetGrpBuf(int index) const;
										// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@ï¿½Eï¿½æ“¾
	const DWORD* FASTCALL GetPCGBuf() const;
										// PCGï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@ï¿½Eï¿½æ“¾
	const DWORD* FASTCALL GetBGSpBuf() const;
										// BG/ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½gï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@ï¿½Eï¿½æ“¾
	const DWORD* FASTCALL GetMixBuf() const;
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½oï¿½Eï¿½bï¿½Eï¿½tï¿½Eï¿½@ï¿½Eï¿½æ“¾

	const CRTC* FASTCALL GetCRTCDevice() const { return crtc; }
	const VC* FASTCALL GetVCDevice() const { return vc; }
	const TVRAM* FASTCALL GetTVRAMDevice() const { return tvram; }
	const GVRAM* FASTCALL GetGVRAMDevice() const { return gvram; }
	const Sprite* FASTCALL GetSpriteDevice() const { return sprite; }

private:
	friend class GraphicEngine;
	friend class OriginalGraphicEngine;
	friend class Px68kGraphicEngine;
	void FASTCALL StartFrameOriginal();
	void FASTCALL StartFrameFast();
	void FASTCALL EndFrameOriginal();
	void FASTCALL EndFrameFast();
	void FASTCALL HSyncOriginal(int raster);
	void FASTCALL HSyncFast(int raster);
	void FASTCALL SetCRTCOriginal();
	void FASTCALL SetCRTCFast();
	void FASTCALL SetVCOriginal();
	void FASTCALL SetVCFast();
	void FASTCALL InvalidateFrame();
	void FASTCALL InvalidateAll();
	void FASTCALL Process();
	void FASTCALL ProcessFast();
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½_ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½O
	void FASTCALL Video();
	void FASTCALL VideoFastPX68K();
										// VCï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½
	void FASTCALL SetupGrp(int first);
										// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½Nï¿½Eï¿½Zï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½Aï¿½Eï¿½bï¿½Eï¿½v
	void FASTCALL Contrast();
										// ï¿½Eï¿½Rï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Xï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½
	void FASTCALL Palette();
	void FASTCALL PaletteFastPX68K();
										// ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½
	void FASTCALL MakePalette();
										// ï¿½Eï¿½pï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½
	DWORD FASTCALL ConvPalette(int color, int ratio);
										// ï¿½Eï¿½Fï¿½Eï¿½ÏŠï¿½
	void FASTCALL Text(int raster);
	void FASTCALL TextFastPX68K(int raster);
										// ï¿½Eï¿½eï¿½Eï¿½Lï¿½Eï¿½Xï¿½Eï¿½g
	void FASTCALL Grp(int block, int raster);
										// ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½N
	void FASTCALL SpriteReset();
										// ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½gï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Zï¿½Eï¿½bï¿½Eï¿½g
	void FASTCALL BGSprite(int raster);
										// BG/ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½g
	void FASTCALL BG(int page, int raster, DWORD *buf);
										// BG
	void FASTCALL BGBlock(int page, int y);
										// BG(ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½uï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½N)
	void FASTCALL Mix(int offset);
	void FASTCALL MixFast(int y);
	void FASTCALL MixFastLine(int dst_y, int src_y);
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½
	void FASTCALL MixGrp(int y, DWORD *buf);
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½(ï¿½Eï¿½Oï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½tï¿½Eï¿½Bï¿½Eï¿½bï¿½Eï¿½N)
	void FASTCALL FastDrawSpriteLinePX(int raster, int pri, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active);
	void FASTCALL FastDrawBGPageLinePX(int page, int raster, BOOL gd, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active);
	void FASTCALL FastBuildBGLinePX(int sprite_raster, int bg_raster, BOOL ton, int tx_pri, int sp_pri, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active, BOOL *bg_opaq);
	void FASTCALL FastMixGrp(int y, DWORD *grp, DWORD *grp_sp, DWORD *grp_sp2,
		BOOL *grp_sp_tr, BOOL *gon, BOOL *tron, BOOL *pron);
	CRTC *crtc;
										// CRTC
	VC *vc;
										// VC
	Sprite *sprite;
										// ï¿½Eï¿½Xï¿½Eï¿½vï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½Cï¿½Eï¿½g
	TVRAM *tvram;
	GVRAM *gvram;
	GraphicEngine *backend;
	GraphicEngine *backend_original;
	GraphicEngine *backend_px68k;
	XmVideoSnapshotAdapter *video_snapshot_adapter;
	DWORD *palbuf_original;
	DWORD *palbuf_fast;
	int compositor_mode;
	int compositor_mode_pending;
	BOOL compositor_mode_switch_pending;
	DWORD fast_fallback_count;
	BOOL transparency_enabled;
	BOOL original_bg0_render_enabled;
	BOOL render_fast_dummy_enabled;
	Px68kCrtcHost px68k_crtc_host;
	Px68kCrtcStateView px68k_crtc_state_cache;
	IRenderTarget *render_target;
	Px68kRenderAdapter *px68k_adapter;
	render_t render;
										// ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½fï¿½Eï¿½[ï¿½Eï¿½^
	BOOL cmov;
										// CMOVï¿½Eï¿½Lï¿½Eï¿½ï¿½Eï¿½ï¿½Eï¿½bï¿½Eï¿½Vï¿½Eï¿½ï¿½Eï¿½
};

#endif	// render_h

