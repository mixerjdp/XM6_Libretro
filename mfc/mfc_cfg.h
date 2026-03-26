//---------------------------------------------------------------------------
//
//	EMULADOR X68000 "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[MFC configuration]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_cfg_h)
#define mfc_cfg_h

#include "config.h"
#include "ppi.h"

//===========================================================================
//
//	Configuration
//
//===========================================================================
class CConfig : public CComponent
{
public:
	// Funciones basicas
	CConfig(CFrmWnd *pWnd);
										// Constructor
	BOOL FASTCALL Init();
	BOOL FASTCALL CustomInit(BOOL ArchivoDefault); // Custom configuration setup
										// Initialization
	void FASTCALL Cleanup();
	void FASTCALL Cleanup2();
										// Limpieza (Cleanup)

	// Data de configuracion (global)
	void FASTCALL GetConfig(Config *pConfigBuf) const;
										// Get datos de configuracion
	void FASTCALL SetConfig(Config *pConfigBuf);
										// Set datos de configuracion

	// Data de configuracion (individuales)
	void FASTCALL SetStretch(BOOL bStretch);
										// Screen scaling configuration
	void FASTCALL SetMIDIDevice(int nDevice, BOOL bIn);
										// MIDI device configuration

	// MRU
	void FASTCALL SetMRUFile(int nType, LPCTSTR pszFile);
										// MRU file configuration (most recent)
	void FASTCALL GetMRUFile(int nType, int nIndex, LPTSTR pszFile) const;
										// Get archivo MRU
	int FASTCALL GetMRUNum(int nType) const;
										// Get numero de archivos MRU

	// Save / Load
	BOOL FASTCALL Save(Fileio *pFio, int nVer);
										// Save
	BOOL FASTCALL Load(Fileio *pFio, int nVer);
										// Load
	BOOL FASTCALL IsApply();
										// ?Aplicar?

private:
	// Data de configuracion
	typedef struct _INIKEY {
		void *pBuf;						// Pointer to the INI value buffer
		LPCTSTR pszSection;				// Section name
		LPCTSTR pszKey;					// Key name
		int nType;						// Type
		int nDef;						// Default value
		int nMin;						// Minimum value (for some types only)
		int nMax;						// Maximum value (for some types only)
	} INIKEY, *PINIKEY;

	// File INI
	TCHAR m_IniFile[FILEPATH_MAX];
										// Nombre del archivo INI

	// Data de configuracion
	void FASTCALL LoadConfig();
										// Load datos de configuracion
	void FASTCALL SaveConfig() const;
										// Save datos de configuracion
	static const INIKEY IniTable[];
										// Tabla INI de datos de configuracion
	static Config m_Config;
										// Data de configuracion

	// �o�[�W�����݊�
	void FASTCALL ResetSASI();
										// Reconfiguracion SASI
	void FASTCALL ResetCDROM();
										// Reconfiguracion CD-ROM
	static BOOL m_bCDROM;
										// CD-ROM activado

	// MRU
	enum {
		MruTypes=5					// Number de tipos MRU
	};
	void FASTCALL ClearMRU(int nType);
										// Limpiar MRU
	void FASTCALL LoadMRU(int nType);
										// Load MRU
	void FASTCALL SaveMRU(int nType) const;
										// Save MRU
	TCHAR m_MRUFile[MruTypes][9][FILEPATH_MAX];
										// Files MRU
	int m_MRUNum[MruTypes];
										// Number de MRU

	// �L�[
	void FASTCALL LoadKey() const;
										// Load clave
	void FASTCALL SaveKey() const;
										// Save clave

	// TrueKey
	void FASTCALL LoadTKey() const;
										// Load TrueKey
	void FASTCALL SaveTKey() const;
										// Save TrueKey

	// �o�[�W�����݊�
	BOOL FASTCALL Load200(Fileio *pFio);
										// version 2.00 o version 2.01
	BOOL FASTCALL Load202(Fileio *pFio);
										// version 2.02 o version 2.03

	// Load�ESave
	BOOL m_bApply;
										// Load��ApplyCfg��v��
};

