//---------------------------------------------------------------------------
//
//	EMULADOR X68000 "XM6"
//
//	Copyright (C) 2001-2005 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ Subventana MFC (Win32) ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_w32_h)
#define mfc_w32_h

#include "keyboard.h"
#include "mfc_sub.h"
#include "mfc_midi.h"

//===========================================================================
//
//	Window of Componentes
//
//===========================================================================
class CComponentWnd : public CSubTextWnd
{
public:
	CComponentWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration

private:
	CComponent *m_pComponent;
										// Primer componente
};

//===========================================================================
//
//	Window of información del SO
//
//===========================================================================
class COSInfoWnd : public CSubTextWnd
{
public:
	COSInfoWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration
};

//===========================================================================
//
//	Window of sonido
//
//===========================================================================
class CSoundWnd : public CSubTextWnd
{
public:
	CSoundWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration

private:
	Scheduler *m_pScheduler;
										// Planificador
	OPMIF *m_pOPMIF;
										// OPM
	ADPCM *m_pADPCM;
										// ADPCM
	CSound *m_pSound;
										// Componente de sonido
};

//===========================================================================
//
//	Window of entrada
//
//===========================================================================
class CInputWnd : public CSubTextWnd
{
public:
	CInputWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration

private:
	void FASTCALL SetupInput(int x, int y);
                                        // Configuration (full input system)
	void FASTCALL SetupMouse(int x, int y);
                                        // Configuration (mouse)
	void FASTCALL SetupKey(int x, int y);
                                        // Configuration (keyboard)
	void FASTCALL SetupJoy(int x, int y, int nJoy);
                                        // Configuration (joystick)
	CInput *m_pInput;
										// Componente de entrada
};

//===========================================================================
//
//	Window of puertos
//
//===========================================================================
class CPortWnd : public CSubTextWnd
{
public:
	CPortWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration

private:
	CPort *m_pPort;
										// Componente de puerto
};

//===========================================================================
//
//	Window of mapa de bits
//
//===========================================================================
class CBitmapWnd : public CSubTextWnd
{
public:
	CBitmapWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration

private:
	CDrawView *m_pView;
										// Window of dibujo
};

//===========================================================================
//
//	Window ofl controlador MIDI
//
//===========================================================================
class CMIDIDrvWnd : public CSubTextWnd
{
public:
	CMIDIDrvWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration

private:
	void FASTCALL SetupInfo(int x, int y, CMIDI::LPMIDIINFO pInfo);
                                        // Configuration (sub)
	void FASTCALL SetupExCnt(int x, int y, DWORD dwStart, DWORD dwEnd);
                                        // Configuration (exclusive counter)
	static LPCTSTR DescTable[];
										// Tabla de cadenas
	MIDI *m_pMIDI;
										// MIDI
	CMIDI *m_pMIDIDrv;
										// Controller MIDI
};

//===========================================================================
//
//  Keyboard display window
//
//===========================================================================
class CKeyDispWnd : public CWnd
{
public:
	CKeyDispWnd();
										// Constructor
	void PostNcDestroy();
                                        // Window destruction completed
	void FASTCALL SetShiftMode(UINT nMode);
                                        // Shift mode configuration
	void FASTCALL Refresh(const BOOL *m_pKeyBuf);
                                        // Key update
	void FASTCALL SetKey(const BOOL *m_pKeyBuf);
                                        // Batch key configuration

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
                                        // Window creation
	afx_msg void OnDestroy(void);
                                        // Window destruction
	afx_msg void OnSize(UINT nType, int cx, int cy);
                                        // Window resize
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
										// Dibujo de fondo
	afx_msg void OnPaint();
                                        // Redraw window
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
										// Botón izquierdo presionado
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
										// Botón izquierdo soltado
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
										// Botón derecho presionado
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
										// Botón derecho soltado
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
                                        // Mouse movement
	afx_msg UINT OnGetDlgCode();
										// Obtención de código de diálogo

private:
	void FASTCALL SetupBitmap();
										// Preparación de mapa de bits
	void FASTCALL OnDraw(CDC *pDC);
                                        // Window drawing
	LPCTSTR FASTCALL GetKeyString(int nKey);
                                        // Get key string
	int FASTCALL PtInKey(CPoint& point);
                                        // Get key code in rectangle
	void FASTCALL DrawKey(int nKey, BOOL bDown);
                                        // Key display
	void FASTCALL DrawBox(int nColorOut, int nColorIn, RECT& rect);
                                        // Key box drawing
	void FASTCALL DrawCRBox(int nColorOut, int nColorIn, RECT& rect);
                                        // Key box drawing CR
	void FASTCALL DrawChar(int x, int y, int nColor, DWORD dwChar);
										// Dibujo de carácter
	void FASTCALL DrawCRChar(int x, int y, int nColor);
										// Dibujo de carácter CR
	int FASTCALL CalcCGAddr(DWORD dwChar);
										// Cálculo de dirección CGROM de ancho completo
	UINT m_nMode;
										// Modo SHIFT
	UINT m_nKey[0x80];
                                        // Key state (display)
	BOOL m_bKey[0x80];
                                        // Key state (final)
	int m_nPoint;
                                        // Mouse cursor point
	const BYTE* m_pCG;
										// CGROM
	HBITMAP m_hBitmap;
										// Manejador de mapa de bits
	BYTE *m_pBits;
										// Bits de mapa de bits
	UINT m_nBMPWidth;
										// Ancho de mapa de bits
	UINT m_nBMPHeight;
										// Altura de mapa de bits
	UINT m_nBMPMul;
										// Ancho de multiplicación de mapa de bits
	static RGBQUAD PalTable[0x10];
										// Tabla de paleta
	static const RECT RectTable[0x75];
										// Tabla de rectángulos
	static LPCTSTR NormalTable[];
										// Tabla de cadenas
	static LPCTSTR KanaTable[];
										// Tabla de cadenas
	static LPCTSTR KanaShiftTable[];
										// Tabla de cadenas
	static LPCTSTR MarkTable[];
										// Tabla de cadenas
	static LPCTSTR AnotherTable[];
										// Tabla de cadenas

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//  Software keyboard window
//
//===========================================================================
class CSoftKeyWnd : public CSubWnd
{
public:
	CSoftKeyWnd();
										// Constructor
	void FASTCALL Refresh();
										// Update

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
                                        // Window creation
	afx_msg void OnDestroy();
                                        // Window destruction
	afx_msg void OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized);
										// Activar
	afx_msg LONG OnApp(UINT uParam, LONG lParam);
                                        // User (lower window notification)

private:
	void FASTCALL Analyze(Keyboard::keyboard_t *pKbd);
                                        // Keyboard data analysis
	Keyboard *m_pKeyboard;
										// Keyboard
	CInput *m_pInput;
										// Entrada
	CStatusBar m_StatusBar;
                                        // Status bar
	CKeyDispWnd *m_pDispWnd;
//  Key display window
	UINT m_nSoftKey;
										// Key de software presionada

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

#endif	// mfc_w32_h
#endif	// _WIN32
