//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 ・ｰ・ｩ・・ytanaka@ipc-tokai.or.jp)
//	[ Subventana MFC (renderizador) ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "render.h"
#include "mfc_sub.h"
#include "mfc_res.h"
#include "mfc_rend.h"

//===========================================================================
//
//	Ventana del buffer de renderizacion
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	constructor
//
//---------------------------------------------------------------------------
CRendBufWnd::CRendBufWnd(int nType)
{
	Render *render;

	// Adquisicion de renderizador
	render = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// tipo de almacenamiento
	m_nType = nType;

	// Tamano de desplazamiento, parametros de la ventana, direccion del buffer
	switch (nType) {
		// TEXT
		case 0:
			m_nScrlWidth = 1024;
			m_nScrlHeight = 1024;
			m_dwID = MAKEID('T', 'E', 'X', 'B');
			::GetMsg(IDS_SWND_REND_TEXT, m_strCaption);
			m_pRendBuf = render->GetTextBuf();
			break;
		// GRP0
		case 1:
			// Los buffers grﾃ｡ficos usan layout 1024x512 (duplican X para wrap).
			m_nScrlWidth = 1024;
			m_nScrlHeight = 512;
			m_dwID = MAKEID('G', 'P', '0', 'B');
			::GetMsg(IDS_SWND_REND_GP0, m_strCaption);
			m_pRendBuf = render->GetGrpBuf(0);
			break;
		// GRP1
		case 2:
			m_nScrlWidth = 1024;
			m_nScrlHeight = 512;
			m_dwID = MAKEID('G', 'P', '1', 'B');
			::GetMsg(IDS_SWND_REND_GP1, m_strCaption);
			m_pRendBuf = render->GetGrpBuf(1);
			break;
		// GRP2
		case 3:
			m_nScrlWidth = 1024;
			m_nScrlHeight = 512;
			m_dwID = MAKEID('G', 'P', '2', 'B');
			::GetMsg(IDS_SWND_REND_GP2, m_strCaption);
			m_pRendBuf = render->GetGrpBuf(2);
			break;
		// GRP3
		case 4:
			m_nScrlWidth = 1024;
			m_nScrlHeight = 512;
			m_dwID = MAKEID('G', 'P', '3', 'B');
			::GetMsg(IDS_SWND_REND_GP3, m_strCaption);
			m_pRendBuf = render->GetGrpBuf(3);
			break;
		// BG/SP
		case 5:
			m_nScrlWidth = 512;
			m_nScrlHeight = 512;
			m_dwID = MAKEID('B', 'G', 'S', 'P');
			::GetMsg(IDS_SWND_REND_BGSPRITE, m_strCaption);
			m_pRendBuf = render->GetBGSpBuf();
			break;
		default:
			ASSERT(FALSE);
			m_nScrlWidth = 0;
			m_nScrlHeight = 0;
	}
}

//---------------------------------------------------------------------------
//
//	configuracion
//
//---------------------------------------------------------------------------
void FASTCALL CRendBufWnd::Setup(int x, int y, int width, int height, BYTE *ptr)
{
	int i;
	int delta;
	int next = 0;
	const DWORD *p;

	// calculo de punteros
	p = m_pRendBuf;
	switch (m_nType) {
		case 0:
			// Texto.
			p += (y << 10);
			next = 1024;
			break;
		// grafico
		case 1:
		case 2:
		case 3:
		case 4:
			p += (y << 10);
			next = 1024;
			break;
		// BG/Sprite.
		case 5:
			p += (y << 9);
			next = 512;
			break;
		default:
			ASSERT(FALSE);
			break;
	}
	p += x;

	//medida contra la sobrecarga
	if ((y + height) > m_nScrlHeight) {
		height = m_nScrlHeight - y;
	}
	delta = 0;
	if ((x + width) > m_nScrlWidth) {
		delta = width - m_nScrlWidth + x;
		width = m_nScrlWidth - x;
	}

	// bucle
	for (i=0; i<height; i++) {
		// x, width繧貞鋸譯医＠縺ｦ繧ｳ繝斐・
		memcpy(ptr, p, (width << 2));
		p += next;
		ptr += (width << 2);
		ptr += (delta << 2);
	}
}