//---------------------------------------------------------------------------
//
//	Pre-definicion de clase
//
//---------------------------------------------------------------------------
class CConfigSheet;

//===========================================================================
//
//	ConfigurationPage de propiedades
//
//===========================================================================
class CConfigPage : public CPropertyPage
{
public:
	CConfigPage();
										// Constructor
	void FASTCALL Init(CConfigSheet *pSheet);
										// Initialization
	virtual BOOL OnInitDialog();
										// Initialization del dialogo
	virtual BOOL OnSetActive();
										// Page activa
	DWORD FASTCALL GetID() const		{ return m_dwID; }
										// Get ID

protected:
	afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT nMsg);
										// Mouse cursor configuration
	Config *m_pConfig;
										// Data de configuracion
	DWORD m_dwID;
										// ID de la pagina
	int m_nTemplate;
										// ID de plantilla
	UINT m_uHelpID;
										// ID de ayuda
	UINT m_uMsgID;
										// ID de mensaje de ayuda
	CConfigSheet *m_pSheet;
										// Property sheet

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Page basica
//
//===========================================================================
class CBasicPage : public CConfigPage
{
public:
	CBasicPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// Aceptar

protected:
	afx_msg void OnMPUFull();
										// Velocidad completa MPU

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Page de sonido
//
//===========================================================================
class CSoundPage : public CConfigPage
{
public:
	CSoundPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// Aceptar

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Desplazamiento vertical
	afx_msg void OnSelChange();
										// Cambio en el cuadro combinado

private:
	void FASTCALL EnableControls(BOOL bEnable);
										// Cambio de estado de los controles
	BOOL m_bEnableCtrl;
										// State de los controles
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Page de volumen
//
//===========================================================================
class CVolPage : public CConfigPage
{
public:
	CVolPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// Aceptar
	void OnCancel();
										// Cancelar

protected:
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Desplazamiento horizontal
	afx_msg void OnFMCheck();
										// Sintetizador FMCheck
	afx_msg void OnADPCMCheck();
										// Sintetizador ADPCMCheck
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
#else
	afx_msg void OnTimer(UINT nTimerID);
#endif
										// Timer

private:
	CSound *m_pSound;
										// Sonido
	OPMIF *m_pOPMIF;
										// Interfaz OPM
	ADPCM *m_pADPCM;
										// ADPCM
	CMIDI *m_pMIDI;
										// MIDI
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
#else
	UINT m_nTimerID;
#endif
										// ID del temporizador
	int m_nMasterVol;
										// Volumen maestro
	int m_nMasterOrg;
										// Volumen maestro original
	int m_nMIDIVol;
										// Volumen MIDI
	int m_nMIDIOrg;
										// Volumen MIDI original

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Page del teclado
//
//===========================================================================
class CKbdPage : public CConfigPage
{
public:
	CKbdPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// Aceptar
	void OnCancel();
										// Cancelar

protected:
	afx_msg void OnEdit();
										// Editar
	afx_msg void OnDefault();
										// Por defecto
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic en columna
	afx_msg void OnRClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic derecho en columna
	afx_msg void OnConnect();
										// Conexion

private:
	void FASTCALL UpdateReport();
										// ���|�[�gActualizacion
	void FASTCALL EnableControls(BOOL bEnable);
										// Cambio de estado de los controles
	DWORD m_dwEdit[0x100];
										// Edicion
	DWORD m_dwBackup[0x100];
										// Respaldo / Backup
	CInput *m_pInput;
										// CInput
	BOOL m_bEnableCtrl;
										// State de los controles
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Dialogo de edicion de mapa de teclado
//
//===========================================================================
class CKbdMapDlg : public CDialog
{
public:
	CKbdMapDlg(CWnd *pParent, DWORD *pMap);
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	void OnCancel();
										// Cancelar

protected:
	afx_msg void OnPaint();
										// �_�C�A���ODibujar
	afx_msg LONG OnKickIdle(UINT uParam, LONG lParam);
										// Proceso idle
	afx_msg LONG OnApp(UINT uParam, LONG lParam);
										// ���[�U(Notificacion de ventanas subordinadas)

private:
	void FASTCALL OnDraw(CDC *pDC);
										// Sub-rutina de dibujo
	CRect m_rectStat;
										// Rectangulo de estado
	CString m_strStat;
										// Mensaje de estado
	CString m_strGuide;
										// Mensaje guia
	CWnd *m_pDispWnd;
										// CKeyDispWnd
	CInput *m_pInput;
										// CInput
	DWORD *m_pEditMap;
										// Editar���̃}�b�v

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Dialogo de entrada de teclas
//
//===========================================================================
class CKeyinDlg : public CDialog
{
public:
	CKeyinDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	UINT m_nTarget;
										// Key objetivo
	UINT m_nKey;
										// Asignacion�L�[

protected:
	afx_msg void OnPaint();
										// Dibujar
	afx_msg UINT OnGetDlgCode();
										// �_�C�A���O�R�[�hGet
	afx_msg LONG OnKickIdle(UINT uParam, LONG lParam);
										// Proceso idle
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
										// Clic derecho

private:
	void FASTCALL OnDraw(CDC *pDC);
										// Sub-rutina de dibujo
	CInput *m_pInput;
										// CInput
	BOOL m_bKey[0x100];
										// Para memorizar teclas
	CRect m_GuideRect;
										// Rectangulo guia
	CString m_GuideString;
										// String guia
	CRect m_AssignRect;
										// �L�[Asignacion��`
	CString m_AssignString;
										// �L�[Asignacion������
	CRect m_KeyRect;
										// Rectangulo de tecla
	CString m_KeyString;
										// String de tecla

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Page del mouse
//
//===========================================================================
class CMousePage : public CConfigPage
{
public:
	CMousePage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// Aceptar

protected:
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Desplazamiento horizontal
	afx_msg void OnPort();
										// Seleccion de puerto

private:
	void FASTCALL EnableControls(BOOL bEnable);
										// Cambio de estado de los controles
	BOOL m_bEnableCtrl;
										// State de los controles
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Page del joystick
//
//===========================================================================
class CJoyPage : public CConfigPage
{
public:
	CJoyPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// Aceptar
	void OnCancel();
										// Cancelar

protected:
	BOOL OnCommand(WPARAM wParam, LPARAM lParam);
										// Notificacion de comando

private:
	void FASTCALL OnSelChg(CComboBox* pComboBox);
										// Cambio en el cuadro combinado
	void FASTCALL OnDetail(UINT nButton);
										// Detalles
	void FASTCALL OnSetting(UINT nButton);
										// Configuration
	CButton* GetCorButton(UINT nComboBox);
										// �Ή�ButtonsGet
	CComboBox* GetCorCombo(UINT nButton);
										// �Ή�Cuadro combinadoGet
	CInput *m_pInput;
										// CInput
	static UINT ControlTable[];
										// Tabla de controles
};

//===========================================================================
//
//	�W���C�X�e�B�b�NDialogo de detalles
//
//===========================================================================
class CJoyDetDlg : public CDialog
{
public:
	CJoyDetDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Initialization

	CString m_strDesc;
										// �f�o�C�XNombre
	int m_nPort;
										// Number de puerto (0 o 1)
	int m_nType;
										// Tipo (0-12)
};

//===========================================================================
//
//	ButtonsConfiguration�y�[�W
//
//===========================================================================
class CBtnSetPage : public CPropertyPage
{
public:
	CBtnSetPage();
										// Constructor
	void FASTCALL Init(CPropertySheet *pSheet);
										// Create
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// Aceptar
	void OnCancel();
										// Cancelar
	int m_nJoy;
										// Number de joystick (0 o 1)
	int m_nType[PPI::PortMax];
										// �W���C�X�e�B�b�NTipo (0-12)

protected:
	afx_msg void OnPaint();
										// �_�C�A���ODibujar
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
#else
	afx_msg void OnTimer(UINT nTimerID);
#endif
										// Timer
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Desplazamiento horizontal
	BOOL OnCommand(WPARAM wParam, LPARAM lParam);
										// Notificacion de comando

private:
	enum CtrlType {
		BtnLabel,						// Etiqueta(Buttonsn)
		BtnCombo,						// Cuadro combinado
		BtnRapid,						// Rapid-fire�X���C�_
		BtnValue						// Rapid-fireEtiqueta
	};
	void FASTCALL OnDraw(CDC *pDC, BOOL *pButton, BOOL bForce);
										// Dibujo principal
	void FASTCALL OnSlider(int nButton);
										// �X���C�_Modificacion
	void FASTCALL OnSelChg(int nButton);
										// Cambio en el cuadro combinado
	void FASTCALL GetButtonDesc(const char *pszDesc, CString &strDesc);
										// ButtonsMostrarGet
	UINT FASTCALL GetControl(int nButton, CtrlType ctlType) const;
										// �R���g���[��Get ID
	CPropertySheet *m_pSheet;
										// Hoja padre
	CInput *m_pInput;
										// CInput
	CRect m_rectLabel[12];
										// Etiqueta�ʒu
	BOOL m_bButton[12];
										// Buttons�����L��
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
#else
	UINT m_nTimerID;
#endif
										// ID del temporizador
	static const UINT ControlTable[];
										// Tabla de controles
	static const int RapidTable[];
										// Rapid-fire�e�[�u��

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	�W���C�X�e�B�b�NProperty sheet
//
//===========================================================================
class CJoySheet : public CPropertySheet
{
public:
	CJoySheet(CWnd *pParent);
										// Constructor
	void FASTCALL SetParam(int nJoy, int nCombo, int nType[]);
										// �p�����[�^Configuration
	void FASTCALL InitSheet();
										// �V�[�gInitialization
	int FASTCALL GetAxes() const;
										// Number de ejesGet
	int FASTCALL GetButtons() const;
										// Buttons��Get

private:
	CBtnSetPage m_BtnSet;
										// ButtonsConfiguration�y�[�W
	CInput *m_pInput;
										// CInput
	int m_nJoy;
										// Number de joystick (0 o 1)
	int m_nCombo;
										// Cuadro combinado�I��
	int m_nType[PPI::PortMax];
										// Seleccion de tipo del lado VM
	DIDEVCAPS m_DevCaps;
										// Caps del dispositivo
};

//===========================================================================
//
//	Page SASI
//
//===========================================================================
class CSASIPage : public CConfigPage
{
public:
	CSASIPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// Aceptar
	BOOL OnSetActive();
										// Page activa
	int FASTCALL GetDrives(const Config *pConfig) const;
										// SASINumber of drivesGet

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Desplazamiento vertical
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic en columna

private:
	void FASTCALL UpdateList();
										// Update control de lista
	void FASTCALL CheckSASI(DWORD *pDisk);
										// SASI�t�@�C��Check
	void FASTCALL EnableControls(BOOL bEnable, BOOL bDrive = TRUE);
										// Cambio de estado de los controles
	SASI *m_pSASI;
										// SASI
	BOOL m_bInit;
										// Flag de inicializacion
	int m_nDrives;
										// Number of drives
	TCHAR m_szFile[16][FILEPATH_MAX];
										// Nombre del archivo de disco duro SASI
	CString m_strError;
										// String de error

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Page SxSI
//
//===========================================================================
class CSxSIPage : public CConfigPage
{
public:
	CSxSIPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	BOOL OnSetActive();
										// Page activa
	void OnOK();
										// Aceptar
	int FASTCALL GetDrives(const Config *pConfig) const;
										// SxSINumber of drivesGet

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Desplazamiento vertical
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic en columna
	afx_msg void OnCheck();
										// Check�{�b�N�X�N���b�N

private:
	enum DevType {
		DevSASI,						// Disco duro SASI�h���C�u
		DevSCSI,						// Disco duro SCSI�h���C�u
		DevMO,							// Unit MO SCSI
		DevInit,						// SCSI Iniciador (Host)
		DevNone							// Sin dispositivo
	};
	void FASTCALL UpdateList();
										// Update control de lista
	void FASTCALL BuildMap();
										// Mapa de dispositivosCreate
	int FASTCALL CheckSCSI(int nDrive);
										// Dispositivo SCSICheck
	void FASTCALL EnableControls(BOOL bEnable, BOOL bDrive = TRUE);
										// Cambio de estado de los controles
	BOOL m_bInit;
										// Flag de inicializacion
	int m_nSASIDrives;
										// SASINumber of drives
	DevType m_DevMap[8];
										// Mapa de dispositivos
	TCHAR m_szFile[6][FILEPATH_MAX];
										// Nombre del archivo de disco duro SCSI
	CString m_strSASI;
										// String HD SASI
	CString m_strMO;
										// String MO SCSI
	CString m_strInit;
										// String del iniciador
	CString m_strNone;
										// String que indica n/a
	CString m_strError;
										// �f�o�C�XString de error
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Page SCSI
//
//===========================================================================
class CSCSIPage : public CConfigPage
{
public:
	CSCSIPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// Aceptar
	int FASTCALL GetInterface(const Config *pConfig) const;
										// Tipo de interfazGet

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Desplazamiento vertical
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic en columna
	afx_msg void OnButton();
										// ���W�IButtons�I��
	afx_msg void OnCheck();
										// Check�{�b�N�X�N���b�N

private:
	enum DevType {
		DevSCSI,						// Disco duro SCSI�h���C�u
		DevMO,							// Unit MO SCSI
		DevCD,							// CD-ROM SCSI�h���C�u
		DevInit,						// SCSI Iniciador (Host)
		DevNone							// Sin dispositivo
	};
	int FASTCALL GetIfCtrl() const;
										// Tipo de interfazGet(�R���g���[�����)
	BOOL FASTCALL CheckROM(int nType) const;
										// ROMCheck
	void FASTCALL UpdateList();
										// Update control de lista
	void FASTCALL BuildMap();
										// Mapa de dispositivosCreate
	int FASTCALL CheckSCSI(int nDrive);
										// Dispositivo SCSICheck
	void FASTCALL EnableControls(BOOL bEnable, BOOL bDrive = TRUE);
										// Cambio de estado de los controles
	SCSI *m_pSCSI;
										// Dispositivo SCSI
	BOOL m_bInit;
										// Flag de inicializacion
	int m_nDrives;
										// Number of drives
	BOOL m_bMOFirst;
										// Flag de primero MO
	DevType m_DevMap[8];
										// Mapa de dispositivos
	TCHAR m_szFile[5][FILEPATH_MAX];
										// Nombre del archivo de disco duro SCSI
	CString m_strMO;
										// String MO SCSI
	CString m_strCD;
										// String CD SCSI
	CString m_strInit;
										// String del iniciador
	CString m_strNone;
										// String que indica n/a
	CString m_strError;
										// �f�o�C�XString de error
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Page de puertos
//
//===========================================================================
class CPortPage : public CConfigPage
{
public:
	CPortPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// Aceptar
};

//===========================================================================
//
//	Page MIDI
//
//===========================================================================
class CMIDIPage : public CConfigPage
{
public:
	CMIDIPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// Aceptar
	void OnCancel();
										// Cancelar

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Desplazamiento vertical
	afx_msg void OnBIDClick();
										// Clic en ID de placa

private:
	void FASTCALL EnableControls(BOOL bEnable);
										// Cambio de estado de los controles
	CMIDI *m_pMIDI;
										// Componente MIDI
	BOOL m_bEnableCtrl;
										// State de los controles
	int m_nInDelay;
										// Retraso de entrada (ms)
	int m_nOutDelay;
										// Retraso de salida (ms)
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Page de modificaciones
//
//===========================================================================
class CAlterPage : public CConfigPage
{
public:
	CAlterPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	BOOL OnKillActive();
										// Moverse de pagina
	BOOL FASTCALL HasParity(const Config *pConfig) const;
										// SASI�p���e�B�@�\Check

protected:
	void DoDataExchange(CDataExchange *pDX);
										// Intercambio de datos

private:
	BOOL m_bInit;
										// Initialization
	BOOL m_bParity;
										// Con paridad
};

//===========================================================================
//
//	Page de reanudacion (Resume)
//
//===========================================================================
class CResumePage : public CConfigPage
{
public:
	CResumePage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization

protected:
	void DoDataExchange(CDataExchange *pDX);
										// Intercambio de datos
};

//===========================================================================
//
//	TrueKeyDialogo de entrada
//
//===========================================================================
class CTKeyDlg : public CDialog
{
public:
	CTKeyDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	void OnCancel();
										// Cancelar
	int m_nTarget;
										// Key objetivo
	int m_nKey;
										// Asignacion�L�[

protected:
	afx_msg void OnPaint();
										// Dibujar
	afx_msg UINT OnGetDlgCode();
										// �_�C�A���O�R�[�hGet
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
										// Timer
#else
	afx_msg void OnTimer(UINT nTimerID);
										// Timer
#endif
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
										// Clic derecho

private:
	void FASTCALL OnDraw(CDC *pDC);
										// Dibujo principal
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
										// ID del temporizador
#else
	UINT m_nTimerID;
										// ID del temporizador
#endif
	BYTE m_KeyState[0x100];
										// State de teclas VK
	CTKey *m_pTKey;
										// TrueKey
	CRect m_rectGuide;
										// Rectangulo guia
	CString m_strGuide;
										// String guia
	CRect m_rectAssign;
										// �L�[Asignacion��`
	CString m_strAssign;
										// �L�[Asignacion������
	CRect m_rectKey;
										// Rectangulo de tecla
	CString m_strKey;
										// String de tecla

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Page TrueKey
//
//===========================================================================
class CTKeyPage : public CConfigPage
{
public:
	CTKeyPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// Aceptar

protected:
	afx_msg void OnSelChange();
										// Cambio en el cuadro combinado
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic en columna
	afx_msg void OnRClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Clic derecho en columna

private:
	void FASTCALL EnableControls(BOOL bEnable);
										// Cambio de estado de los controles
	void FASTCALL UpdateReport();
										// ���|�[�gActualizacion
	BOOL m_bEnableCtrl;
										// Flag de activacion de controles
	CInput *m_pInput;
										// CInput
	CTKey *m_pTKey;
										// TrueKey
	int m_nKey[0x73];
										// Editar����Conversion�e�[�u��
	static const UINT ControlTable[];
										// Tabla de controles

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Page de miscelanea
//
//===========================================================================
class CMiscPage : public CConfigPage
{
public:
	CMiscPage();
	BOOL OnInitDialog();
	void OnBuscarFolder();
	void OnOK();
	void DoDataExchange(CDataExchange *pDX);
										// Intercambio de datos