//---------------------------------------------------------------------------
//
//	Actualizacion del hilo de mensajes
//
//---------------------------------------------------------------------------
void FASTCALL CRendBufWnd::Update()
{
	int x;
	int y;
	DWORD rgb = 0;
	CString string;

	// Comprobacion de la ventana BMP
	if (!m_pBMPWnd) {
		return;
	}

	// Comprobacion del cursor del raton
	if ((m_pBMPWnd->m_nCursorX < 0) || (m_pBMPWnd->m_nCursorY < 0)) {
		m_StatusBar.SetPaneText(0, "");
		return;
	}

	// Calculos de coordenadas, sobrecomprobacion
	x = m_pBMPWnd->m_nCursorX + m_pBMPWnd->m_nScrlX;
	y = m_pBMPWnd->m_nCursorY + m_pBMPWnd->m_nScrlY;
	if (x >= m_nScrlWidth) {
		return;
	}
	if (y >= m_nScrlHeight) {
		return;
	}

	//Creacion de datos en pantalla
	switch (m_nType) {
		case 0:
			rgb = m_pRendBuf[(y << 10) + x];
			break;
		case 1:
		case 2:
		case 3:
		case 4:
			rgb = m_pRendBuf[(y << 10) + x];
			break;
		case 5:
			rgb = m_pRendBuf[(y << 9) + x];
			break;
		default:
			ASSERT(FALSE);
			break;
	}
	string.Format("( %d, %d) R%d G%d B%d",
				x, y, (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, (rgb & 0xff));
	m_StatusBar.SetPaneText(0, string);
}

//===========================================================================
//
//	ventana del buffer compuesto
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	constructor
//
//---------------------------------------------------------------------------
CMixBufWnd::CMixBufWnd()
{
	Render *render;

	// parametro basico
	m_nScrlWidth = 1024;
	m_nScrlHeight = 1024;
	m_dwID = MAKEID('M', 'I', 'X', 'B');
	::GetMsg(IDS_SWND_REND_MIX, m_strCaption);

	// Adquisicion del renderizador
	render = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// adquisicion de direcciones
	m_pRendWork = render->GetWorkAddr();
	ASSERT(m_pRendWork);
}

//---------------------------------------------------------------------------
//
//	configuracion
//
//---------------------------------------------------------------------------
void FASTCALL CMixBufWnd::Setup(int x, int y, int width, int height, BYTE *ptr)
{
	int i;
	int delta;
	int below;
	const DWORD *p;

	// x, y Comprobar.
	if (x >= m_pRendWork->mixwidth) {
		// No hay zona de visualizacion. Todo negro.
		for (i=0; i<height; i++) {
			memset(ptr, 0, (width << 2));
			ptr += (width << 2);
		}
		return;
	}
	if (y >= m_pRendWork->mixheight) {
		//No hay zona de visualizacion. Todo negro.
		for (i=0; i<height; i++) {
			memset(ptr, 0, (width << 2));
			ptr += (width << 2);
		}
		return;
	}

	// calculo de punteros
	p = m_pRendWork->mixbuf;
	ASSERT(p);
	p += (y * m_pRendWork->mixwidth);
	p += x;

	//medida contra la sobrecarga
	below = 0;
	if ((y + height) > m_pRendWork->mixheight) {
		below = height - m_pRendWork->mixheight + y;
		height = m_pRendWork->mixheight - y;
	}
	delta = 0;
	if ((x + width) > m_pRendWork->mixwidth) {
		delta = width - m_pRendWork->mixwidth + x;
		width = m_pRendWork->mixwidth - x;
	}

	// bucle
	for (i=0; i<height; i++) {
		// x, width繧貞鋸譯医＠縺ｦ繧ｳ繝斐・
		memcpy(ptr, p, (width << 2));
		p += m_pRendWork->mixwidth;
		ptr += (width << 2);
		memset(ptr, 0, (delta << 2));
		ptr += (delta << 2);
	}

	// Borrar la direccion descendente extrana.
	for (i=0; i<below; i++) {
		memset(ptr, 0, (width << 2));
		ptr += (width << 2);
		memset(ptr, 0, (delta << 2));
		ptr += (delta << 2);
	}
}

//---------------------------------------------------------------------------
//
//	Actualizacion del hilo de mensajes
//
//---------------------------------------------------------------------------
void FASTCALL CMixBufWnd::Update()
{
	int x;
	int y;
	DWORD rgb;
	CString string;

	// Comprobacion de la ventana BMP
	if (!m_pBMPWnd) {
		return;
	}

	// Comprobacion del cursor del raton
	if ((m_pBMPWnd->m_nCursorX < 0) || (m_pBMPWnd->m_nCursorY < 0)) {
		m_StatusBar.SetPaneText(0, "");
		return;
	}

	// Calculos de coordenadas, sobrecomprobacion
	x = m_pBMPWnd->m_nCursorX + m_pBMPWnd->m_nScrlX;
	y = m_pBMPWnd->m_nCursorY + m_pBMPWnd->m_nScrlY;
	if (x >= m_nScrlWidth) {
		return;
	}
	if (y >= m_nScrlHeight) {
		return;
	}

	// Creacion de datos de visualizacion
	if (x >= m_pRendWork->mixwidth) {
		return;
	}
	if (y >= m_pRendWork->mixheight) {
		return;
	}
	rgb = m_pRendWork->mixbuf[(y * m_pRendWork->mixwidth) + x];

	// mostrar
	string.Format("( %d, %d) R%d G%d B%d  Width: %d Height: %d",
				x, y, (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, (rgb & 0xff), m_pRendWork->mixwidth, m_pRendWork->mixheight);
	m_StatusBar.SetPaneText(0, string);
}

//===========================================================================
//
//	PCG繝舌ャ繝輔ぃ繧ｦ繧｣繝ｳ繝峨え
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	繧ｳ繝ｳ繧ｹ繝医Λ繧ｯ繧ｿ
//
//---------------------------------------------------------------------------
CPCGBufWnd::CPCGBufWnd()
{
	Render *render;
	const Render::render_t *p;

	// 蝓ｺ譛ｬ繝代Λ繝｡繝ｼ繧ｿ
	m_nWidth = 28;
	m_nHeight = 16;
	m_nScrlWidth = 256;
	m_nScrlHeight = 4096;
	m_dwID = MAKEID('P', 'C', 'G', 'B');
	::GetMsg(IDS_SWND_REND_PCG, m_strCaption);

	// 繝ｬ繝ｳ繝繝ｩ蜿門ｾ・
	render = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// 繧｢繝峨Ξ繧ｹ蜿門ｾ・
	m_pPCGBuf = render->GetPCGBuf();
	ASSERT(m_pPCGBuf);
	p = render->GetWorkAddr();
	ASSERT(p);
	m_dwPCGBuf = p->pcguse; 
	ASSERT(m_dwPCGBuf);
}

//---------------------------------------------------------------------------
//
//	繧ｻ繝・ヨ繧｢繝・・
//
//---------------------------------------------------------------------------
void FASTCALL CPCGBufWnd::Setup(int x, int y, int width, int height, BYTE *ptr)
{
	int i;
	int j;
	int delta;
	const DWORD *p;
	DWORD buf[256];

	// x繝√ぉ繝・け
	if (x >= 256) {
		// 陦ｨ遉ｺ鬆伜沺縺ｪ縺励ゅ☆縺ｹ縺ｦ鮟・
		for (i=0; i<height; i++) {
			memset(ptr, 0, (width << 2));
			ptr += (width << 2);
		}
		return;
	}

	// 繧ｪ繝ｼ繝舌・蟇ｾ遲・
	delta = 0;
	if ((x + width) > 256) {
		delta = width - 256 + x;
		width = 256 - x;
	}

	// 繝ｫ繝ｼ繝・
	for (i=0; i<height; i++) {
		// 繝舌ャ繝輔ぃ繝昴う繝ｳ繧ｿ邂怜・
		p = m_pPCGBuf;
		ASSERT((y >> 4) < 256);
		p += ((y >> 4) << 8);
		p += ((y & 0x0f) << 4);

		// 繝・・繧ｿ菴懈・
		memset(buf, 0, sizeof(buf));
		for (j=0; j<16; j++) {
			memcpy(&buf[j << 4], p, sizeof(DWORD) * 16);

			// 繝舌ャ繝輔ぃ繧・56x16x16縺縺代∝・縺ｸ騾ｲ繧√ｋ
			p += 0x10000;
		}

		// x, width繧貞鋸譯医＠縺ｦ繧ｳ繝斐・
		memcpy(ptr, buf, (width << 2));
		ptr += (width << 2);
		memset(ptr, 0, (delta << 2));
		ptr += (delta << 2);

		y++;
	}
}

//---------------------------------------------------------------------------
//
//	繝｡繝・そ繝ｼ繧ｸ繧ｹ繝ｬ繝・ラ縺九ｉ縺ｮ譖ｴ譁ｰ
//
//---------------------------------------------------------------------------
void FASTCALL CPCGBufWnd::Update()
{
	int x;
	int y;
	CString string;
	int index;

	// BMP繧ｦ繧｣繝ｳ繝峨え繝√ぉ繝・け
	if (!m_pBMPWnd) {
		return;
	}

	// 繝槭え繧ｹ繧ｫ繝ｼ繧ｽ繝ｫ繝√ぉ繝・け
	if ((m_pBMPWnd->m_nCursorX < 0) || (m_pBMPWnd->m_nCursorY < 0)) {
		m_StatusBar.SetPaneText(0, "");
		return;
	}

	// 蠎ｧ讓呵ｨ育ｮ励√が繝ｼ繝舌・繝√ぉ繝・け
	x = m_pBMPWnd->m_nCursorX + m_pBMPWnd->m_nScrlX;
	y = m_pBMPWnd->m_nCursorY + m_pBMPWnd->m_nScrlY;
	if (x >= m_nScrlWidth) {
		return;
	}
	if (y >= m_nScrlHeight) {
		return;
	}

	// 繧､繝ｳ繝・ャ繧ｯ繧ｹ菴懈・
	index = y >> 4;

	// 陦ｨ遉ｺ
	string.Format("( %d, %d) Pal%1X [$%02X +%d +%d]",
				x, y, (x >> 4), index, (x & 0x0f), (y & 0x0f));
	m_StatusBar.SetPaneText(0, string);
}

#endif	// _WIN32