	int m_nRendererDefault;

DECLARE_MESSAGE_MAP()
};

//===========================================================================
//
//	ConfigurationProperty sheet
//
//===========================================================================
class CConfigSheet : public CPropertySheet
{
public:
	CConfigSheet(CWnd *pParent);
										// Constructor
	Config *m_pConfig;
										// Data de configuracion
	CConfigPage* FASTCALL SearchPage(DWORD dwID) const;
										// �y�[�WBusqueda

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// �E�B���h�ECreate
	afx_msg void OnDestroy();
										// �E�B���h�EDelete
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
#else
	afx_msg void OnTimer(UINT nTimerID);
#endif
										// Timer

private:
	CFrmWnd *m_pFrmWnd;
										// Window of marco (Frame window)
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
#else
	UINT m_nTimerID;
#endif
										// ID del temporizador

	CBasicPage m_Basic;
										// Basico
	CSoundPage m_Sound;
										// Sonido
	CVolPage m_Vol;
										// Volumen
	CKbdPage m_Kbd;
										// Keyboard
	CMousePage m_Mouse;
										// Mouse
	CJoyPage m_Joy;
										// Joystick
	CSASIPage m_SASI;
										// SASI
	CSxSIPage m_SxSI;
										// SxSI
	CSCSIPage m_SCSI;
										// SCSI
	CPortPage m_Port;
										// Puertos
	CMIDIPage m_MIDI;
										// MIDI
	CAlterPage m_Alter;
										// ����
	CResumePage m_Resume;
										// ���W���[��
	CTKeyPage m_TKey;
										// TrueKey
	CMiscPage m_Misc;
										// Otros

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

#endif	// mfc_cfg_h
#endif	// _WIN32
