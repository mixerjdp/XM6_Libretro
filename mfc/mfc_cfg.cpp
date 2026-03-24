//---------------------------------------------------------------------------
//
//	EMULADOR X68000 "XM6"
//
//	Copyright (C) 2001-2006 ・ｽo・ｽh・ｽD(ytanaka@ipc-tokai.or.jp)
//	[ MFC Configuracion ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "memory.h"
#include "opmif.h"
#include "adpcm.h"
#include "config.h"
#include "render.h"
#include "sasi.h"
#include "scsi.h"
#include "disk.h"
#include "filepath.h"
#include "ppi.h"
#include "mfc_frm.h"
#include "mfc_com.h"
#include "mfc_res.h"
#include "mfc_snd.h"
#include "mfc_inp.h"
#include "mfc_midi.h"
#include "mfc_tkey.h"
#include "mfc_w32.h"
#include "mfc_info.h"
#include "mfc_cfg.h"

//===========================================================================
//
//	Configuracion
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CConfig::CConfig(CFrmWnd *pWnd) : CComponent(pWnd)
{
	// Parametros del componente
	m_dwID = MAKEID('C', 'F', 'G', ' ');
	m_strDesc = _T("Config Manager");
}

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL FASTCALL CConfig::Init()
{
	int i;
	Filepath path;

	ASSERT(this);

	// Clase basica
	if (!CComponent::Init()) {
		return FALSE;
	}

	// Determinacion de la ruta del archivo INI
	path.SetPath(_T("XM6.ini"));






	
	char szAppPath[MAX_PATH] = "";
	CString strAppDirectory;

	GetModuleFileName(NULL, szAppPath, MAX_PATH);

	// Extract directory
	strAppDirectory = szAppPath;
	strAppDirectory = strAppDirectory.Left(strAppDirectory.ReverseFind('\\'));
	

	CString sz; // Obtener nombre de archivo de ruta completa y Asignarla a NombreArchivoXM6
	sz.Format(_T("%s"), m_pFrmWnd->RutaCompletaArchivoXM6);
	m_pFrmWnd->NombreArchivoXM6 = sz.Mid(sz.ReverseFind('\\') + 1);

	// Remover Extension de nombre de archivo
	int nLen = m_pFrmWnd->NombreArchivoXM6.GetLength();	
	TCHAR lpszBuf[MAX_PATH];
	_tcscpy(lpszBuf, m_pFrmWnd->NombreArchivoXM6.GetBuffer(nLen));
	PathRemoveExtensionA(lpszBuf);
   //	OutputDebugString("\n\n NombreArchivoXm6:" + m_pFrmWnd->NombreArchivoXM6 + "\n\n");
   //   MessageBox(NULL, m_pFrmWnd->NombreArchivoXM6, "Xm6", 2);


	// Concatenar todo
	CString ArchivoSinExtension = lpszBuf;
	CString ArchivoAEncontrar = strAppDirectory + "\\" + ArchivoSinExtension + ".ini";

    
	
	// Verifica si existe archivo de ruta completa
	GetFileAttributes(ArchivoAEncontrar); // from winbase.h
	if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(ArchivoAEncontrar) && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		//MessageBox(NULL, "No se encontro " +  ArchivoAEncontrar, "Xm6", 2);
		path.SetBaseFile("XM6");
	}
	else
	{
		//MessageBox(NULL, "SI se encontro " + ArchivoAEncontrar, "Xm6", 2);
		path.SetBaseFile(ArchivoSinExtension);
	}
		
	_tcscpy(m_IniFile, path.GetPath());

	// Datos de configuracion -> Aqui se carga la configuraci・ｽn *-*
	LoadConfig();

		  




	
	// Aqui cargamos el parametro de linea de comandos si es HDF *-*	

	if (m_pFrmWnd->RutaCompletaArchivoXM6.GetLength() > 0) // Si RutaCompletaArchivoXM6 ya esta ocupado
	{
		CString str = m_pFrmWnd->RutaCompletaArchivoXM6;
		CString extensionArchivo = "";
	
		int curPos = 0;
		CString resToken = str.Tokenize(_T("."), curPos); // Obtiene extension de la ruta completa del archivo
		while (!resToken.IsEmpty())
		{			
			// Obtain next token
			extensionArchivo = resToken;
			resToken = str.Tokenize(_T("."), curPos);
		}

		//MessageBox(NULL, m_pFrmWnd->RutaCompletaArchivoXM6, "BBC", MB_OKCANCEL | MB_DEFBUTTON2);
		/* Si es hdf lo analiza y carga*/
		if (extensionArchivo.MakeUpper() == "HDF")
		{
			
			// Process resToken here - print, store etc
		    // int msgboxID = MessageBox(NULL, m_pFrmWnd->RutaCompletaArchivoXM6, "Xm6", 2);
			_tcscpy(m_Config.sasi_file[0], m_pFrmWnd->RutaCompletaArchivoXM6);
		}

	}










	// Mantener la compatibilidad
	ResetSASI();
	ResetCDROM();

	// MRU
	for (i=0; i<MruTypes; i++) {
		ClearMRU(i);
		LoadMRU(i);
	}

	// Clave
	LoadKey();

	// TrueKey
	LoadTKey();


	

	// Guardar y cargar
	m_bApply = FALSE;

	return TRUE;
}





BOOL FASTCALL CConfig::CustomInit(BOOL ArchivoDefault)
{	
	Filepath path;

	ASSERT(this);

	// Clase basica
	if (!CComponent::Init()) {
		return FALSE;
	}

	// Determinacion de la ruta del archivo INI
	path.SetPath(_T("XM6.ini"));

	// Obtener nombre archivo de juego actual y remover extensi・ｽn
	int nLen = m_pFrmWnd->NombreArchivoXM6.GetLength();
	TCHAR lpszBuf[MAX_PATH];
	_tcscpy(lpszBuf, m_pFrmWnd->NombreArchivoXM6.GetBuffer(nLen));
	PathRemoveExtensionA(lpszBuf);

	//int msgboxID = MessageBox(NULL, lpszBuf, "Xm6", 2);	 
	
	// Si elige archivo default guardara XM6 aunque haya juego cargado
	if (ArchivoDefault) 
	{	
		_tcscpy(lpszBuf, "XM6");
	}

	path.SetBaseFile(lpszBuf);
	_tcscpy(m_IniFile, path.GetPath());


   /* CString sz;
	sz.Format(_T("\n\nRutaArchivoXM6: %s\n\n"), m_pFrmWnd->RutaCompletaArchivoXM6);
	OutputDebugStringW(CT2W(sz)); */	

	OutputDebugString("\n\nSe ejecut? CustomInit para guardar configuracion...\n\n");	

	// Guardar y cargar
	m_bApply = FALSE;

	return TRUE;
}




//---------------------------------------------------------------------------
//
//	Limpieza
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::Cleanup()
{
	//int i;

	ASSERT(this);

	// ・ｽﾝ抵ｿｽf・ｽ[・ｽ^
	//SaveConfig();

	// MRU
	//for (i=0; i<MruTypes; i++) {
	//		SaveMRU(i);
	//}

	// ・ｽL・ｽ[
	//SaveKey();

	// TrueKey
	//SaveTKey();

	// Clase base
	CComponent::Cleanup();
}



// Igual que cleanup pero guardando todo
void FASTCALL CConfig::Cleanup2()
{
	int i;
	
	// Guardar estatus de ventana y de disco
	m_pFrmWnd->SaveFrameWnd();
	m_pFrmWnd->SaveDiskState();

	ASSERT(this);


	// ・ｽGuardar configuracion
	SaveConfig();

	// Guardar MRU
	for (i = 0; i < MruTypes; i++) {
		SaveMRU(i);
	}

	// Guardar claves
	SaveKey();

	// TrueKey
	SaveTKey();
		

	// Clase base
	//CComponent::Cleanup();
}


//---------------------------------------------------------------------------
//
//	・ｽﾝ抵ｿｽf・ｽ[・ｽ^・ｽ・ｽ・ｽ・ｽ
//
//---------------------------------------------------------------------------
Config CConfig::m_Config;

//---------------------------------------------------------------------------
//
//	・ｽﾝ抵ｿｽf・ｽ[・ｽ^・ｽ謫ｾ
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::GetConfig(Config *pConfigBuf) const
{
	ASSERT(this);
	ASSERT(pConfigBuf);

	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽN・ｽ・ｽCopiar
	*pConfigBuf = m_Config;
}

//---------------------------------------------------------------------------
//
//	・ｽﾝ抵ｿｽf・ｽ[・ｽ^・ｽﾝ抵ｿｽ
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SetConfig(Config *pConfigBuf)
{
	ASSERT(this);
	ASSERT(pConfigBuf);

	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽN・ｽ・ｽCopiar
	m_Config = *pConfigBuf;
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽﾊ拡・ｽ・ｽﾝ抵ｿｽ
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SetStretch(BOOL bStretch)
{
	ASSERT(this);

	m_Config.aspect_stretch = bStretch;
}

//---------------------------------------------------------------------------
//
//	MIDI・ｽf・ｽo・ｽC・ｽX・ｽﾝ抵ｿｽ
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SetMIDIDevice(int nDevice, BOOL bIn)
{
	ASSERT(this);
	ASSERT(nDevice >= 0);

	// In・ｽﾜゑｿｽ・ｽ・ｽOut
	if (bIn) {
		m_Config.midiin_device = nDevice;
	}
	else {
		m_Config.midiout_device = nDevice;
	}
}

//---------------------------------------------------------------------------
//
//	INI・ｽt・ｽ@・ｽC・ｽ・ｽ・ｽe・ｽ[・ｽu・ｽ・ｽ
//	・ｽ・ｽ・ｽ|・ｽC・ｽ・ｽ・ｽ^・ｽE・ｽZ・ｽN・ｽV・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽE・ｽL・ｽ[・ｽ・ｽ・ｽE・ｽ^・ｽE・ｽf・ｽt・ｽH・ｽ・ｽ・ｽg・ｽl・ｽE・ｽﾅ擾ｿｽ・ｽl・ｽE・ｽﾅ托ｿｽl・ｽﾌ擾ｿｽ
//
//---------------------------------------------------------------------------
const CConfig::INIKEY CConfig::IniTable[] = {
	{ &CConfig::m_Config.system_clock, _T("Basic"), _T("Clock"), 0, 0, 0, 5 },
	{ &CConfig::m_Config.mpu_fullspeed, NULL, _T("MPUFullSpeed"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.vm_fullspeed, NULL, _T("VMFullSpeed"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.ram_size, NULL, _T("Memory"), 0, 0, 0, 5 },
	{ &CConfig::m_Config.ram_sramsync, NULL, _T("AutoMemSw"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.mem_type, NULL, _T("Map"), 0, 1, 1, 6 },

	{ &CConfig::m_Config.sound_device, _T("Sound"), _T("Device"), 0, 0, 0, 15 },
	{ &CConfig::m_Config.sample_rate, NULL, _T("Rate"), 0, 5, 0, 5 },
	{ &CConfig::m_Config.primary_buffer, NULL, _T("Primary"), 0, 10, 2, 100 },
	{ &CConfig::m_Config.polling_buffer, NULL, _T("Polling"), 0, 5, 1, 100 },
	{ &CConfig::m_Config.adpcm_interp, NULL, _T("ADPCMInterP"), 1, TRUE, 0, 0 },

	{ &CConfig::m_Config.aspect_stretch, _T("Display"), _T("Stretch"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.render_shader, NULL, _T("Shader"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.render_vsync, NULL, _T("VSync"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.render_mode, NULL, _T("Renderer"), 0, 1, 0, 1 },
	{ &CConfig::m_Config.alt_raster, NULL, _T("AltRaster"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.caption_info, NULL, _T("Info"), 1, TRUE, 0, 0 },

	{ &CConfig::m_Config.master_volume, _T("Volume"), _T("Master"), 0, 100, 0, 100 },
	{ &CConfig::m_Config.fm_enable, NULL, _T("FMEnable"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.fm_volume, NULL, _T("FM"), 0, 54, 0, 100 },
	{ &CConfig::m_Config.adpcm_enable, NULL, _T("ADPCMEnable"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.adpcm_volume, NULL, _T("ADPCM"), 0, 52, 0, 100 },

	{ &CConfig::m_Config.kbd_connect, _T("Keyboard"), _T("Connect"), 1, TRUE, 0, 0 },

	{ &CConfig::m_Config.mouse_speed, _T("Mouse"), _T("Speed"), 0, 205, 0, 512 },
	{ &CConfig::m_Config.mouse_port, NULL, _T("Port"), 0, 1, 0, 2 },
	{ &CConfig::m_Config.mouse_swap, NULL, _T("Swap"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.mouse_mid, NULL, _T("MidBtn"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.mouse_trackb, NULL, _T("TrackBall"), 1, FALSE, 0, 0 },

	{ &CConfig::m_Config.joy_type[0], _T("Joystick"), _T("Port1"), 0, 1, 0, 15 },
	{ &CConfig::m_Config.joy_type[1], NULL, _T("Port2"), 0, 1, 0, 15 },
	{ &CConfig::m_Config.joy_dev[0], NULL, _T("Device1"), 0, 1, 0, 15 },
	{ &CConfig::m_Config.joy_dev[1], NULL, _T("Device2"), 0, 2, 0, 15 },
	{ &CConfig::m_Config.joy_button0[0], NULL, _T("Button11"), 0, 1, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[1], NULL, _T("Button12"), 0, 2, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[2], NULL, _T("Button13"), 0, 3, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[3], NULL, _T("Button14"), 0, 4, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[4], NULL, _T("Button15"), 0, 5, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[5], NULL, _T("Button16"), 0, 6, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[6], NULL, _T("Button17"), 0, 7, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[7], NULL, _T("Button18"), 0, 8, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[8], NULL, _T("Button19"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[9], NULL, _T("Button1A"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[10], NULL, _T("Button1B"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button0[11], NULL, _T("Button1C"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[0], NULL, _T("Button21"), 0, 65537, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[1], NULL, _T("Button22"), 0, 65538, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[2], NULL, _T("Button23"), 0, 65539, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[3], NULL, _T("Button24"), 0, 65540, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[4], NULL, _T("Button25"), 0, 65541, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[5], NULL, _T("Button26"), 0, 65542, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[6], NULL, _T("Button27"), 0, 65543, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[7], NULL, _T("Button28"), 0, 65544, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[8], NULL, _T("Button29"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[9], NULL, _T("Button2A"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[10], NULL, _T("Button2B"), 0, 0, 0, 131071 },
	{ &CConfig::m_Config.joy_button1[11], NULL, _T("Button2C"), 0, 0, 0, 131071 },

	{ &CConfig::m_Config.sasi_drives, _T("SASI"), _T("Drives"), 0, -1, 0, 16 },
	{ &CConfig::m_Config.sasi_sramsync, NULL, _T("AutoMemSw"), 1, TRUE, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 0], NULL, _T("File0") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 1], NULL, _T("File1") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 2], NULL, _T("File2") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 3], NULL, _T("File3") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 4], NULL, _T("File4") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 5], NULL, _T("File5") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 6], NULL, _T("File6") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 7], NULL, _T("File7") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 8], NULL, _T("File8") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[ 9], NULL, _T("File9") , 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[10], NULL, _T("File10"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[11], NULL, _T("File11"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[12], NULL, _T("File12"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[13], NULL, _T("File13"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[14], NULL, _T("File14"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sasi_file[15], NULL, _T("File15"), 2, FILEPATH_MAX, 0, 0 },

	{ &CConfig::m_Config.sxsi_drives, _T("SxSI"), _T("Drives"), 0, 0, 0, 7 },
	{ &CConfig::m_Config.sxsi_mofirst, NULL, _T("FirstMO"), 1, FALSE, 0, 0 },
	{ CConfig::m_Config.sxsi_file[ 0], NULL, _T("File0"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sxsi_file[ 1], NULL, _T("File1"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sxsi_file[ 2], NULL, _T("File2"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sxsi_file[ 3], NULL, _T("File3"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sxsi_file[ 4], NULL, _T("File4"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.sxsi_file[ 5], NULL, _T("File5"), 2, FILEPATH_MAX, 0, 0 },

	{ &CConfig::m_Config.scsi_ilevel, _T("SCSI"), _T("IntLevel"), 0, 1, 0, 1 },
	{ &CConfig::m_Config.scsi_drives, NULL, _T("Drives"), 0, 0, 0, 7 },
	{ &CConfig::m_Config.scsi_sramsync, NULL, _T("AutoMemSw"), 1, TRUE, 0, 0 },
	{ &m_bCDROM,						NULL, _T("CDROM"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.scsi_mofirst, NULL, _T("FirstMO"), 1, FALSE, 0, 0 },
	{ CConfig::m_Config.scsi_file[ 0], NULL, _T("File0"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.scsi_file[ 1], NULL, _T("File1"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.scsi_file[ 2], NULL, _T("File2"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.scsi_file[ 3], NULL, _T("File3"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.scsi_file[ 4], NULL, _T("File4"), 2, FILEPATH_MAX, 0, 0 },

	{ &CConfig::m_Config.port_com, _T("Port"), _T("COM"), 0, 0, 0, 9 },
	{ CConfig::m_Config.port_recvlog, NULL, _T("RecvLog"), 2, FILEPATH_MAX, 0, 0 },
	{ &CConfig::m_Config.port_384, NULL, _T("Force38400"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.port_lpt, NULL, _T("LPT"), 0, 0, 0, 9 },
	{ CConfig::m_Config.port_sendlog, NULL, _T("SendLog"), 2, FILEPATH_MAX, 0, 0 },

	{ &CConfig::m_Config.midi_bid, _T("MIDI"), _T("ID"), 0, 0, 0, 2 },
	{ &CConfig::m_Config.midi_ilevel, NULL, _T("IntLevel"), 0, 0, 0, 1 },
	{ &CConfig::m_Config.midi_reset, NULL, _T("ResetCmd"), 0, 0, 0, 3 },
	{ &CConfig::m_Config.midiin_device, NULL, _T("InDevice"), 0, 0, 0, 15 },
	{ &CConfig::m_Config.midiin_delay, NULL, _T("InDelay"), 0, 0, 0, 200 },
	{ &CConfig::m_Config.midiout_device, NULL, _T("OutDevice"), 0, 0, 0, 15 },
	{ &CConfig::m_Config.midiout_delay, NULL, _T("OutDelay"), 0, 84, 20, 200 },

	{ &CConfig::m_Config.windrv_enable, _T("Windrv"), _T("Enable"), 0, 0, 0, 255 },
	{ &CConfig::m_Config.host_option, NULL, _T("Option"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_resume, NULL, _T("Resume"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.host_drives, NULL, _T("Drives"), 0, 0, 0, 10 },
	{ &CConfig::m_Config.host_flag[ 0], NULL, _T("Flag0"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 1], NULL, _T("Flag1"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 2], NULL, _T("Flag2"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 3], NULL, _T("Flag3"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 4], NULL, _T("Flag4"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 5], NULL, _T("Flag5"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 6], NULL, _T("Flag6"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 7], NULL, _T("Flag7"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 8], NULL, _T("Flag8"), 0, 0, 0, 0x7FFFFFFF },
	{ &CConfig::m_Config.host_flag[ 9], NULL, _T("Flag9"), 0, 0, 0, 0x7FFFFFFF },
	{ CConfig::m_Config.host_path[ 0], NULL, _T("Path0"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 1], NULL, _T("Path1"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 2], NULL, _T("Path2"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 3], NULL, _T("Path3"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 4], NULL, _T("Path4"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 5], NULL, _T("Path5"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 6], NULL, _T("Path6"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 7], NULL, _T("Path7"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 8], NULL, _T("Path8"), 2, FILEPATH_MAX, 0, 0 },
	{ CConfig::m_Config.host_path[ 9], NULL, _T("Path9"), 2, FILEPATH_MAX, 0, 0 },

	{ &CConfig::m_Config.sram_64k, _T("Alter"), _T("SRAM64K"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.scc_clkup, NULL, _T("SCCClock"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.power_led, NULL, _T("BlueLED"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.dual_fdd, NULL, _T("DualFDD"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.sasi_parity, NULL, _T("SASIParity"), 1, TRUE, 0, 0 },

	{ &CConfig::m_Config.caption, _T("Window"), _T("Caption"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.menu_bar, NULL, _T("MenuBar"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.status_bar, NULL, _T("StatusBar"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.window_left, NULL, _T("Left"), 0, -0x8000, -0x8000, 0x7fff },
	{ &CConfig::m_Config.window_top, NULL, _T("Top"), 0, -0x8000, -0x8000, 0x7fff },
	{ &CConfig::m_Config.window_full, NULL, _T("Full"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.window_mode, NULL, _T("Mode"), 0, 0, 0, 0 },

	{ &CConfig::m_Config.resume_fd, _T("Resume"), _T("FD"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_fdi[0], NULL, _T("FDI0"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_fdi[1], NULL, _T("FDI1"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_fdw[0], NULL, _T("FDW0"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_fdw[1], NULL, _T("FDW1"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_fdm[0], NULL, _T("FDM0"), 0, 0, 0, 15 },
	{ &CConfig::m_Config.resume_fdm[1], NULL, _T("FDM1"), 0, 0, 0, 15 },
	{ &CConfig::m_Config.resume_mo, NULL, _T("MO"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_mos, NULL, _T("MOS"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_mow, NULL, _T("MOW"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_cd, NULL, _T("CD"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_iso, NULL, _T("ISO"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_state, NULL, _T("State"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_xm6, NULL, _T("XM6"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_screen, NULL, _T("Screen"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_dir, NULL, _T("Dir"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.resume_path, NULL, _T("Path"), 2, FILEPATH_MAX, 0, 0 },

	{ &CConfig::m_Config.tkey_mode, _T("TrueKey"), _T("Mode"), 0, 1, 0, 3 },
	{ &CConfig::m_Config.tkey_com, NULL, _T("COM"), 0, 0, 0, 9 },
	{ &CConfig::m_Config.tkey_rts, NULL, _T("RTS"), 1, FALSE, 0, 0 },

	{ &CConfig::m_Config.floppy_speed, _T("Misc"), _T("FloppySpeed"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.floppy_led, NULL, _T("FloppyLED"), 1, TRUE, 0, 0 },
	{ &CConfig::m_Config.popup_swnd, NULL, _T("PopupWnd"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.auto_mouse, NULL, _T("AutoMouse"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.power_off, NULL, _T("PowerOff"), 1, FALSE, 0, 0 },
	{ &CConfig::m_Config.ruta_savestate, NULL, _T("RutaSave"), 2, FILEPATH_MAX, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 }
};

//---------------------------------------------------------------------------
//
//	・ｽﾝ抵ｿｽf・ｽ[・ｽ^・ｽ・ｽ・ｽ[・ｽh
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::LoadConfig()
{
	PINIKEY pIni;
	int nValue;
	BOOL bFlag;
	LPCTSTR pszSection;
	TCHAR szDef[1];
	TCHAR szBuf[FILEPATH_MAX];

	ASSERT(this);

	// ・ｽe・ｽ[・ｽu・ｽ・ｽ・ｽﾌ先頭・ｽﾉ搾ｿｽ・ｽ墲ｹ・ｽ・ｽ
	pIni = (const PINIKEY)&IniTable[0];
	pszSection = NULL;
	szDef[0] = _T('\0');

	// ・ｽe・ｽ[・ｽu・ｽ・ｽBucle
	while (pIni->pBuf) {
		// ・ｽZ・ｽN・ｽV・ｽ・ｽ・ｽ・ｽ・ｽﾝ抵ｿｽ
		if (pIni->pszSection) {
			pszSection = pIni->pszSection;
		}
		ASSERT(pszSection);

		// ・ｽ^・ｽC・ｽvVerificacion
		switch (pIni->nType) {
			// ・ｽ・ｽ・ｽ・ｽ・ｽ^(・ｽﾍ囲を超ゑｿｽ・ｽ・ｽ・ｽ・ｽf・ｽt・ｽH・ｽ・ｽ・ｽg・ｽl)
			case 0:
				nValue = ::GetPrivateProfileInt(pszSection, pIni->pszKey, pIni->nDef, m_IniFile);
				if ((nValue < pIni->nMin) || (pIni->nMax < nValue)) {
					nValue = pIni->nDef;
				}
				*((int*)pIni->pBuf) = nValue;
				break;

			// ・ｽ_・ｽ・ｽ・ｽ^(0,1・ｽﾌどゑｿｽ・ｽ・ｽﾅゑｿｽ・ｽﾈゑｿｽ・ｽ・ｽﾎデ・ｽt・ｽH・ｽ・ｽ・ｽg・ｽl)
			case 1:
				nValue = ::GetPrivateProfileInt(pszSection, pIni->pszKey, -1, m_IniFile);
				switch (nValue) {
					case 0:
						bFlag = FALSE;
						break;
					case 1:
						bFlag = TRUE;
						break;
					default:
						bFlag = (BOOL)pIni->nDef;
						break;
				}
				*((BOOL*)pIni->pBuf) = bFlag;
				break;

			// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ^(・ｽo・ｽb・ｽt・ｽ@・ｽT・ｽC・ｽY・ｽﾍ囲難ｿｽ・ｽﾅのタ・ｽ[・ｽ~・ｽl・ｽ[・ｽg・ｽ・ｽﾛ擾ｿｽ)
			case 2:
				ASSERT(pIni->nDef <= (sizeof(szBuf)/sizeof(TCHAR)));
				::GetPrivateProfileString(pszSection, pIni->pszKey, szDef, szBuf,
										sizeof(szBuf)/sizeof(TCHAR), m_IniFile);

				// ・ｽf・ｽt・ｽH・ｽ・ｽ・ｽg・ｽl・ｽﾉはバ・ｽb・ｽt・ｽ@・ｽT・ｽC・ｽY・ｽ・ｽ・ｽL・ｽ・ｽ・ｽ・ｽ・ｽ驍ｱ・ｽ・ｽ
				ASSERT(pIni->nDef > 0);
				szBuf[pIni->nDef - 1] = _T('\0');
				_tcscpy((LPTSTR)pIni->pBuf, szBuf);
				break;

			// Otros
			default:
				ASSERT(FALSE);
				break;
		}

		// Siguiente
		pIni++;
	}
}

//---------------------------------------------------------------------------
//
//	・ｽﾝ抵ｿｽf・ｽ[・ｽ^・ｽZ・ｽ[・ｽu
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SaveConfig() const
{
	PINIKEY pIni;
	CString string;
	LPCTSTR pszSection;

	ASSERT(this);

	// ・ｽe・ｽ[・ｽu・ｽ・ｽ・ｽﾌ先頭・ｽﾉ搾ｿｽ・ｽ墲ｹ・ｽ・ｽ
	pIni = (const PINIKEY)&IniTable[0];
	pszSection = NULL;

	// ・ｽe・ｽ[・ｽu・ｽ・ｽBucle
	while (pIni->pBuf) {
		// ・ｽZ・ｽN・ｽV・ｽ・ｽ・ｽ・ｽ・ｽﾝ抵ｿｽ
		if (pIni->pszSection) {
			pszSection = pIni->pszSection;
		}
		ASSERT(pszSection);

		// ・ｽ^・ｽC・ｽvVerificacion
		switch (pIni->nType) {
			// ・ｽ・ｽ・ｽ・ｽ・ｽ^
			case 0:
				string.Format(_T("%d"), *((int*)pIni->pBuf));
				::WritePrivateProfileString(pszSection, pIni->pszKey,
											string, m_IniFile);
				break;

			// ・ｽ_・ｽ・ｽ・ｽ^
			case 1:
				if (*(BOOL*)pIni->pBuf) {
					string = _T("1");
				}
				else {
					string = _T("0");
				}
				::WritePrivateProfileString(pszSection, pIni->pszKey,
											string, m_IniFile);
				break;

			// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ^
			case 2:
				::WritePrivateProfileString(pszSection, pIni->pszKey,
											(LPCTSTR)pIni->pBuf, m_IniFile);
				break;

			// Otros
			default:
				ASSERT(FALSE);
				break;
		}

		// Siguiente
		pIni++;
	}
}

//---------------------------------------------------------------------------
//
//	SASI・ｽ・ｽ・ｽZ・ｽb・ｽg
//	・ｽ・ｽversion1.44・ｽﾜでは趣ｿｽ・ｽ・ｽ・ｽt・ｽ@・ｽC・ｽ・ｽBusqueda・ｽﾌゑｿｽ・ｽﾟ、・ｽ・ｽ・ｽ・ｽ・ｽﾅ搾ｿｽBusqueda・ｽﾆ設抵ｿｽ・ｽ・ｽs・ｽ・ｽ
//	version1.45・ｽﾈ降・ｽﾖの移行・ｽ・ｽ・ｽX・ｽ・ｽ・ｽ[・ｽY・ｽﾉ行・ｽ・ｽ
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::ResetSASI()
{
	int i;
	Filepath path;
	Fileio fio;
	TCHAR szPath[FILEPATH_MAX];

	ASSERT(this);

	// Numero de unidades>=0・ｽﾌ場合・ｽﾍ不・ｽv(・ｽﾝ抵ｿｽﾏゑｿｽ)
	if (m_Config.sasi_drives >= 0) {
		return;
	}

	// Numero de unidades0
	m_Config.sasi_drives = 0;

	// Nombre de archivoCrearBucle
	for (i=0; i<16; i++) {
		_stprintf(szPath, _T("HD%d.HDF"), i);
		path.SetPath(szPath);
		path.SetBaseDir();
		_tcscpy(m_Config.sasi_file[i], path.GetPath());
	}

	// ・ｽﾅ擾ｿｽ・ｽ・ｽ・ｽ・ｽVerificacion・ｽ・ｽ・ｽﾄ、・ｽL・ｽ・ｽ・ｽ・ｽNumero de unidades・ｽ・ｽ・ｽ・ｽ・ｽﾟゑｿｽ
	for (i=0; i<16; i++) {
		path.SetPath(m_Config.sasi_file[i]);
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return;
		}

		// ・ｽT・ｽC・ｽYVerificacion(version1.44・ｽﾅゑｿｽ40MB・ｽh・ｽ・ｽ・ｽC・ｽu・ｽﾌみサ・ｽ|・ｽ[・ｽg)
		if (fio.GetFileSize() != 0x2793000) {
			fio.Close();
			return;
		}

		// ・ｽJ・ｽE・ｽ・ｽ・ｽg・ｽA・ｽb・ｽv・ｽﾆク・ｽ・ｽ・ｽ[・ｽY
		m_Config.sasi_drives++;
		fio.Close();
	}
}

//---------------------------------------------------------------------------
//
//	CD-ROM・ｽ・ｽ・ｽZ・ｽb・ｽg
//	・ｽ・ｽversion2.02・ｽﾜでゑｿｽCD-ROM・ｽ・ｽ・ｽT・ｽ|・ｽ[・ｽg・ｽﾌゑｿｽ・ｽﾟ、SCSINumero de unidades・ｽ・ｽ+1・ｽ・ｽ・ｽ・ｽ
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::ResetCDROM()
{
	ASSERT(this);

	// CD-ROM・ｽt・ｽ・ｽ・ｽO・ｽ・ｽ・ｽZ・ｽb・ｽg・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽ鼾・ｿｽﾍ不・ｽv(・ｽﾝ抵ｿｽﾏゑｿｽ)
	if (m_bCDROM) {
		return;
	}

	// CD-ROM・ｽt・ｽ・ｽ・ｽO・ｽ・ｽ・ｽZ・ｽb・ｽg
	m_bCDROM = TRUE;

	// SCSINumero de unidades・ｽ・ｽ3・ｽﾈ擾ｿｽ6・ｽﾈ会ｿｽ・ｽﾌ場合・ｽﾉ鯉ｿｽ・ｽ・ｽA+1
	if ((m_Config.scsi_drives >= 3) && (m_Config.scsi_drives <= 6)) {
		m_Config.scsi_drives++;
	}
}

//---------------------------------------------------------------------------
//
//	CD-ROM・ｽt・ｽ・ｽ・ｽO
//
//---------------------------------------------------------------------------
BOOL CConfig::m_bCDROM;

//---------------------------------------------------------------------------
//
//	MRULimpiar
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::ClearMRU(int nType)
{
	int i;

	ASSERT(this);
	ASSERT((nType >= 0) && (nType < MruTypes));

	// ・ｽ・ｽ・ｽ・ｽLimpiar
	for (i=0; i<9; i++) {
		memset(m_MRUFile[nType][i], 0, FILEPATH_MAX * sizeof(TCHAR));
	}

	// ・ｽﾂ撰ｿｽLimpiar
	m_MRUNum[nType] = 0;
}

//---------------------------------------------------------------------------
//
//	MRU・ｽ・ｽ・ｽ[・ｽh
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::LoadMRU(int nType)
{
	CString strSection;
	CString strKey;
	int i;

	ASSERT(this);
	ASSERT((nType >= 0) && (nType < MruTypes));

	// ・ｽZ・ｽN・ｽV・ｽ・ｽ・ｽ・ｽCrear
	strSection.Format(_T("MRU%d"), nType);

	// Bucle
	for (i=0; i<9; i++) {
		strKey.Format(_T("File%d"), i);
		::GetPrivateProfileString(strSection, strKey, _T(""), m_MRUFile[nType][i],
								FILEPATH_MAX, m_IniFile);
		if (m_MRUFile[nType][i][0] == _T('\0')) {
			break;
		}
		m_MRUNum[nType]++;
	}
}

//---------------------------------------------------------------------------
//
//	MRU・ｽZ・ｽ[・ｽu
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SaveMRU(int nType) const
{
	CString strSection;
	CString strKey;
	int i;

	ASSERT(this);
	ASSERT((nType >= 0) && (nType < MruTypes));

	// ・ｽZ・ｽN・ｽV・ｽ・ｽ・ｽ・ｽCrear
	strSection.Format(_T("MRU%d"), nType);

	// Bucle
	for (i=0; i<9; i++) {
		strKey.Format(_T("File%d"), i);
		::WritePrivateProfileString(strSection, strKey, m_MRUFile[nType][i],
								m_IniFile);
	}
}

//---------------------------------------------------------------------------
//
//	MRU・ｽZ・ｽb・ｽg
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SetMRUFile(int nType, LPCTSTR lpszFile)
{
	int nMRU;
	int nCpy;
	int nNum;

	ASSERT(this);
	ASSERT((nType >= 0) && (nType < MruTypes));
	ASSERT(lpszFile);

	// ・ｽ・ｽ・ｽﾉ難ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽﾌゑｿｽ・ｽﾈゑｿｽ・ｽ・ｽ
	nNum = GetMRUNum(nType);
	for (nMRU=0; nMRU<nNum; nMRU++) {
		if (_tcscmp(m_MRUFile[nType][nMRU], lpszFile) == 0) {
			// ・ｽ謫ｪ・ｽﾉゑｿｽ・ｽ・ｽ・ｽﾄ、・ｽﾜゑｿｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾌゑｿｽAgregar・ｽ・ｽ・ｽ謔､・ｽﾆゑｿｽ・ｽ・ｽ
			if (nMRU == 0) {
				return;
			}

			// Copiar
			for (nCpy=nMRU; nCpy>=1; nCpy--) {
				memcpy(m_MRUFile[nType][nCpy], m_MRUFile[nType][nCpy - 1],
						FILEPATH_MAX * sizeof(TCHAR));
			}

			// ・ｽ謫ｪ・ｽﾉセ・ｽb・ｽg
			_tcscpy(m_MRUFile[nType][0], lpszFile);
			return;
		}
	}

	// ・ｽﾚ難ｿｽ
	for (nMRU=7; nMRU>=0; nMRU--) {
		memcpy(m_MRUFile[nType][nMRU + 1], m_MRUFile[nType][nMRU],
				FILEPATH_MAX * sizeof(TCHAR));
	}

	// ・ｽ謫ｪ・ｽﾉセ・ｽb・ｽg
	ASSERT(_tcslen(lpszFile) < FILEPATH_MAX);
	_tcscpy(m_MRUFile[nType][0], lpszFile);

	// ・ｽﾂ撰ｿｽActualizacion
	if (m_MRUNum[nType] < 9) {
		m_MRUNum[nType]++;
	}
}

//---------------------------------------------------------------------------
//
//	MRU・ｽ謫ｾ
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::GetMRUFile(int nType, int nIndex, LPTSTR lpszFile) const
{
	ASSERT(this);
	ASSERT((nType >= 0) && (nType < MruTypes));
	ASSERT((nIndex >= 0) && (nIndex < 9));
	ASSERT(lpszFile);

	// ・ｽﾂ撰ｿｽ・ｽﾈ擾ｿｽﾈゑｿｽ\0
	if (nIndex >= m_MRUNum[nType]) {
		lpszFile[0] = _T('\0');
		return;
	}

	// Copiar
	ASSERT(_tcslen(m_MRUFile[nType][nIndex]) < FILEPATH_MAX);
	_tcscpy(lpszFile, m_MRUFile[nType][nIndex]);
}

//---------------------------------------------------------------------------
//
//	MRU・ｽﾂ撰ｿｽ・ｽ謫ｾ
//
//---------------------------------------------------------------------------
int FASTCALL CConfig::GetMRUNum(int nType) const
{
	ASSERT(this);
	ASSERT((nType >= 0) && (nType < MruTypes));

	return m_MRUNum[nType];
}

//---------------------------------------------------------------------------
//
//	・ｽL・ｽ[・ｽ・ｽ・ｽ[・ｽh
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::LoadKey() const
{
	DWORD dwMap[0x100];
	CInput *pInput;
	CString strName;
	int i;
	int nValue;
	BOOL bFlag;

	ASSERT(this);

	// Obtener entrada
	pInput = m_pFrmWnd->GetInput();
	ASSERT(pInput);

	// ・ｽt・ｽ・ｽ・ｽOOFF(・ｽL・ｽ・ｽ・ｽf・ｽ[・ｽ^・ｽﾈゑｿｽ)・ｽALimpiar
	bFlag = FALSE;
	memset(dwMap, 0, sizeof(dwMap));

	// Bucle
	for (i=0; i<0x100; i++) {
		strName.Format(_T("Key%d"), i);
		nValue = ::GetPrivateProfileInt(_T("Keyboard"), strName, 0, m_IniFile);

		// ・ｽl・ｽ・ｽ・ｽﾍ囲難ｿｽ・ｽﾉ趣ｿｽ・ｽﾜゑｿｽ・ｽﾄゑｿｽ・ｽﾈゑｿｽ・ｽ・ｽﾎ、・ｽ・ｽ・ｽ・ｽ・ｽﾅ打ゑｿｽ・ｽﾘゑｿｽ(・ｽf・ｽt・ｽH・ｽ・ｽ・ｽg・ｽl・ｽ・ｽ・ｽg・ｽ・ｽ)
		if ((nValue < 0) || (nValue > 0x73)) {
			return;
		}

		// ・ｽl・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾎセ・ｽb・ｽg・ｽ・ｽ・ｽﾄ、・ｽt・ｽ・ｽ・ｽO・ｽ・ｽ・ｽﾄゑｿｽ
		if (nValue != 0) {
			dwMap[i] = nValue;
			bFlag = TRUE;
		}
	}

	// ・ｽt・ｽ・ｽ・ｽO・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽﾎ、・ｽ}・ｽb・ｽv・ｽf・ｽ[・ｽ^・ｽﾝ抵ｿｽ
	if (bFlag) {
		pInput->SetKeyMap(dwMap);
	}
}

//---------------------------------------------------------------------------
//
//	・ｽL・ｽ[・ｽZ・ｽ[・ｽu
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SaveKey() const
{
	DWORD dwMap[0x100];
	CInput *pInput;
	CString strName;
	CString strKey;
	int i;

	ASSERT(this);

	// Obtener entrada
	pInput = m_pFrmWnd->GetInput();
	ASSERT(pInput);

	// ・ｽ}・ｽb・ｽv・ｽf・ｽ[・ｽ^・ｽ謫ｾ
	pInput->GetKeyMap(dwMap);

	// Bucle
	for (i=0; i<0x100; i++) {
		// ・ｽ・ｽ・ｽﾗゑｿｽ(256・ｽ・ｽ・ｽ)・ｽ・ｽ・ｽ・ｽ
		strName.Format(_T("Key%d"), i);
		strKey.Format(_T("%d"), dwMap[i]);
		::WritePrivateProfileString(_T("Keyboard"), strName,
									strKey, m_IniFile);
	}
}

//---------------------------------------------------------------------------
//
//	TrueKey・ｽ・ｽ・ｽ[・ｽh
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::LoadTKey() const
{
	CTKey *pTKey;
	int nMap[0x73];
	CString strName;
	CString strKey;
	int i;
	int nValue;
	BOOL bFlag;

	ASSERT(this);

	// Obtener TrueKey
	pTKey = m_pFrmWnd->GetTKey();
	ASSERT(pTKey);

	// ・ｽt・ｽ・ｽ・ｽOOFF(・ｽL・ｽ・ｽ・ｽf・ｽ[・ｽ^・ｽﾈゑｿｽ)・ｽALimpiar
	bFlag = FALSE;
	memset(nMap, 0, sizeof(nMap));

	// Bucle
	for (i=0; i<0x73; i++) {
		strName.Format(_T("Key%d"), i);
		nValue = ::GetPrivateProfileInt(_T("TrueKey"), strName, 0, m_IniFile);

		// ・ｽl・ｽ・ｽ・ｽﾍ囲難ｿｽ・ｽﾉ趣ｿｽ・ｽﾜゑｿｽ・ｽﾄゑｿｽ・ｽﾈゑｿｽ・ｽ・ｽﾎ、・ｽ・ｽ・ｽ・ｽ・ｽﾅ打ゑｿｽ・ｽﾘゑｿｽ(・ｽf・ｽt・ｽH・ｽ・ｽ・ｽg・ｽl・ｽ・ｽ・ｽg・ｽ・ｽ)
		if ((nValue < 0) || (nValue > 0xfe)) {
			return;
		}

		// ・ｽl・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾎセ・ｽb・ｽg・ｽ・ｽ・ｽﾄ、・ｽt・ｽ・ｽ・ｽO・ｽ・ｽ・ｽﾄゑｿｽ
		if (nValue != 0) {
			nMap[i] = nValue;
			bFlag = TRUE;
		}
	}

	// ・ｽt・ｽ・ｽ・ｽO・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽﾎ、・ｽ}・ｽb・ｽv・ｽf・ｽ[・ｽ^・ｽﾝ抵ｿｽ
	if (bFlag) {
		pTKey->SetKeyMap(nMap);
	}
}

//---------------------------------------------------------------------------
//
//	TrueKey・ｽZ・ｽ[・ｽu
//
//---------------------------------------------------------------------------
void FASTCALL CConfig::SaveTKey() const
{
	CTKey *pTKey;
	int nMap[0x73];
	CString strName;
	CString strKey;
	int i;

	ASSERT(this);

	// Obtener TrueKey
	pTKey = m_pFrmWnd->GetTKey();
	ASSERT(pTKey);

	// ・ｽL・ｽ[・ｽ}・ｽb・ｽv・ｽ謫ｾ
	pTKey->GetKeyMap(nMap);

	// Bucle
	for (i=0; i<0x73; i++) {
		// ・ｽ・ｽ・ｽﾗゑｿｽ(0x73・ｽ・ｽ・ｽ)・ｽ・ｽ・ｽ・ｽ
		strName.Format(_T("Key%d"), i);
		strKey.Format(_T("%d"), nMap[i]);
		::WritePrivateProfileString(_T("TrueKey"), strName,
									strKey, m_IniFile);
	}
}

//---------------------------------------------------------------------------
//
//	・ｽZ・ｽ[・ｽu
//
//---------------------------------------------------------------------------
BOOL FASTCALL CConfig::Save(Fileio *pFio, int /*nVer*/)
{
	size_t sz;

	ASSERT(this);
	ASSERT(pFio);

	// ・ｽT・ｽC・ｽY・ｽ・ｽ・ｽZ・ｽ[・ｽu
	sz = sizeof(m_Config);
	if (!pFio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// ・ｽ{・ｽﾌゑｿｽ・ｽZ・ｽ[・ｽu
	if (!pFio->Write(&m_Config, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽ[・ｽh
//
//---------------------------------------------------------------------------
BOOL FASTCALL CConfig::Load(Fileio *pFio, int nVer)
{
	size_t sz;

	ASSERT(this);
	ASSERT(pFio);

	// ・ｽﾈ前・ｽﾌバ・ｽ[・ｽW・ｽ・ｽ・ｽ・ｽ・ｽﾆの互奇ｿｽ
	if (nVer <= 0x0201) {
		return Load200(pFio);
	}
	if (nVer <= 0x0203) {
		return Load202(pFio);
	}

	// ・ｽT・ｽC・ｽY・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽh・ｽA・ｽﾆ搾ｿｽ
	if (!pFio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(m_Config)) {
		return FALSE;
	}

	// ・ｽ{・ｽﾌゑｿｽ・ｽ・ｽ・ｽ[・ｽh
	if (!pFio->Read(&m_Config, (int)sz)) {
		return FALSE;
	}

	// ApplyCfg・ｽv・ｽ・ｽ・ｽt・ｽ・ｽ・ｽO・ｽ・ｽ・ｽ繧ｰ・ｽ・ｽ
	m_bApply = TRUE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽ[・ｽh(version2.00)
//
//---------------------------------------------------------------------------
BOOL FASTCALL CConfig::Load200(Fileio *pFio)
{
	int i;
	size_t sz;
	Config200 *pConfig200;

	ASSERT(this);
	ASSERT(pFio);

	// ・ｽL・ｽ・ｽ・ｽX・ｽg・ｽ・ｽ・ｽﾄ、version2.00・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽh・ｽﾅゑｿｽ・ｽ・ｽ謔､・ｽﾉゑｿｽ・ｽ・ｽ
	pConfig200 = (Config200*)&m_Config;

	// ・ｽT・ｽC・ｽY・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽh・ｽA・ｽﾆ搾ｿｽ
	if (!pFio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(Config200)) {
		return FALSE;
	}

	// ・ｽ{・ｽﾌゑｿｽ・ｽ・ｽ・ｽ[・ｽh
	if (!pFio->Read(pConfig200, (int)sz)) {
		return FALSE;
	}

	// ・ｽV・ｽK・ｽ・ｽ・ｽ・ｽ(Config202)・ｽ・ｽInicializacion
	m_Config.mem_type = 1;
	m_Config.scsi_ilevel = 1;
	m_Config.scsi_drives = 0;
	m_Config.scsi_sramsync = TRUE;
	m_Config.scsi_mofirst = FALSE;
	for (i=0; i<5; i++) {
		m_Config.scsi_file[i][0] = _T('\0');
	}

	// ・ｽV・ｽK・ｽ・ｽ・ｽ・ｽ(Config204)・ｽ・ｽInicializacion
	m_Config.windrv_enable = 0;
	m_Config.resume_fd = FALSE;
	m_Config.resume_mo = FALSE;
	m_Config.resume_cd = FALSE;
	m_Config.resume_state = FALSE;
	m_Config.resume_screen = FALSE;
	m_Config.resume_dir = FALSE;

	// ApplyCfg・ｽv・ｽ・ｽ・ｽt・ｽ・ｽ・ｽO・ｽ・ｽ・ｽ繧ｰ・ｽ・ｽ
	m_bApply = TRUE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽ[・ｽh(version2.02)
//
//---------------------------------------------------------------------------
BOOL FASTCALL CConfig::Load202(Fileio *pFio)
{
	size_t sz;
	Config202 *pConfig202;

	ASSERT(this);
	ASSERT(pFio);

	// ・ｽL・ｽ・ｽ・ｽX・ｽg・ｽ・ｽ・ｽﾄ、version2.02・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽh・ｽﾅゑｿｽ・ｽ・ｽ謔､・ｽﾉゑｿｽ・ｽ・ｽ
	pConfig202 = (Config202*)&m_Config;

	// ・ｽT・ｽC・ｽY・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽh・ｽA・ｽﾆ搾ｿｽ
	if (!pFio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(Config202)) {
		return FALSE;
	}

	// ・ｽ{・ｽﾌゑｿｽ・ｽ・ｽ・ｽ[・ｽh
	if (!pFio->Read(pConfig202, (int)sz)) {
		return FALSE;
	}

	// ・ｽV・ｽK・ｽ・ｽ・ｽ・ｽ(Config204)・ｽ・ｽInicializacion
	m_Config.windrv_enable = 0;
	m_Config.resume_fd = FALSE;
	m_Config.resume_mo = FALSE;
	m_Config.resume_cd = FALSE;
	m_Config.resume_state = FALSE;
	m_Config.resume_screen = FALSE;
	m_Config.resume_dir = FALSE;

	// ApplyCfg・ｽv・ｽ・ｽ・ｽt・ｽ・ｽ・ｽO・ｽ・ｽ・ｽ繧ｰ・ｽ・ｽ
	m_bApply = TRUE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply・ｽv・ｽ・ｽVerificacion
//
//---------------------------------------------------------------------------
BOOL FASTCALL CConfig::IsApply()
{
	ASSERT(this);

	// ・ｽv・ｽ・ｽ・ｽﾈゑｿｽA・ｽ・ｽ・ｽ・ｽ・ｽﾅ会ｿｽ・ｽ・ｷ
	if (m_bApply) {
		m_bApply = FALSE;
		return TRUE;
	}

	// ・ｽv・ｽ・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽﾈゑｿｽ
	return FALSE;
}

//===========================================================================
//
//	Configuracion・ｽv・ｽ・ｽ・ｽp・ｽe・ｽB・ｽy・ｽ[・ｽW
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CConfigPage::CConfigPage()
{
	// ・ｽ・ｽ・ｽ・ｽ・ｽo・ｽﾏ撰ｿｽLimpiar
	m_dwID = 0;
	m_nTemplate = 0;
	m_uHelpID = 0;
	m_uMsgID = 0;
	m_pConfig = NULL;
	m_pSheet = NULL;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CConfigPage, CPropertyPage)
	ON_WM_SETCURSOR()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
void FASTCALL CConfigPage::Init(CConfigSheet *pSheet)
{
	int nID;

	ASSERT(this);
	ASSERT(m_dwID != 0);

	// Memorizar hoja padre
	ASSERT(pSheet);
	m_pSheet = pSheet;

	// IDAceptar
	nID = m_nTemplate;
	if (!::IsJapanese()) {
		nID += 50;
	}

	// Construccion
	CommonConstruct(MAKEINTRESOURCE(nID), 0);

	// ・ｽe・ｽV・ｽ[・ｽg・ｽ・ｽAgregar
	pSheet->AddPage(this);
}

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CConfigPage::OnInitDialog()
{
	CConfigSheet *pSheet;

	ASSERT(this);

	// ・ｽe・ｽE・ｽB・ｽ・ｽ・ｽh・ｽE・ｽ・ｽ・ｽ・ｽﾝ抵ｿｽf・ｽ[・ｽ^・ｽ・ｽ・ｽｯ趣ｿｽ・ｽ
	pSheet = (CConfigSheet*)GetParent();
	ASSERT(pSheet);
	m_pConfig = pSheet->m_pConfig;

	// Clase base
	return CPropertyPage::OnInitDialog();
}

//---------------------------------------------------------------------------
//
//	Pagina activa
//
//---------------------------------------------------------------------------
BOOL CConfigPage::OnSetActive()
{
	CStatic *pStatic;
	CString strEmpty;

	ASSERT(this);

	// Clase base
	if (!CPropertyPage::OnSetActive()) {
		return FALSE;
	}

	// AyudaInicializacion
	ASSERT(m_uHelpID > 0);
	m_uMsgID = 0;
	pStatic = (CStatic*)GetDlgItem(m_uHelpID);
	ASSERT(pStatic);
	strEmpty.Empty();
	pStatic->SetWindowText(strEmpty);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	・ｽ}・ｽE・ｽXCursor・ｽﾝ抵ｿｽ
//
//---------------------------------------------------------------------------
BOOL CConfigPage::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT nMsg)
{
	CWnd *pChildWnd;
	CPoint pt;
	UINT nID;
	CRect rectParent;
	CRect rectChild;
	CString strText;
	CStatic *pStatic;

	// Ayuda・ｽ・ｽ・ｽw・ｽ閧ｳ・ｽ・ｽﾄゑｿｽ・ｽ驍ｱ・ｽ・ｽ
	ASSERT(this);
	ASSERT(m_uHelpID > 0);

	// ・ｽ}・ｽE・ｽX・ｽﾊ置・ｽ謫ｾ
	GetCursorPos(&pt);

	// ・ｽq・ｽE・ｽB・ｽ・ｽ・ｽh・ｽE・ｽ・ｽ・ｽﾜゑｿｽ・ｽ・ｽﾄ、・ｽ・ｽ`・ｽ・ｽ・ｽﾉ位置・ｽ・ｽ・ｽ驍ｩ・ｽ・ｽ・ｽﾗゑｿｽ
	nID = 0;
	rectParent.top = 0;
	pChildWnd = GetTopWindow();

	// Bucle
	while (pChildWnd) {
		// AyudaID・ｽ・ｽ・ｽg・ｽﾈゑｿｽX・ｽL・ｽb・ｽv
		if (pChildWnd->GetDlgCtrlID() == (int)m_uHelpID) {
			pChildWnd = pChildWnd->GetNextWindow();
			continue;
		}

		// ・ｽ・ｽ`・ｽ・ｽ・ｽ謫ｾ
		pChildWnd->GetWindowRect(&rectChild);

		// ・ｽ・ｽ・ｽ・ｽ・ｽﾉゑｿｽ・ｽ驍ｩ
		if (rectChild.PtInRect(pt)) {
			// ・ｽ・ｽ・ｽﾉ取得・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ`・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾎ、・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
			if (rectParent.top == 0) {
				// ・ｽﾅ擾ｿｽ・ｽﾌ鯉ｿｽ・ｽ
				rectParent = rectChild;
				nID = pChildWnd->GetDlgCtrlID();
			}
			else {
				if (rectChild.Width() < rectParent.Width()) {
					// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾌ鯉ｿｽ・ｽ
					rectParent = rectChild;
					nID = pChildWnd->GetDlgCtrlID();
				}
			}
		}

		// Siguiente
		pChildWnd = pChildWnd->GetNextWindow();
	}

	// nID・ｽ・ｽ・ｽr
	if (m_uMsgID == nID) {
		// Clase base
		return CPropertyPage::OnSetCursor(pWnd, nHitTest, nMsg);
	}
	m_uMsgID = nID;

	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽh・ｽA・ｽﾝ抵ｿｽ
	::GetMsg(m_uMsgID, strText);
	pStatic = (CStatic*)GetDlgItem(m_uHelpID);
	ASSERT(pStatic);
	pStatic->SetWindowText(strText);

	// Clase base
	return CPropertyPage::OnSetCursor(pWnd, nHitTest, nMsg);
}

//===========================================================================
//
//	・ｽ・ｽ{・ｽy・ｽ[・ｽW
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CBasicPage::CBasicPage()
{
	// Configurar siempre ID y Help
	m_dwID = MAKEID('B', 'A', 'S', 'C');
	m_nTemplate = IDD_BASICPAGE;
	m_uHelpID = IDC_BASIC_HELP;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CBasicPage, CConfigPage)
	ON_BN_CLICKED(IDC_BASIC_CPUFULLB, OnMPUFull)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CBasicPage::OnInitDialog()
{
	CString string;
	CButton *pButton;
	CComboBox *pComboBox;
	int i;

	// Clase base
	CConfigPage::OnInitDialog();

	// Sistema・ｽNBloquear
	pComboBox = (CComboBox*)GetDlgItem(IDC_BASIC_CLOCKC);
	ASSERT(pComboBox);
	for (i=0; i<6; i++) {
		::GetMsg((IDS_BASIC_CLOCK0 + i), string);
		pComboBox->AddString(string);
	}
	pComboBox->SetCurSel(m_pConfig->system_clock);

	// MPU・ｽt・ｽ・ｽ・ｽX・ｽs・ｽ[・ｽh
	pButton = (CButton*)GetDlgItem(IDC_BASIC_CPUFULLB);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->mpu_fullspeed);

	// VM・ｽt・ｽ・ｽ・ｽX・ｽs・ｽ[・ｽh
	pButton = (CButton*)GetDlgItem(IDC_BASIC_ALLFULLB);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->vm_fullspeed);

	// ・ｽ・ｽ・ｽC・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	pComboBox = (CComboBox*)GetDlgItem(IDC_BASIC_MEMORYC);
	ASSERT(pComboBox);
	for (i=0; i<6; i++) {
		::GetMsg((IDS_BASIC_MEMORY0 + i), string);
		pComboBox->AddString(string);
	}
	pComboBox->SetCurSel(m_pConfig->ram_size);

	// SRAM・ｽ・ｽ・ｽ・ｽ
	pButton = (CButton*)GetDlgItem(IDC_BASIC_MEMSWB);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->ram_sramsync);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CBasicPage::OnOK()
{
	CButton *pButton;
	CComboBox *pComboBox;

	// Sistema・ｽNBloquear
	pComboBox = (CComboBox*)GetDlgItem(IDC_BASIC_CLOCKC);
	ASSERT(pComboBox);
	m_pConfig->system_clock = pComboBox->GetCurSel();

	// MPU・ｽt・ｽ・ｽ・ｽX・ｽs・ｽ[・ｽh
	pButton = (CButton*)GetDlgItem(IDC_BASIC_CPUFULLB);
	ASSERT(pButton);
	m_pConfig->mpu_fullspeed = pButton->GetCheck();

	// VM・ｽt・ｽ・ｽ・ｽX・ｽs・ｽ[・ｽh
	pButton = (CButton*)GetDlgItem(IDC_BASIC_ALLFULLB);
	ASSERT(pButton);
	m_pConfig->vm_fullspeed = pButton->GetCheck();

	// ・ｽ・ｽ・ｽC・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	pComboBox = (CComboBox*)GetDlgItem(IDC_BASIC_MEMORYC);
	ASSERT(pComboBox);
	m_pConfig->ram_size = pComboBox->GetCurSel();

	// SRAM・ｽ・ｽ・ｽ・ｽ
	pButton = (CButton*)GetDlgItem(IDC_BASIC_MEMSWB);
	ASSERT(pButton);
	m_pConfig->ram_sramsync = pButton->GetCheck();

	// Clase base
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	MPU・ｽt・ｽ・ｽ・ｽX・ｽs・ｽ[・ｽh
//
//---------------------------------------------------------------------------
void CBasicPage::OnMPUFull()
{
	CSxSIPage *pSxSIPage;
	CButton *pButton;
	CString strWarn;

	// Obtener boton
	pButton = (CButton*)GetDlgItem(IDC_BASIC_CPUFULLB);
	ASSERT(pButton);

	// ・ｽI・ｽt・ｽﾈゑｿｽNo hacer nada
	if (pButton->GetCheck() == 0) {
		return;
	}

	// SxSI・ｽ・ｽ・ｽ・ｽ・ｽﾈゑｿｽNo hacer nada
	pSxSIPage = (CSxSIPage*)m_pSheet->SearchPage(MAKEID('S', 'X', 'S', 'I'));
	ASSERT(pSxSIPage);
	if (pSxSIPage->GetDrives(m_pConfig) == 0) {
		return;
	}

	// Aviso
	::GetMsg(IDS_MPUSXSI, strWarn);
	MessageBox(strWarn, NULL, MB_ICONINFORMATION | MB_OK);
}

//===========================================================================
//
//	Pagina de sonido
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSoundPage::CSoundPage()
{
	// Configurar siempre ID y Help
	m_dwID = MAKEID('S', 'N', 'D', ' ');
	m_nTemplate = IDD_SOUNDPAGE;
	m_uHelpID = IDC_SOUND_HELP;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSoundPage, CConfigPage)
	ON_WM_VSCROLL()
	ON_CBN_SELCHANGE(IDC_SOUND_DEVICEC, OnSelChange)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CSoundPage::OnInitDialog()
{
	CFrmWnd *pFrmWnd;
	CSound *pSound;
	CComboBox *pComboBox;
	CButton *pButton;
	CEdit *pEdit;
	CSpinButtonCtrl *pSpin;
	CString strName;
	CString strEdit;
	int i;

	// Clase base
	CConfigPage::OnInitDialog();

	// Obtener el componente de sonido
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	pSound = pFrmWnd->GetSound();
	ASSERT(pSound);

	// Cuadro combinado de dispositivosInicializacion
	pComboBox = (CComboBox*)GetDlgItem(IDC_SOUND_DEVICEC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	::GetMsg(IDS_SOUND_NOASSIGN, strName);
	pComboBox->AddString(strName);
	for (i=0; i<pSound->m_nDeviceNum; i++) {
		pComboBox->AddString(pSound->m_DeviceDescr[i]);
	}

	// Posicion del cursor del cuadro combinado
	if (m_pConfig->sample_rate == 0) {
		pComboBox->SetCurSel(0);
	}
	else {
		if (pSound->m_nDeviceNum <= m_pConfig->sound_device) {
			pComboBox->SetCurSel(0);
		}
		else {
			pComboBox->SetCurSel(m_pConfig->sound_device + 1);
		}
	}

	// ・ｽT・ｽ・ｽ・ｽv・ｽ・ｽ・ｽ・ｽ・ｽO・ｽ・ｽ・ｽ[・ｽgInicializacion
	for (i=0; i<5; i++) {
		pButton = (CButton*)GetDlgItem(IDC_SOUND_RATE0 + i);
		ASSERT(pButton);
		pButton->SetCheck(0);
	}
	if (m_pConfig->sample_rate > 0) {
		pButton = (CButton*)GetDlgItem(IDC_SOUND_RATE0 + m_pConfig->sample_rate - 1);
		ASSERT(pButton);
		pButton->SetCheck(1);
	}

	// ・ｽo・ｽb・ｽt・ｽ@・ｽT・ｽC・ｽYInicializacion
	pEdit = (CEdit*)GetDlgItem(IDC_SOUND_BUF1E);
	ASSERT(pEdit);
	strEdit.Format(_T("%d"), m_pConfig->primary_buffer * 10);
	pEdit->SetWindowText(strEdit);
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF1S);
	pSpin->SetBase(10);
	pSpin->SetRange(2, 100);
	pSpin->SetPos(m_pConfig->primary_buffer);

	// ・ｽ|・ｽ[・ｽ・ｽ・ｽ・ｽ・ｽO・ｽﾔ隔Inicializacion
	pEdit = (CEdit*)GetDlgItem(IDC_SOUND_BUF2E);
	ASSERT(pEdit);
	strEdit.Format(_T("%d"), m_pConfig->polling_buffer);
	pEdit->SetWindowText(strEdit);
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF2S);
	pSpin->SetBase(10);
	pSpin->SetRange(1, 100);
	pSpin->SetPos(m_pConfig->polling_buffer);

	// ADPCM・ｽ・ｽ・ｽ`・ｽ・ｽ・ｽInicializacion
	pButton = (CButton*)GetDlgItem(IDC_SOUND_INTERP);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->adpcm_interp);

	// Controles activados/desactivados
	m_bEnableCtrl = TRUE;
	if (m_pConfig->sample_rate == 0) {
		EnableControls(FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CSoundPage::OnOK()
{
	CComboBox *pComboBox;
	CButton *pButton;
	CSpinButtonCtrl *pSpin;
	int i;

	// Obtener dispositivo
	pComboBox = (CComboBox*)GetDlgItem(IDC_SOUND_DEVICEC);
	ASSERT(pComboBox);
	if (pComboBox->GetCurSel() == 0) {
		// Sin dispositivo seleccionado
		m_pConfig->sample_rate = 0;
	}
	else {
		// Dispositivo seleccionado
		m_pConfig->sound_device = pComboBox->GetCurSel() - 1;

		// Obtener frecuencia de muestreo
		for (i=0; i<5; i++) {
			pButton = (CButton*)GetDlgItem(IDC_SOUND_RATE0 + i);
			ASSERT(pButton);
			if (pButton->GetCheck() == 1) {
				m_pConfig->sample_rate = i + 1;
				break;
			}
		}
	}

	// Obtener buffer
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF1S);
	ASSERT(pSpin);
	m_pConfig->primary_buffer = LOWORD(pSpin->GetPos());
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF2S);
	ASSERT(pSpin);
	m_pConfig->polling_buffer = LOWORD(pSpin->GetPos());

	// Obtener interpolacion lineal ADPCM
	pButton = (CButton*)GetDlgItem(IDC_SOUND_INTERP);
	ASSERT(pButton);
	m_pConfig->adpcm_interp = pButton->GetCheck();

	// Clase base
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Desplazamiento vertical
//
//---------------------------------------------------------------------------
void CSoundPage::OnVScroll(UINT /*nSBCode*/, UINT nPos, CScrollBar* pBar)
{
	CEdit *pEdit;
	CSpinButtonCtrl *pSpin;
	CString strEdit;

	// ?Coincide con el control de giro (spin control)?
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF1S);
	if ((CWnd*)pBar == (CWnd*)pSpin) {
		// Reflejar en el cuadro de edicion
		pEdit = (CEdit*)GetDlgItem(IDC_SOUND_BUF1E);
		strEdit.Format(_T("%d"), nPos * 10);
		pEdit->SetWindowText(strEdit);
	}

	// ?Coincide con el control de giro (spin control)?
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SOUND_BUF2S);
	if ((CWnd*)pBar == (CWnd*)pSpin) {
		// Reflejar en el cuadro de edicion
		pEdit = (CEdit*)GetDlgItem(IDC_SOUND_BUF2E);
		strEdit.Format(_T("%d"), nPos);
		pEdit->SetWindowText(strEdit);
	}
}

//---------------------------------------------------------------------------
//
//	Cambio en el cuadro combinado
//
//---------------------------------------------------------------------------
void CSoundPage::OnSelChange()
{
	int i;
	CComboBox *pComboBox;
	CButton *pButton;

	pComboBox = (CComboBox*)GetDlgItem(IDC_SOUND_DEVICEC);
	ASSERT(pComboBox);
	if (pComboBox->GetCurSel() == 0) {
		EnableControls(FALSE);
	}
	else {
		EnableControls(TRUE);
	}

	// Considerar la configuracion de la frecuencia de muestreo
	if (m_bEnableCtrl) {
		// Si esta activado, basta con que uno este marcado
		for (i=0; i<5; i++) {
			pButton = (CButton*)GetDlgItem(IDC_SOUND_RATE0 + i);
			ASSERT(pButton);
			if (pButton->GetCheck() != 0) {
				return;
			}
		}

		// Ninguno esta marcado, se marca 62.5kHz
		pButton = (CButton*)GetDlgItem(IDC_SOUND_RATE4);
		ASSERT(pButton);
		pButton->SetCheck(1);
		return;
	}

	// Si esta desactivado, desmarcar todos
	for (i=0; i<5; i++) {
		pButton = (CButton*)GetDlgItem(IDC_SOUND_RATE0 + i);
		ASSERT(pButton);
		pButton->SetCheck(0);
	}
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CSoundPage::EnableControls(BOOL bEnable) 
{
	int i;
	CWnd *pWnd;

	ASSERT(this);

	// Verificacion de flags
	if (m_bEnableCtrl == bEnable) {
		return;
	}
	m_bEnableCtrl = bEnable;

	// Configurar todos los controles excepto Dispositivo y Ayuda
	for(i=0; ; i++) {
		// FinVerificacion
		if (ControlTable[i] == NULL) {
			break;
		}

		// Obtener control
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);
		pWnd->EnableWindow(bEnable);
	}
}

//---------------------------------------------------------------------------
//
//	Tabla de IDs de controles
//
//---------------------------------------------------------------------------
const UINT CSoundPage::ControlTable[] = {
	IDC_SOUND_RATEG,
	IDC_SOUND_RATE0,
	IDC_SOUND_RATE1,
	IDC_SOUND_RATE2,
	IDC_SOUND_RATE3,
	IDC_SOUND_RATE4,
	IDC_SOUND_BUFFERG,
	IDC_SOUND_BUF1L,
	IDC_SOUND_BUF1E,
	IDC_SOUND_BUF1S,
	IDC_SOUND_BUF1MS,
	IDC_SOUND_BUF2L,
	IDC_SOUND_BUF2E,
	IDC_SOUND_BUF2S,
	IDC_SOUND_BUF2MS,
	IDC_SOUND_OPTIONG,
	IDC_SOUND_INTERP,
	NULL
};

//===========================================================================
//
//	Pagina de volumen
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CVolPage::CVolPage()
{
	// Configurar siempre ID y Help
	m_dwID = MAKEID('V', 'O', 'L', ' ');
	m_nTemplate = IDD_VOLPAGE;
	m_uHelpID = IDC_VOL_HELP;

	// Objetos
	m_pSound = NULL;
	m_pOPMIF = NULL;
	m_pADPCM = NULL;
	m_pMIDI = NULL;

	// Temporizador (Timer)
	m_nTimerID = NULL;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CVolPage, CConfigPage)
	ON_WM_HSCROLL()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_VOL_FMC, OnFMCheck)
	ON_BN_CLICKED(IDC_VOL_ADPCMC, OnADPCMCheck)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CVolPage::OnInitDialog()
{
	CFrmWnd *pFrmWnd;
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	CString strLabel;
	CButton *pButton;
	int nPos;
	int nMax;

	// Clase base
	CConfigPage::OnInitDialog();

	// Obtener el componente de sonido
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pSound = pFrmWnd->GetSound();
	ASSERT(m_pSound);

	// Obtener OPMIF
	m_pOPMIF = (OPMIF*)::GetVM()->SearchDevice(MAKEID('O', 'P', 'M', ' '));
	ASSERT(m_pOPMIF);

	// Obtener ADPCM
	m_pADPCM = (ADPCM*)::GetVM()->SearchDevice(MAKEID('A', 'P', 'C', 'M'));
	ASSERT(m_pADPCM);

	// Obtener MIDI
	m_pMIDI = pFrmWnd->GetMIDI();
	ASSERT(m_pMIDI);

	// Volumen maestro
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_VOLS);
	ASSERT(pSlider);
	pSlider->SetRange(0, 100);
	nPos = m_pSound->GetMasterVol(nMax);
	if (nPos >= 0) {
		// Se puede ajustar el volumen
		pSlider->SetRange(0, nMax);
		pSlider->SetPos(nPos);
		pSlider->EnableWindow(TRUE);
		strLabel.Format(_T(" %d"), (nPos * 100) / nMax);
	}
	else {
		// No se puede ajustar el volumen
		pSlider->SetPos(0);
		pSlider->EnableWindow(FALSE);
		strLabel.Empty();
	}
	pStatic = (CStatic*)GetDlgItem(IDC_VOL_VOLN);
	pStatic->SetWindowText(strLabel);
	m_nMasterVol = nPos;
	m_nMasterOrg = nPos;

	// Nivel WAVE
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_MASTERS);
	ASSERT(pSlider);
	pSlider->SetRange(0, 100);
	::LockVM();
	nPos = m_pSound->GetVolume();
	::UnlockVM();
	pSlider->SetPos(nPos);
	strLabel.Format(_T(" %d"), nPos);
	pStatic = (CStatic*)GetDlgItem(IDC_VOL_MASTERN);
	pStatic->SetWindowText(strLabel);

	// Nivel MIDI
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_SEPS);
	ASSERT(pSlider);
	pSlider->SetRange(0, 0xffff);
	nPos = m_pMIDI->GetOutVolume();
	if (nPos >= 0) {
		// MIDI・ｽo・ｽﾍデ・ｽo・ｽC・ｽX・ｽﾍア・ｽN・ｽe・ｽB・ｽu・ｽ・ｽ・ｽ・ｽSe puede ajustar el volumen
		pSlider->SetPos(nPos);
		pSlider->EnableWindow(TRUE);
		strLabel.Format(_T(" %d"), ((nPos + 1) * 100) >> 16);
	}
	else {
		// MIDI・ｽo・ｽﾍデ・ｽo・ｽC・ｽX・ｽﾍア・ｽN・ｽe・ｽB・ｽu・ｽﾅなゑｿｽ・ｽA・ｽ・ｽ・ｽ・ｽNo se puede ajustar el volumen
		pSlider->SetPos(0);
		pSlider->EnableWindow(FALSE);
		strLabel.Empty();
	}
	pStatic = (CStatic*)GetDlgItem(IDC_VOL_SEPN);
	pStatic->SetWindowText(strLabel);
	m_nMIDIVol = nPos;
	m_nMIDIOrg = nPos;

	// Sintetizador FM
	pButton = (CButton*)GetDlgItem(IDC_VOL_FMC);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->fm_enable);
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_FMS);
	ASSERT(pSlider);
	pSlider->SetRange(0, 100);
	pSlider->SetPos(m_pConfig->fm_volume);
	strLabel.Format(_T(" %d"), m_pConfig->fm_volume);
	pStatic = (CStatic*)GetDlgItem(IDC_VOL_FMN);
	pStatic->SetWindowText(strLabel);

	// Sintetizador ADPCM
	pButton = (CButton*)GetDlgItem(IDC_VOL_ADPCMC);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->adpcm_enable);
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_ADPCMS);
	ASSERT(pSlider);
	pSlider->SetRange(0, 100);
	pSlider->SetPos(m_pConfig->adpcm_volume);
	strLabel.Format(_T(" %d"), m_pConfig->adpcm_volume);
	pStatic = (CStatic*)GetDlgItem(IDC_VOL_ADPCMN);
	pStatic->SetWindowText(strLabel);

	// Iniciar temporizador (fuego cada 100ms)
	m_nTimerID = SetTimer(IDD_VOLPAGE, 100, NULL);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Desplazamiento horizontal
//
//---------------------------------------------------------------------------
void CVolPage::OnHScroll(UINT /*nSBCode*/, UINT nPos, CScrollBar *pBar)
{
	UINT uID;
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	CString strLabel;

	ASSERT(this);
	ASSERT(pBar);

	// Conversion
	pSlider = (CSliderCtrl*)pBar;
	ASSERT(pSlider);

	// Verificacion
	switch (pSlider->GetDlgCtrlID()) {
		// Volumen maestroModificacion
		case IDC_VOL_VOLS:
			nPos = pSlider->GetPos();
			m_pSound->SetMasterVol(nPos);
			// Delegar la actualizacion a OnTimer
			OnTimer(m_nTimerID);
			return;

		// Nivel WAVEModificacion
		case IDC_VOL_MASTERS:
			// Modificacion
			nPos = pSlider->GetPos();
			::LockVM();
			m_pSound->SetVolume(nPos);
			::UnlockVM();

			// Actualizacion
			uID = IDC_VOL_MASTERN;
			strLabel.Format(_T(" %d"), nPos);
			break;

		// Nivel MIDIModificacion
		case IDC_VOL_SEPS:
			nPos = pSlider->GetPos();
			m_pMIDI->SetOutVolume(nPos);
			// Delegar la actualizacion a OnTimer
			OnTimer(m_nTimerID);
			return;

		// Volumen FMModificacion
		case IDC_VOL_FMS:
			// Modificacion
			nPos = pSlider->GetPos();
			::LockVM();
			m_pSound->SetFMVol(nPos);
			::UnlockVM();

			// Actualizacion
			uID = IDC_VOL_FMN;
			strLabel.Format(_T(" %d"), nPos);
			break;

		// Volumen ADPCMModificacion
		case IDC_VOL_ADPCMS:
			// Modificacion
			nPos = pSlider->GetPos();
			::LockVM();
			m_pSound->SetADPCMVol(nPos);
			::UnlockVM();

			// Actualizacion
			uID = IDC_VOL_ADPCMN;
			strLabel.Format(_T(" %d"), nPos);
			break;

		// Otros
		default:
			ASSERT(FALSE);
			return;
	}

	// Modificacion
	pStatic = (CStatic*)GetDlgItem(uID);
	ASSERT(pStatic);
	pStatic->SetWindowText(strLabel);
}

//---------------------------------------------------------------------------
//
//	Temporizador
//
//---------------------------------------------------------------------------
void CVolPage::OnTimer(UINT /*nTimerID*/)
{
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	CString strLabel;
	int nPos;
	int nMax;

	// Obtener volumen principal
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_VOLS);
	ASSERT(pSlider);
	nPos = m_pSound->GetMasterVol(nMax);

	// Comparacion de volumen
	if (nPos != m_nMasterVol) {
		m_nMasterVol = nPos;

		// Procesamiento
		if (nPos >= 0) {
			// Activacion
			pSlider->SetPos(nPos);
			pSlider->EnableWindow(TRUE);
			strLabel.Format(_T(" %d"), (nPos * 100) / nMax);
		}
		else {
			// Desactivacion
			pSlider->SetPos(0);
			pSlider->EnableWindow(FALSE);
			strLabel.Empty();
		}

		pStatic = (CStatic*)GetDlgItem(IDC_VOL_VOLN);
		pStatic->SetWindowText(strLabel);
	}

	// MIDI
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_SEPS);
	nPos = m_pMIDI->GetOutVolume();

	// Comparacion MIDI
	if (nPos != m_nMIDIVol) {
		m_nMIDIVol = nPos;

		// Procesamiento
		if (nPos >= 0) {
			// Activacion
			pSlider->SetPos(nPos);
			pSlider->EnableWindow(TRUE);
			strLabel.Format(_T(" %d"), ((nPos + 1) * 100) >> 16);
		}
		else {
			// Desactivacion
			pSlider->SetPos(0);
			pSlider->EnableWindow(FALSE);
			strLabel.Empty();
		}

		pStatic = (CStatic*)GetDlgItem(IDC_VOL_SEPN);
		pStatic->SetWindowText(strLabel);
	}
}

//---------------------------------------------------------------------------
//
//	Sintetizador FMVerificacion
//
//---------------------------------------------------------------------------
void CVolPage::OnFMCheck()
{
	CButton *pButton;

	pButton = (CButton*)GetDlgItem(IDC_VOL_FMC);
	ASSERT(pButton);
	if (pButton->GetCheck()) {
		m_pOPMIF->EnableFM(TRUE);
	}
	else {
		m_pOPMIF->EnableFM(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Sintetizador ADPCMVerificacion
//
//---------------------------------------------------------------------------
void CVolPage::OnADPCMCheck()
{
	CButton *pButton;

	pButton = (CButton*)GetDlgItem(IDC_VOL_ADPCMC);
	ASSERT(pButton);
	if (pButton->GetCheck()) {
		m_pADPCM->EnableADPCM(TRUE);
	}
	else {
		m_pADPCM->EnableADPCM(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CVolPage::OnOK()
{
	CSliderCtrl *pSlider;
	CButton *pButton;

	// Temporizador・ｽ・ｽ~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// Nivel WAVE
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_MASTERS);
	ASSERT(pSlider);
	m_pConfig->master_volume = pSlider->GetPos();

	// FM activado
	pButton = (CButton*)GetDlgItem(IDC_VOL_FMC);
	ASSERT(pButton);
	m_pConfig->fm_enable = pButton->GetCheck();

	// Volumen FM
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_FMS);
	ASSERT(pSlider);
	m_pConfig->fm_volume = pSlider->GetPos();

	// ADPCM activado
	pButton = (CButton*)GetDlgItem(IDC_VOL_ADPCMC);
	ASSERT(pButton);
	m_pConfig->adpcm_enable = pButton->GetCheck();

	// Volumen ADPCM
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_VOL_ADPCMS);
	ASSERT(pSlider);
	m_pConfig->adpcm_volume = pSlider->GetPos();

	// Clase base
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CVolPage::OnCancel()
{
	// Temporizador・ｽ・ｽ~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// Restablecer a los valores originales (datos CONFIG)
	::LockVM();
	m_pSound->SetVolume(m_pConfig->master_volume);
	m_pOPMIF->EnableFM(m_pConfig->fm_enable);
	m_pSound->SetFMVol(m_pConfig->fm_volume);
	m_pADPCM->EnableADPCM(m_pConfig->adpcm_enable);
	m_pSound->SetADPCMVol(m_pConfig->adpcm_volume);
	::UnlockVM();

	// Restablecer a los valores originales (mezclador)
	if (m_nMasterOrg >= 0) {
		m_pSound->SetMasterVol(m_nMasterOrg);
	}
	if (m_nMIDIOrg >= 0) {
		m_pMIDI->SetOutVolume(m_nMIDIOrg);
	}

	// Clase base
	CConfigPage::OnCancel();
}

//===========================================================================
//
//	Pagina del teclado
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CKbdPage::CKbdPage()
{
	CFrmWnd *pWnd;

	// Configurar siempre ID y Help
	m_dwID = MAKEID('K', 'E', 'Y', 'B');
	m_nTemplate = IDD_KBDPAGE;
	m_uHelpID = IDC_KBD_HELP;

	// Obtener entrada
	pWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pWnd);
	m_pInput = pWnd->GetInput();
	ASSERT(m_pInput);
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CKbdPage, CConfigPage)
	ON_COMMAND(IDC_KBD_EDITB, OnEdit)
	ON_COMMAND(IDC_KBD_DEFB, OnDefault)
	ON_BN_CLICKED(IDC_KBD_NOCONB, OnConnect)
	ON_NOTIFY(NM_CLICK, IDC_KBD_MAPL, OnClick)
	ON_NOTIFY(NM_RCLICK, IDC_KBD_MAPL, OnRClick)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CKbdPage::OnInitDialog()
{
	CButton *pButton;
	CListCtrl *pListCtrl;
	CClientDC *pDC;
	TEXTMETRIC tm;
	CString strText;
	LPCTSTR lpszName;
	int nKey;
	LONG cx;

	// Clase base
	CConfigPage::OnInitDialog();

	// Hacer copia de seguridad del mapa de teclas
	m_pInput->GetKeyMap(m_dwBackup);
	memcpy(m_dwEdit, m_dwBackup, sizeof(m_dwBackup));

	// Obtener metricas de texto
	pDC = new CClientDC(this);
	::GetTextMetrics(pDC->m_hDC, &tm);
	delete pDC;
	cx = tm.tmAveCharWidth;

	// Columnas del control de lista
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_KBD_MAPL);
	ASSERT(pListCtrl);
	if (::IsJapanese()) {
		// Japones
		::GetMsg(IDS_KBD_NO, strText);
		pListCtrl->InsertColumn(0, strText, LVCFMT_LEFT, cx * 4, 0);
		::GetMsg(IDS_KBD_KEYTOP, strText);
		pListCtrl->InsertColumn(1, strText, LVCFMT_LEFT, cx * 10, 1);
		::GetMsg(IDS_KBD_DIRECTX, strText);
		pListCtrl->InsertColumn(2, strText, LVCFMT_LEFT, cx * 22, 2);
	}
	else {
		// Ingles
		::GetMsg(IDS_KBD_NO, strText);
		pListCtrl->InsertColumn(0, strText, LVCFMT_LEFT, cx * 5, 0);
		::GetMsg(IDS_KBD_KEYTOP, strText);
		pListCtrl->InsertColumn(1, strText, LVCFMT_LEFT, cx * 12, 1);
		::GetMsg(IDS_KBD_DIRECTX, strText);
		pListCtrl->InsertColumn(2, strText, LVCFMT_LEFT, cx * 18, 2);
	}

	// Opcion de fila completa para el control de lista (COMCTL32.DLL v4.71+)
	pListCtrl->SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

	// Crear items (la informacion del lado X68000 es fija independientemente del mapeo)
	pListCtrl->DeleteAllItems();
	cx = 0;
	for (nKey=0; nKey<=0x73; nKey++) {
		// Obtener el nombre de la tecla desde CKeyDispWnd
		lpszName = m_pInput->GetKeyName(nKey);
		if (lpszName) {
			// Esta tecla es valida
			strText.Format(_T("%02X"), nKey);
			pListCtrl->InsertItem(cx, strText);
			pListCtrl->SetItemText(cx, 1, lpszName);
			pListCtrl->SetItemData(cx, (DWORD)nKey);
			cx++;
		}
	}

	// ・ｽ・ｽ・ｽ|・ｽ[・ｽgActualizacion
	UpdateReport();

	// Conexion
	pButton = (CButton*)GetDlgItem(IDC_KBD_NOCONB);
	ASSERT(pButton);
	pButton->SetCheck(!m_pConfig->kbd_connect);

	// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion
	m_bEnableCtrl = TRUE;
	if (!m_pConfig->kbd_connect) {
		EnableControls(FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CKbdPage::OnOK()
{
	CButton *pButton;

	// Configurar mapa de teclas
	m_pInput->SetKeyMap(m_dwEdit);

	// Conexion
	pButton = (CButton*)GetDlgItem(IDC_KBD_NOCONB);
	ASSERT(pButton);
	m_pConfig->kbd_connect = !(pButton->GetCheck());

	// Clase base
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CKbdPage::OnCancel()
{
	// Restaurar mapa de teclas desde la copia de seguridad
	m_pInput->SetKeyMap(m_dwBackup);

	// Clase base
	CConfigPage::OnCancel();
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽ|・ｽ[・ｽgActualizacion
//
//---------------------------------------------------------------------------
void FASTCALL CKbdPage::UpdateReport()
{
	CListCtrl *pListCtrl;
	int nX68;
	int nWin;
	int nItem;
	CString strNext;
	CString strPrev;
	LPCTSTR lpszName;

	ASSERT(this);

	// Obtener control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_KBD_MAPL);
	ASSERT(pListCtrl);

	// Fila del control de lista
	nItem = 0;
	for (nX68=0; nX68<=0x73; nX68++) {
		// Obtener el nombre de la tecla desde CKeyDispWnd
		lpszName = m_pInput->GetKeyName(nX68);
		if (lpszName) {
			// ・ｽL・ｽ・ｽ・ｽﾈキ・ｽ[・ｽ・ｽ・ｽ・ｽ・ｽ・ｽBInicializacion
			strNext.Empty();

			// Establecer si hay una asignacion
			for (nWin=0; nWin<0x100; nWin++) {
				if (nX68 == (int)m_dwEdit[nWin]) {
					// Obtener nombre
					lpszName = m_pInput->GetKeyID(nWin);
					strNext = lpszName;
					break;
				}
			}

			// Sobreescribir si es diferente
			strPrev = pListCtrl->GetItemText(nItem, 2);
			if (strPrev != strNext) {
				pListCtrl->SetItemText(nItem, 2, strNext);
			}

			// Al siguiente item
			nItem++;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Editar
//
//---------------------------------------------------------------------------
void CKbdPage::OnEdit()
{
	CKbdMapDlg dlg(this, m_dwEdit);

	ASSERT(this);

	// Ejecutar dialogo
	dlg.DoModal();

	// Mostrar・ｽ・ｽActualizacion
	UpdateReport();
}

//---------------------------------------------------------------------------
//
//	Restablecer valores predeterminados
//
//---------------------------------------------------------------------------
void CKbdPage::OnDefault()
{
	ASSERT(this);

	// Poner el mapa 106 en el propio buffer y establecerlo
	m_pInput->SetDefaultKeyMap(m_dwEdit);
	m_pInput->SetKeyMap(m_dwEdit);

	// Mostrar・ｽ・ｽActualizacion
	UpdateReport();
}

//---------------------------------------------------------------------------
//
//	Clic en item
//
//---------------------------------------------------------------------------
void CKbdPage::OnClick(NMHDR * /*pNMHDR*/, LRESULT * /*pResult*/)
{
	CListCtrl *pListCtrl;
	int nCount;
	int nItem;
	int nKey;
	int nPrev;
	int i;
	CKeyinDlg dlg(this);

	// ・ｽ・ｽ・ｽX・ｽgObtener control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_KBD_MAPL);
	ASSERT(pListCtrl);

	// Obtener conteo
	nCount = pListCtrl->GetItemCount();

	// Obtener el indice seleccionado
	for (nItem=0; nItem<nCount; nItem++) {
		if (pListCtrl->GetItemState(nItem, LVIS_SELECTED)) {
			break;
		}
	}
	if (nItem >= nCount) {
		return;
	}

	// Obtener los datos apuntados por ese indice
	nKey = (int)pListCtrl->GetItemData(nItem);
	ASSERT((nKey >= 0) && (nKey <= 0x73));

	// Iniciar configuracion
	dlg.m_nTarget = nKey;
	dlg.m_nKey = 0;

	// Configurar si existe la tecla de Windows correspondiente
	nPrev = -1;
	for (i=0; i<0x100; i++) {
		if (m_dwEdit[i] == (DWORD)nKey) {
			dlg.m_nKey = i;
			nPrev = i;
			break;
		}
	}

	// Ejecutar dialogo
	m_pInput->EnableKey(FALSE);
	if (dlg.DoModal() != IDOK) {
		m_pInput->EnableKey(TRUE);
		return;
	}
	m_pInput->EnableKey(TRUE);

	// Configurar el mapa de teclas
	if (nPrev >= 0) {
		m_dwEdit[nPrev] = 0;
	}
	m_dwEdit[dlg.m_nKey] = (DWORD)nKey;

	// SHIFT・ｽL・ｽ[・ｽ・ｽOProcesamiento
	if (nPrev == DIK_LSHIFT) {
		m_dwEdit[DIK_RSHIFT] = 0;
	}
	if (dlg.m_nKey == DIK_LSHIFT) {
		m_dwEdit[DIK_RSHIFT] = (DWORD)nKey;
	}

	// Mostrar・ｽ・ｽActualizacion
	UpdateReport();
}

//---------------------------------------------------------------------------
//
//	Clic derecho en item
//
//---------------------------------------------------------------------------
void CKbdPage::OnRClick(NMHDR * /*pNMHDR*/, LRESULT * /*pResult*/)
{
	CListCtrl *pListCtrl;
	int nCount;
	int nItem;
	int nKey;
	int nWin;
	int i;
	CString strText;
	CString strMsg;

	// ・ｽ・ｽ・ｽX・ｽgObtener control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_KBD_MAPL);
	ASSERT(pListCtrl);

	// Obtener conteo
	nCount = pListCtrl->GetItemCount();

	// Obtener el indice seleccionado
	for (nItem=0; nItem<nCount; nItem++) {
		if (pListCtrl->GetItemState(nItem, LVIS_SELECTED)) {
			break;
		}
	}
	if (nItem >= nCount) {
		return;
	}

	// Obtener los datos apuntados por ese indice
	nKey = (int)pListCtrl->GetItemData(nItem);
	ASSERT((nKey >= 0) && (nKey <= 0x73));

	// ?Existe la tecla de Windows correspondiente?
	nWin = -1;
	for (i=0; i<0x100; i++) {
		if (m_dwEdit[i] == (DWORD)nKey) {
			nWin = i;
			break;
		}
	}
	if (nWin < 0) {
		// No esta mapeado
		return;
	}

	// ・ｽ・ｽ・ｽb・ｽZ・ｽ[・ｽW・ｽ{・ｽb・ｽN・ｽX・ｽﾅ、Eliminar・ｽﾌ有・ｽ・ｽ・ｽ・ｽVerificacion
	::GetMsg(IDS_KBD_DELMSG, strText);
	strMsg.Format(strText, nKey, m_pInput->GetKeyID(nWin));
	::GetMsg(IDS_KBD_DELTITLE, strText);
	if (MessageBox(strMsg, strText, MB_ICONQUESTION | MB_YESNO) != IDYES) {
		return;
	}

	// ・ｽY・ｽ・ｽ・ｽ・ｽ・ｽ・ｽWindows・ｽL・ｽ[・ｽ・ｽEliminar
	m_dwEdit[nWin] = 0;

	// SHIFT・ｽL・ｽ[・ｽ・ｽOProcesamiento
	if (nWin == DIK_LSHIFT) {
		m_dwEdit[DIK_RSHIFT] = 0;
	}

	// Mostrar・ｽ・ｽActualizacion
	UpdateReport();
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽConexion
//
//---------------------------------------------------------------------------
void CKbdPage::OnConnect()
{
	CButton *pButton;

	pButton = (CButton*)GetDlgItem(IDC_KBD_NOCONB);
	ASSERT(pButton);

	// Controles activados/desactivados
	if (pButton->GetCheck() == 1) {
		EnableControls(FALSE);
	}
	else {
		EnableControls(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CKbdPage::EnableControls(BOOL bEnable) 
{
	int i;
	CWnd *pWnd;

	ASSERT(this);

	// Verificacion de flags
	if (m_bEnableCtrl == bEnable) {
		return;
	}
	m_bEnableCtrl = bEnable;

	// Configurar todos los controles excepto ID de placa y Ayuda
	for(i=0; ; i++) {
		// FinVerificacion
		if (ControlTable[i] == NULL) {
			break;
		}

		// Obtener control
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);
		pWnd->EnableWindow(bEnable);
	}
}

//---------------------------------------------------------------------------
//
//	Tabla de IDs de controles
//
//---------------------------------------------------------------------------
const UINT CKbdPage::ControlTable[] = {
	IDC_KBD_MAPG,
	IDC_KBD_MAPL,
	IDC_KBD_EDITB,
	IDC_KBD_DEFB,
	NULL
};

//===========================================================================
//
//	Teclado・ｽ}・ｽb・ｽvEditar・ｽ_・ｽC・ｽA・ｽ・ｽ・ｽO
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CKbdMapDlg::CKbdMapDlg(CWnd *pParent, DWORD *pMap) : CDialog(IDD_KBDMAPDLG, pParent)
{
	CFrmWnd *pFrmWnd;

	// Editar・ｽf・ｽ[・ｽ^・ｽ・ｽ・ｽL・ｽ・ｽ
	ASSERT(pMap);
	m_pEditMap = pMap;

	// Ingles・ｽﾂ具ｿｽ・ｽﾖの対会ｿｽ
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_KBDMAPDLG);
		m_nIDHelp = IDD_US_KBDMAPDLG;
	}

	// Obtener entrada
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pInput = pFrmWnd->GetInput();
	ASSERT(m_pInput);

	// Sin mensaje de estado
	m_strStat.Empty();
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
#if !defined(WM_KICKIDLE)
#define WM_KICKIDLE		0x36a
#endif	// WM_KICKIDLE
BEGIN_MESSAGE_MAP(CKbdMapDlg, CDialog)
	ON_WM_PAINT()
	ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
	ON_MESSAGE(WM_APP, OnApp)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	・ｽ_・ｽC・ｽA・ｽ・ｽ・ｽOInicializacion
//
//---------------------------------------------------------------------------
BOOL CKbdMapDlg::OnInitDialog()
{
	LONG cx;
	LONG cy;
	CRect rectClient;
	CRect rectWnd;
	CStatic *pStatic;

	// Clase base
	CDialog::OnInitDialog();

	// Obtener el tama?o del cliente
	GetClientRect(&rectClient);

	// Obtener la altura de la barra de estado
	pStatic = (CStatic*)GetDlgItem(IDC_KBDMAP_STAT);
	ASSERT(pStatic);
	pStatic->GetWindowRect(&rectWnd);

	// Calcular la diferencia (se asume > 0)
	cx = 616 - rectClient.Width();
	ASSERT(cx > 0);
	cy = (140 + rectWnd.Height()) - rectClient.Height();
	ASSERT(cy > 0);

	// Ampliar por cx, cy
	GetWindowRect(&rectWnd);
	SetWindowPos(&wndTop, 0, 0, rectWnd.Width() + cx, rectWnd.Height() + cy, SWP_NOMOVE);

	// ・ｽX・ｽe・ｽ[・ｽ^・ｽX・ｽo・ｽ[・ｽ・ｽ・ｽﾉ移難ｿｽ・ｽAEliminar
	pStatic->GetWindowRect(&rectClient);
	ScreenToClient(&rectClient);
	pStatic->SetWindowPos(&wndTop, 0, 140,
					rectClient.Width() + cx, rectClient.Height(), SWP_NOZORDER);
	pStatic->GetWindowRect(&m_rectStat);
	ScreenToClient(&m_rectStat);
	pStatic->DestroyWindow();

	// Mover la ventana de visualizacion
	pStatic = (CStatic*)GetDlgItem(IDC_KBDMAP_DISP);
	ASSERT(pStatic);
	pStatic->GetWindowRect(&rectWnd);
	ScreenToClient(&rectWnd);
	pStatic->SetWindowPos(&wndTop, 0, 0, 616, 140, SWP_NOZORDER);

	// Colocar CKeyDispWnd en la posicion de la ventana de visualizacion
	pStatic->GetWindowRect(&rectWnd);
	ScreenToClient(&rectWnd);
	pStatic->DestroyWindow();
	m_pDispWnd = new CKeyDispWnd;
	m_pDispWnd->Create(NULL, NULL, WS_CHILD | WS_VISIBLE,
					rectWnd, this, 0, NULL);

	// Centrar ventana
	CenterWindow(GetParent());

	// Desactivar IME
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Prohibir entrada de teclado
	ASSERT(m_pInput);
	m_pInput->EnableKey(FALSE);

	// Cargar mensaje guia
	::GetMsg(IDS_KBDMAP_GUIDE, m_strGuide);

	// Llamar a OnKickIdle al final (mostrar el estado actual desde el inicio)
	OnKickIdle(0, 0);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	OK
//
//---------------------------------------------------------------------------
void CKbdMapDlg::OnOK()
{
	// [CR]・ｽﾉゑｿｽ・ｽFin・ｽ・ｽ}・ｽ・ｽ
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CKbdMapDlg::OnCancel()
{
	// Tecla habilitada
	m_pInput->EnableKey(TRUE);

	// Clase base
	CDialog::OnCancel();
}

//---------------------------------------------------------------------------
//
//	Dibujar dialogo
//
//---------------------------------------------------------------------------
void CKbdMapDlg::OnPaint()
{
	CPaintDC dc(this);

	// Delegar a OnDraw
	OnDraw(&dc);
}

//---------------------------------------------------------------------------
//
//	Sub-rutina de dibujo
//
//---------------------------------------------------------------------------
void FASTCALL CKbdMapDlg::OnDraw(CDC *pDC)
{
	CFont *pFont;

	ASSERT(this);
	ASSERT(pDC);

	// Configuracion de colores
	pDC->SetTextColor(::GetSysColor(COLOR_BTNTEXT));
	pDC->SetBkColor(::GetSysColor(COLOR_3DFACE));

	// Configuracion de fuentes
	pFont = (CFont*)pDC->SelectStockObject(DEFAULT_GUI_FONT);
	ASSERT(pFont);

	pDC->FillSolidRect(m_rectStat, ::GetSysColor(COLOR_3DFACE));
	pDC->DrawText(m_strStat, m_rectStat,
				DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

	// Restaurar fuente
	pDC->SelectObject(pFont);
}

//---------------------------------------------------------------------------
//
//	Ocioso (idle)Procesamiento
//
//---------------------------------------------------------------------------
LONG CKbdMapDlg::OnKickIdle(UINT /*uParam*/, LONG /*lParam*/)
{
	BOOL bBuf[0x100];
	BOOL bFlg[0x100];
	int nWin;
	DWORD dwCode;
	CKeyDispWnd *pWnd;

	// Obtener estado de las teclas
	ASSERT(m_pInput);
	m_pInput->GetKeyBuf(bBuf);

	// Limpiar flags de teclas temporalmente
	memset(bFlg, 0, sizeof(bFlg));

	// ・ｽ・ｽ・ｽﾝのマ・ｽb・ｽv・ｽﾉ従・ｽ・ｽ・ｽﾄ、Conversion・ｽ\・ｽ・ｽ・ｽ・ｽ・ｽ
	for (nWin=0; nWin<0x100; nWin++) {
		// ?Esta la tecla presionada?
		if (bBuf[nWin]) {
			// Obtener codigo
			dwCode = m_pEditMap[nWin];
			if (dwCode != 0) {
				// La tecla esta pulsada y asignada, configurar buffer de teclado
				bFlg[dwCode] = TRUE;
			}
		}
	}

	// SHIFT・ｽL・ｽ[・ｽ・ｽOProcesamiento(L,R・ｽ・ｽ・ｽ・ｽ・ｽ墲ｹ・ｽ・ｽ)
	bFlg[0x74] = bFlg[0x70];

	// Refrescar (dibujar)
	pWnd = (CKeyDispWnd*)m_pDispWnd;
	pWnd->Refresh(bFlg);

	return 0;
}

//---------------------------------------------------------------------------
//
//	Notificacion de ventanas subordinadas
//
//---------------------------------------------------------------------------
LONG CKbdMapDlg::OnApp(UINT uParam, LONG lParam)
{
	CKeyinDlg dlg(this);
	int nPrev;
	int nWin;
	CString strText;
	CString strName;
	CClientDC *pDC;

	ASSERT(this);
	ASSERT(uParam <= 0x73);

	// Distribucion
	switch (lParam) {
		// Boton izquierdo presionado
		case WM_LBUTTONDOWN:
			// ・ｽ^・ｽ[・ｽQ・ｽb・ｽg・ｽ・ｽAsignacion・ｽL・ｽ[・ｽ・ｽInicializacion
			dlg.m_nTarget = uParam;
			dlg.m_nKey = 0;

			// Configurar si existe la tecla de Windows correspondiente
			nPrev = -1;
			for (nWin=0; nWin<0x100; nWin++) {
				if (m_pEditMap[nWin] == uParam) {
					dlg.m_nKey = nWin;
					nPrev = nWin;
					break;
				}
			}

			// Ejecutar dialogo
			if (dlg.DoModal() != IDOK) {
				return 0;
			}

			// Configurar el mapa de teclas
			m_pEditMap[dlg.m_nKey] = uParam;
			if (nPrev >= 0) {
				m_pEditMap[nPrev] = 0;
			}

			// SHIFT・ｽL・ｽ[・ｽ・ｽOProcesamiento
			if (nPrev == DIK_LSHIFT) {
				m_pEditMap[DIK_RSHIFT] = 0;
			}
			if (dlg.m_nKey == DIK_LSHIFT) {
				m_pEditMap[DIK_RSHIFT] = uParam;
			}
			break;

		// Boton izquierdo soltado
		case WM_LBUTTONUP:
			break;

		// Boton derecho presionado
		case WM_RBUTTONDOWN:
			// Configurar si existe la tecla de Windows correspondiente
			nPrev = -1;
			for (nWin=0; nWin<0x100; nWin++) {
				if (m_pEditMap[nWin] == uParam) {
					dlg.m_nKey = nWin;
					nPrev = nWin;
					break;
				}
			}
			if (nPrev < 0) {
				break;
			}

			// ・ｽ・ｽ・ｽb・ｽZ・ｽ[・ｽW・ｽ{・ｽb・ｽN・ｽX・ｽﾅ、Eliminar・ｽﾌ有・ｽ・ｽ・ｽ・ｽVerificacion
			::GetMsg(IDS_KBD_DELMSG, strName);
			strText.Format(strName, uParam, m_pInput->GetKeyID(nWin));
			::GetMsg(IDS_KBD_DELTITLE, strName);
			if (MessageBox(strText, strName, MB_ICONQUESTION | MB_YESNO) != IDYES) {
				break;
			}

			// ・ｽY・ｽ・ｽ・ｽ・ｽ・ｽ・ｽWindows・ｽL・ｽ[・ｽ・ｽEliminar
			m_pEditMap[nWin] = 0;

			// SHIFT・ｽL・ｽ[・ｽ・ｽOProcesamiento
			if (nWin == DIK_LSHIFT) {
				m_pEditMap[DIK_RSHIFT] = 0;
			}
			break;

		// Boton derecho soltado
		case WM_RBUTTONUP:
			break;

		// Movimiento del raton
		case WM_MOUSEMOVE:
			// Configuracion del mensaje inicial
			strText = m_strGuide;

			// Cuando la tecla tiene el foco
			if (uParam != 0) {
				// Primero mostrar la tecla X68000
				strText.Format(_T("Key%02X  "), uParam);
				strText += m_pInput->GetKeyName(uParam);

				// ・ｽY・ｽ・ｽ・ｽ・ｽ・ｽ・ｽWindows・ｽL・ｽ[・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽAgregar
				for (nWin=0; nWin<0x100; nWin++) {
					if (m_pEditMap[nWin] == uParam) {
						// Habia una tecla de Windows
						strName = m_pInput->GetKeyID(nWin);
						strText += _T("    (");
						strText += strName;
						strText += _T(")");
						break;
					}
				}
			}

			// Configuracion de mensaje
			m_strStat = strText;
			pDC = new CClientDC(this);
			OnDraw(pDC);
			delete pDC;
			break;

		// Otros(・ｽ・ｽ・ｽ閧ｦ・ｽﾈゑｿｽ)
		default:
			ASSERT(FALSE);
			break;
	}

	return 0;
}

//===========================================================================
//
//	Dialogo de entrada de teclas
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CKeyinDlg::CKeyinDlg(CWnd *pParent) : CDialog(IDD_KEYINDLG, pParent)
{
	CFrmWnd *pFrmWnd;

	// Ingles・ｽﾂ具ｿｽ・ｽﾖの対会ｿｽ
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_KEYINDLG);
		m_nIDHelp = IDD_US_KEYINDLG;
	}

	// Obtener entrada
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pInput = pFrmWnd->GetInput();
	ASSERT(m_pInput);
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
#if !defined(WM_KICKIDLE)
#define WM_KICKIDLE		0x36a
#endif	// WM_KICKIDLE
BEGIN_MESSAGE_MAP(CKeyinDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_GETDLGCODE()
	ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	・ｽ_・ｽC・ｽA・ｽ・ｽ・ｽOInicializacion
//
//---------------------------------------------------------------------------
BOOL CKeyinDlg::OnInitDialog()
{
	CStatic *pStatic;
	CString string;

	// Clase base
	CDialog::OnInitDialog();

	// Desactivar IME
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Pasar teclas actuales al buffer
	ASSERT(m_pInput);
	m_pInput->GetKeyBuf(m_bKey);

	// ・ｽK・ｽC・ｽh・ｽ・ｽ`・ｽ・ｽProcesamiento
	pStatic = (CStatic*)GetDlgItem(IDC_KEYIN_LABEL);
	ASSERT(pStatic);
	pStatic->GetWindowText(string);
	m_GuideString.Format(string, m_nTarget);
	pStatic->GetWindowRect(&m_GuideRect);
	ScreenToClient(&m_GuideRect);
	pStatic->DestroyWindow();

	// Asignacion・ｽ・ｽ`・ｽ・ｽProcesamiento
	pStatic = (CStatic*)GetDlgItem(IDC_KEYIN_STATIC);
	ASSERT(pStatic);
	pStatic->GetWindowText(m_AssignString);
	pStatic->GetWindowRect(&m_AssignRect);
	ScreenToClient(&m_AssignRect);
	pStatic->DestroyWindow();

	// ・ｽL・ｽ[・ｽ・ｽ`・ｽ・ｽProcesamiento
	pStatic = (CStatic*)GetDlgItem(IDC_KEYIN_KEY);
	ASSERT(pStatic);
	pStatic->GetWindowText(m_KeyString);
	if (m_nKey != 0) {
		m_KeyString = m_pInput->GetKeyID(m_nKey);
	}
	pStatic->GetWindowRect(&m_KeyRect);
	ScreenToClient(&m_KeyRect);
	pStatic->DestroyWindow();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	OK
//
//---------------------------------------------------------------------------
void CKeyinDlg::OnOK()
{
	// [CR]・ｽﾉゑｿｽ・ｽFin・ｽ・ｽ}・ｽ・ｽ
}

//---------------------------------------------------------------------------
//
//	・ｽ_・ｽC・ｽA・ｽ・ｽ・ｽOObtener codigo
//
//---------------------------------------------------------------------------
UINT CKeyinDlg::OnGetDlgCode()
{
	// Habilitar la recepcion de mensajes de teclado
	return DLGC_WANTMESSAGE;
}

//---------------------------------------------------------------------------
//
//	Dibujar
//
//---------------------------------------------------------------------------
void CKeyinDlg::OnPaint()
{
	CPaintDC dc(this);
	CDC mDC;
	CRect rect;
	CBitmap Bitmap;
	CBitmap *pBitmap;

	// Crear DC en memoria
	VERIFY(mDC.CreateCompatibleDC(&dc));

	// Crear mapa de bits compatible
	GetClientRect(&rect);
	VERIFY(Bitmap.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height()));
	pBitmap = mDC.SelectObject(&Bitmap);
	ASSERT(pBitmap);

	// Limpiar fondo
	mDC.FillSolidRect(&rect, ::GetSysColor(COLOR_3DFACE));

	// Dibujar
	OnDraw(&mDC);

	// BitBlt
	VERIFY(dc.BitBlt(0, 0, rect.Width(), rect.Height(), &mDC, 0, 0, SRCCOPY));

	// ・ｽr・ｽb・ｽg・ｽ}・ｽb・ｽvFin
	VERIFY(mDC.SelectObject(pBitmap));
	VERIFY(Bitmap.DeleteObject());

	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽDCFin
	VERIFY(mDC.DeleteDC());
}

//---------------------------------------------------------------------------
//
//	Sub-rutina de dibujo
//
//---------------------------------------------------------------------------
void FASTCALL CKeyinDlg::OnDraw(CDC *pDC)
{
	CFont *pFont;

	ASSERT(this);
	ASSERT(pDC);

	// Configuracion de colores
	pDC->SetTextColor(::GetSysColor(COLOR_BTNTEXT));
	pDC->SetBkColor(::GetSysColor(COLOR_3DFACE));

	// Configuracion de fuentes
	pFont = (CFont*)pDC->SelectStockObject(DEFAULT_GUI_FONT);
	ASSERT(pFont);

	// Mostrar
	pDC->DrawText(m_GuideString, m_GuideRect,
					DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
	pDC->DrawText(m_AssignString, m_AssignRect,
					DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
	pDC->DrawText(m_KeyString, m_KeyRect,
					DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);

	// Restaurar fuente(Objetos・ｽ・ｽEliminar・ｽ・ｽ・ｽﾈゑｿｽ・ｽﾄよい)
	pDC->SelectObject(pFont);
}

//---------------------------------------------------------------------------
//
//	Ocioso (idle)
//
//---------------------------------------------------------------------------
LONG CKeyinDlg::OnKickIdle(UINT /*uParam*/, LONG /*lParam*/)
{
	BOOL bKey[0x100];
	int i;
	UINT nOld;

	// Memorizar tecla anterior
	nOld = m_nKey;

	// Recibir teclas via DirectInput
	m_pInput->GetKeyBuf(bKey);

	// ・ｽL・ｽ[Busqueda
	for (i=0; i<0x100; i++) {
		// Si hay una tecla nueva respecto a la anterior, configurarla
		if (!m_bKey[i] && bKey[i]) {
			m_nKey = (UINT)i;
		}

		// Copiar
		m_bKey[i] = bKey[i];
	}

	// SHIFT・ｽL・ｽ[・ｽ・ｽOProcesamiento
	if (m_nKey == DIK_RSHIFT) {
		m_nKey = DIK_LSHIFT;
	}

	// Si coinciden, no es necesario cambiar
	if (m_nKey == nOld) {
		return 0;
	}

	// ・ｽL・ｽ[Nombre・ｽ・ｽMostrar
	m_KeyString = m_pInput->GetKeyID(m_nKey);
	Invalidate(FALSE);

	return 0;
}

//---------------------------------------------------------------------------
//
//	Boton derecho presionado
//
//---------------------------------------------------------------------------
void CKeyinDlg::OnRButtonDown(UINT /*nFlags*/, CPoint /*point*/)
{
	// ・ｽ_・ｽC・ｽA・ｽ・ｽ・ｽOFin
	EndDialog(IDOK);
}

//===========================================================================
//
//	Pagina del raton
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CMousePage::CMousePage()
{
	// Configurar siempre ID y Help
	m_dwID = MAKEID('M', 'O', 'U', 'S');
	m_nTemplate = IDD_MOUSEPAGE;
	m_uHelpID = IDC_MOUSE_HELP;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CMousePage, CConfigPage)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_MOUSE_NPORT, OnPort)
	ON_BN_CLICKED(IDC_MOUSE_FPORT, OnPort)
	ON_BN_CLICKED(IDC_MOUSE_KPORT, OnPort)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CMousePage::OnInitDialog()
{
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	CButton *pButton;
	CString strText;
	UINT nID;

	// Clase base
	CConfigPage::OnInitDialog();

	// Velocidad
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_MOUSE_SLIDER);
	ASSERT(pSlider);
	pSlider->SetRange(0, 512);
	pSlider->SetPos(m_pConfig->mouse_speed);

	// Velocidad・ｽe・ｽL・ｽX・ｽg
	strText.Format(_T("%d%%"), (m_pConfig->mouse_speed * 100) >> 8);
	pStatic = (CStatic*)GetDlgItem(IDC_MOUSE_PARS);
	pStatic->SetWindowText(strText);

	// Conexion・ｽ・ｽ|・ｽ[・ｽg
	nID = IDC_MOUSE_NPORT;
	switch (m_pConfig->mouse_port) {
		// Conexion・ｽ・ｽ・ｽﾈゑｿｽ
		case 0:
			break;
		// SCC
		case 1:
			nID = IDC_MOUSE_FPORT;
			break;
		// Teclado
		case 2:
			nID = IDC_MOUSE_KPORT;
			break;
		// Otros(・ｽ・ｽ・ｽ閧ｦ・ｽﾈゑｿｽ)
		default:
			ASSERT(FALSE);
			break;
	}
	pButton = (CButton*)GetDlgItem(nID);
	ASSERT(pButton);
	pButton->SetCheck(1);

	// Opciones
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_SWAPB);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->mouse_swap);
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_MIDB);
	ASSERT(pButton);
	pButton->SetCheck(!m_pConfig->mouse_mid);
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_TBG);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->mouse_trackb);

	// Controles activados/desactivados
	m_bEnableCtrl = TRUE;
	if (m_pConfig->mouse_port == 0) {
		EnableControls(FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CMousePage::OnOK()
{
	CSliderCtrl *pSlider;
	CButton *pButton;

	// Velocidad
	pSlider = (CSliderCtrl*)GetDlgItem(IDC_MOUSE_SLIDER);
	ASSERT(pSlider);
	m_pConfig->mouse_speed = pSlider->GetPos();

	// Conexion・ｽ|・ｽ[・ｽg
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_NPORT);
	ASSERT(pButton);
	if (pButton->GetCheck()) {
		m_pConfig->mouse_port = 0;
	}
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_FPORT);
	ASSERT(pButton);
	if (pButton->GetCheck()) {
		m_pConfig->mouse_port = 1;
	}
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_KPORT);
	ASSERT(pButton);
	if (pButton->GetCheck()) {
		m_pConfig->mouse_port = 2;
	}

	// Opciones
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_SWAPB);
	ASSERT(pButton);
	m_pConfig->mouse_swap = pButton->GetCheck();
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_MIDB);
	ASSERT(pButton);
	m_pConfig->mouse_mid = !(pButton->GetCheck());
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_TBG);
	ASSERT(pButton);
	m_pConfig->mouse_trackb = pButton->GetCheck();

	// Clase base
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Desplazamiento horizontal
//
//---------------------------------------------------------------------------
void CMousePage::OnHScroll(UINT /*nSBCode*/, UINT /*nPos*/, CScrollBar *pBar)
{
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	CString strText;

	// Conversion・ｽAVerificacion
	pSlider = (CSliderCtrl*)pBar;
	if (pSlider->GetDlgCtrlID() != IDC_MOUSE_SLIDER) {
		return;
	}

	// Mostrar
	strText.Format(_T("%d%%"), (pSlider->GetPos() * 100) >> 8);
	pStatic = (CStatic*)GetDlgItem(IDC_MOUSE_PARS);
	pStatic->SetWindowText(strText);
}

//---------------------------------------------------------------------------
//
//	Seleccion de puerto
//
//---------------------------------------------------------------------------
void CMousePage::OnPort()
{
	CButton *pButton;

	// Obtener boton
	pButton = (CButton*)GetDlgItem(IDC_MOUSE_NPORT);
	ASSERT(pButton);

	// Conexion・ｽ・ｽ・ｽﾈゑｿｽ or ・ｽ・ｽ・ｽﾌポ・ｽ[・ｽg・ｽﾅ費ｿｽ・ｽ・ｽ
	if (pButton->GetCheck()) {
		EnableControls(FALSE);
	}
	else {
		EnableControls(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CMousePage::EnableControls(BOOL bEnable) 
{
	int i;
	CWnd *pWnd;

	ASSERT(this);

	// Verificacion de flags
	if (m_bEnableCtrl == bEnable) {
		return;
	}
	m_bEnableCtrl = bEnable;

	// Configurar todos los controles excepto ID de placa y Ayuda
	for(i=0; ; i++) {
		// FinVerificacion
		if (ControlTable[i] == NULL) {
			break;
		}

		// Obtener control
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);
		pWnd->EnableWindow(bEnable);
	}
}

//---------------------------------------------------------------------------
//
//	Tabla de IDs de controles
//
//---------------------------------------------------------------------------
const UINT CMousePage::ControlTable[] = {
	IDC_MOUSE_SPEEDG,
	IDC_MOUSE_SLIDER,
	IDC_MOUSE_PARS,
	IDC_MOUSE_OPTG,
	IDC_MOUSE_SWAPB,
	IDC_MOUSE_MIDB,
	IDC_MOUSE_TBG,
	NULL
};

//===========================================================================
//
//	Pagina del joystick
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CJoyPage::CJoyPage()
{
	CFrmWnd *pFrmWnd;

	// Configurar siempre ID y Help
	m_dwID = MAKEID('J', 'O', 'Y', ' ');
	m_nTemplate = IDD_JOYPAGE;
	m_uHelpID = IDC_JOY_HELP;

	// Obtener entrada
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pInput = pFrmWnd->GetInput();
	ASSERT(m_pInput);
}

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CJoyPage::OnInitDialog()
{
	int i;
	int nDevice;
	CString strNoA;
	CString strDesc;
	DIDEVCAPS ddc;
	CComboBox *pComboBox;

	ASSERT(this);
	ASSERT(m_pInput);

	// Clase base
	CConfigPage::OnInitDialog();

	// Obtener cadena "No Assign"
	::GetMsg(IDS_JOY_NOASSIGN, strNoA);

	// Cuadro combinado de puertos
	for (i=0; i<2; i++) {
		// Cuadro combinado・ｽ謫ｾ
		if (i == 0) {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_PORTC1);
		}
		else {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_PORTC2);
		}
		ASSERT(pComboBox);

		// Limpiar
		pComboBox->ResetContent();

		// Configurar cadenas secuencialmente
		pComboBox->AddString(strNoA);
		::GetMsg(IDS_JOY_ATARI, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_ATARISS, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_CYBERA, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_CYBERD, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_MD3, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_MD6, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_CPSF, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_CPSFMD, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_MAGICAL, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_LR, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_PACLAND, strDesc);
		pComboBox->AddString(strDesc);
		::GetMsg(IDS_JOY_BM, strDesc);
		pComboBox->AddString(strDesc);

		// Cursor
		pComboBox->SetCurSel(m_pConfig->joy_type[i]);

		// ・ｽﾎ会ｿｽBotones・ｽ・ｽInicializacion
		OnSelChg(pComboBox);
	}

	// Cuadro combinado de dispositivos
	for (i=0; i<2; i++) {
		// Cuadro combinado・ｽ謫ｾ
		if (i == 0) {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_DEVCA);
		}
		else {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_DEVCB);
		}
		ASSERT(pComboBox);

		// Limpiar
		pComboBox->ResetContent();

		// Configuracion "No Assign"
		pComboBox->AddString(strNoA);

		// ・ｽf・ｽo・ｽC・ｽXBucle
		for (nDevice=0; ; nDevice++) {
			if (!m_pInput->GetJoyCaps(nDevice, strDesc, &ddc)) {
				// No hay mas dispositivos
				break;
			}

			// Agregar
			pComboBox->AddString(strDesc);
		}

		// ・ｽﾝ定項・ｽﾚゑｿｽCursor
		if (m_pConfig->joy_dev[i] < pComboBox->GetCount()) {
			pComboBox->SetCurSel(m_pConfig->joy_dev[i]);
		}
		else {
			// El valor excede el numero de dispositivos -> foco en "No Assign"
			pComboBox->SetCurSel(0);
		}

		// ・ｽﾎ会ｿｽBotones・ｽ・ｽInicializacion
		OnSelChg(pComboBox);
	}

	// Teclado・ｽf・ｽo・ｽC・ｽX
	pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_DEVCC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	pComboBox->AddString(strNoA);
	pComboBox->SetCurSel(0);
	OnSelChg(pComboBox);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CJoyPage::OnOK()
{
	int i;
	int nButton;
	CComboBox *pComboBox;
	CInput::JOYCFG cfg;

	ASSERT(this);

	// Cuadro combinado de puertos
	for (i=0; i<2; i++) {
		// Cuadro combinado・ｽ謫ｾ
		if (i == 0) {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_PORTC1);
		}
		else {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_PORTC2);
		}

		// Obtener valor de configuracion
		m_pConfig->joy_type[i] = pComboBox->GetCurSel();
		m_pInput->joyType[i] = m_pConfig->joy_type[i];
	}

	// Cuadro combinado de dispositivos
	for (i=0; i<2; i++) {
		// Cuadro combinado・ｽ謫ｾ
		if (i == 0) {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_DEVCA);
		}
		else {
			pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_DEVCB);
		}
		ASSERT(pComboBox);

		// Obtener valor de configuracion
		m_pConfig->joy_dev[i] = pComboBox->GetCurSel();
	}

	// Crear m_pConfig para ejes y botones basado en la configuracion actual
	for (i=0; i<CInput::JoyDevices; i++) {
		// Leer la configuracion de operacion actual
		m_pInput->GetJoyCfg(i, &cfg);

		// Botones
		for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
			// Asignacion・ｽ・ｽDisparo rapido・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
			if (i == 0) {
				// Puerto 1
				m_pConfig->joy_button0[nButton] = 
						cfg.dwButton[nButton] | (cfg.dwRapid[nButton] << 8);
			}
			else {
				// Puerto 2
				m_pConfig->joy_button1[nButton] = 
						cfg.dwButton[nButton] | (cfg.dwRapid[nButton] << 8);
			}
		}
	}

	// Clase base
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CJoyPage::OnCancel()
{
	// CInput・ｽﾉ対ゑｿｽ・ｽﾄ独趣ｿｽ・ｽ・ｽApplyCfg(・ｽﾝ抵ｿｽ・ｽEditar・ｽO・ｽﾉ戻ゑｿｽ)
	m_pInput->ApplyCfg(m_pConfig);

	// Clase base
	CConfigPage::OnCancel();
}

//---------------------------------------------------------------------------
//
//	Notificacion de comando
//
//---------------------------------------------------------------------------
BOOL CJoyPage::OnCommand(WPARAM wParam, LPARAM lParam)
{
	CComboBox *pComboBox;
	UINT nID;

	ASSERT(this);

	// ・ｽ・ｽ・ｽM・ｽ・ｽObtener ID
	nID = (UINT)LOWORD(wParam);

	// CBN_SELCHANGE
	if (HIWORD(wParam) == CBN_SELCHANGE) {
		pComboBox = (CComboBox*)GetDlgItem(nID);
		ASSERT(pComboBox);

		// Rutina dedicada
		OnSelChg(pComboBox);
		return TRUE;
	}

	// BN_CLICKED
	if (HIWORD(wParam) == BN_CLICKED) {
		if ((nID == IDC_JOY_PORTD1) || (nID == IDC_JOY_PORTD2)) {
			// Detalles del lado del puerto
			OnDetail(nID);
		}
		else {
			// Configuracion del lado del dispositivo
			OnSetting(nID);
		}
		return TRUE;
	}

	// Clase base
	return CConfigPage::OnCommand(wParam, lParam);
}

//---------------------------------------------------------------------------
//
//	Cambio en el cuadro combinado
//
//---------------------------------------------------------------------------
void FASTCALL CJoyPage::OnSelChg(CComboBox *pComboBox)
{
	CButton *pButton;

	ASSERT(this);
	ASSERT(pComboBox);

	// ・ｽﾎ会ｿｽBotones・ｽ・ｽ・ｽ謫ｾ
	pButton = GetCorButton(pComboBox->GetDlgCtrlID());
	if (!pButton) {
		return;
	}

	// Cuadro combinado・ｽﾌ選・ｽ・ｽ・ｽｵにゑｿｽ・ｽ・ｽﾄ鯉ｿｽ・ｽﾟゑｿｽ
	if (pComboBox->GetCurSel() == 0) {
		// (Asignacion・ｽﾈゑｿｽ)・ｽ・ｽBotones・ｽ・ｽ・ｽ・ｽ
		pButton->EnableWindow(FALSE);
	}
	else {
		// Botones・ｽL・ｽ・ｽ
		pButton->EnableWindow(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	・ｽ|・ｽ[・ｽg・ｽﾚ搾ｿｽ
//
//---------------------------------------------------------------------------
void FASTCALL CJoyPage::OnDetail(UINT nButton)
{
	CComboBox *pComboBox;
	int nPort;
	int nType;
	CString strDesc;
	CJoyDetDlg dlg(this);

	ASSERT(this);
	ASSERT(nButton != 0);

	// ・ｽ|・ｽ[・ｽg・ｽ謫ｾ
	nPort = 0;
	if (nButton == IDC_JOY_PORTD2) {
		nPort++;
	}

	// Obtener el cuadro combinado correspondiente
	pComboBox = GetCorCombo(nButton);
	if (!pComboBox) {
		return;
	}

	// ・ｽI・ｽ・ｽﾔ搾ｿｽ・ｽｾゑｿｽ
	nType = pComboBox->GetCurSel();
	if (nType == 0) {
		return;
	}

	// ・ｽI・ｽ・ｽﾔ搾ｿｽ・ｽ・ｽ・ｽ・ｽAObtener nombre
	pComboBox->GetLBText(nType, strDesc);

	// ・ｽp・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽ^・ｽ・ｽn・ｽ・ｽ・ｽAEjecutar dialogo
	dlg.m_strDesc = strDesc;
	dlg.m_nPort = nPort;
	dlg.m_nType = nType;
	dlg.DoModal();
}

//---------------------------------------------------------------------------
//
//	・ｽf・ｽo・ｽC・ｽX・ｽﾝ抵ｿｽ
//
//---------------------------------------------------------------------------
void FASTCALL CJoyPage::OnSetting(UINT nButton)
{
	CComboBox *pComboBox;
	CJoySheet sheet(this);
	CInput::JOYCFG cfg;
	int nJoy;
	int nCombo;
	int nType[PPI::PortMax];

	ASSERT(this);
	ASSERT(nButton != 0);

	// Obtener el cuadro combinado correspondiente
	pComboBox = GetCorCombo(nButton);
	if (!pComboBox) {
		return;
	}

	// ・ｽf・ｽo・ｽC・ｽX・ｽC・ｽ・ｽ・ｽf・ｽb・ｽN・ｽX・ｽ謫ｾ
	nJoy = -1;
	switch (pComboBox->GetDlgCtrlID()) {
		// ・ｽf・ｽo・ｽC・ｽXA
		case IDC_JOY_DEVCA:
			nJoy = 0;
			break;

		// ・ｽf・ｽo・ｽC・ｽXB
		case IDC_JOY_DEVCB:
			nJoy = 1;
			break;

		// Otros(・ｽQ・ｽ[・ｽ・ｽ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾅはなゑｿｽ・ｽf・ｽo・ｽC・ｽX)
		default:
			return;
	}
	ASSERT((nJoy == 0) || (nJoy == 1));
	ASSERT(nJoy < CInput::JoyDevices);

	// Cuadro combinado・ｽﾌ選・ｽ・ｽﾔ搾ｿｽ・ｽｾゑｿｽ
	nCombo = pComboBox->GetCurSel();
	if (nCombo == 0) {
		// Asignacion・ｽ・ｽ・ｽ・ｽ
		return;
	}

	// Cuadro combinado・ｽﾌ選・ｽ・ｽﾔ搾ｿｽ・ｽｾゑｿｽB0(Asignacion・ｽ・ｽ・ｽ・ｽ)・ｽ・ｽ・ｽ・ｽ・ｽe
	pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_PORTC1);
	ASSERT(pComboBox);
	nType[0] = pComboBox->GetCurSel();
	pComboBox = (CComboBox*)GetDlgItem(IDC_JOY_PORTC2);
	ASSERT(pComboBox);
	nType[1] = pComboBox->GetCurSel();

	// ・ｽ・ｽ・ｽﾝのジ・ｽ・ｽ・ｽC・ｽX・ｽe・ｽB・ｽb・ｽN・ｽﾝ抵ｿｽ・ｽﾛ托ｿｽ
	m_pInput->GetJoyCfg(nJoy, &cfg);

	// ・ｽp・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽ^・ｽﾝ抵ｿｽ
	sheet.SetParam(nJoy, nCombo, nType);

	// Ejecutar dialogo(・ｽW・ｽ・ｽ・ｽC・ｽX・ｽe・ｽB・ｽb・ｽN・ｽﾘゑｿｽﾖゑｿｽ・ｽ・ｽ・ｽ・ｽ・ｽﾝ、Cancelar・ｽﾈゑｿｽﾝ抵ｿｽﾟゑｿｽ)
	m_pInput->EnableJoy(FALSE);
	if (sheet.DoModal() != IDOK) {
		m_pInput->SetJoyCfg(nJoy, &cfg);
	}
	m_pInput->EnableJoy(TRUE);
}

//---------------------------------------------------------------------------
//
//	・ｽﾎ会ｿｽ・ｽ・ｽ・ｽ・ｽBotones・ｽ・ｽ・ｽ謫ｾ
//
//---------------------------------------------------------------------------
CButton* CJoyPage::GetCorButton(UINT nComboBox)
{
	int i;
	CButton *pButton;

	ASSERT(this);
	ASSERT(nComboBox != 0);

	pButton = NULL;

	// Tabla de controles・ｽ・ｽBusqueda
	for (i=0; ; i+=2) {
		// ・ｽI・ｽ[Verificacion
		if (ControlTable[i] == NULL) {
			return NULL;
		}

		// Si coincide, OK
		if (ControlTable[i] == nComboBox) {
			// ・ｽﾎ会ｿｽ・ｽ・ｽ・ｽ・ｽBotones・ｽｾゑｿｽ
			pButton = (CButton*)GetDlgItem(ControlTable[i + 1]);
			break;
		}
	}

	ASSERT(pButton);
	return pButton;
}

//---------------------------------------------------------------------------
//
//	Obtener el cuadro combinado correspondiente
//
//---------------------------------------------------------------------------
CComboBox* CJoyPage::GetCorCombo(UINT nButton)
{
	int i;
	CComboBox *pComboBox;

	ASSERT(this);
	ASSERT(nButton != 0);

	pComboBox = NULL;

	// Tabla de controles・ｽ・ｽBusqueda
	for (i=1; ; i+=2) {
		// ・ｽI・ｽ[Verificacion
		if (ControlTable[i] == NULL) {
			return NULL;
		}

		// Si coincide, OK
		if (ControlTable[i] == nButton) {
			// Obtener el cuadro combinado correspondiente
			pComboBox = (CComboBox*)GetDlgItem(ControlTable[i - 1]);
			break;
		}
	}

	ASSERT(pComboBox);
	return pComboBox;
}

//---------------------------------------------------------------------------
//
//	Tabla de controles
//	・ｽ・ｽCuadro combinado・ｽ・ｽBotones・ｽﾆの托ｿｽ・ｽﾝ対会ｿｽ・ｽ・ｽ・ｽﾆるた・ｽ・ｽ
//
//---------------------------------------------------------------------------
UINT CJoyPage::ControlTable[] = {
	IDC_JOY_PORTC1, IDC_JOY_PORTD1,
	IDC_JOY_PORTC2, IDC_JOY_PORTD2,
	IDC_JOY_DEVCA, IDC_JOY_DEVAA,
	IDC_JOY_DEVCB, IDC_JOY_DEVAB,
	IDC_JOY_DEVCC, IDC_JOY_DEVAC,
	NULL, NULL
};

//===========================================================================
//
//	Dialogo de detalles del joystick
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CJoyDetDlg::CJoyDetDlg(CWnd *pParent) : CDialog(IDD_JOYDETDLG, pParent)
{
	// Ingles・ｽﾂ具ｿｽ・ｽﾖの対会ｿｽ
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_JOYDETDLG);
		m_nIDHelp = IDD_US_JOYDETDLG;
	}

	m_strDesc.Empty();
	m_nPort = -1;
	m_nType = 0;
}

//---------------------------------------------------------------------------
//
//	・ｽ_・ｽC・ｽA・ｽ・ｽ・ｽOInicializacion
//
//---------------------------------------------------------------------------
BOOL CJoyDetDlg::OnInitDialog()
{
	CString strBase;
	CString strText;
	CStatic *pStatic;
	PPI *pPPI;
	JoyDevice *pDevice;

	// Clase base
	CDialog::OnInitDialog();

	ASSERT(m_strDesc.GetLength() > 0);
	ASSERT((m_nPort >= 0) && (m_nPort < PPI::PortMax));
	ASSERT(m_nType >= 1);

	// Nombre del puerto
	GetWindowText(strBase);
	strText.Format(strBase, m_nPort + 1);
	SetWindowText(strText);

	// Nombre
	pStatic = (CStatic*)GetDlgItem(IDC_JOYDET_NAMEL);
	ASSERT(pStatic);
	pStatic->SetWindowText(m_strDesc);

	// Crear joystick
	pPPI = (PPI*)::GetVM()->SearchDevice(MAKEID('P', 'P', 'I', ' '));
	ASSERT(pPPI);
	pDevice = pPPI->CreateJoy(m_nPort, m_nType);
	ASSERT(pDevice);

	// Numero de ejes
	pStatic = (CStatic*)GetDlgItem(IDC_JOYDET_AXISS);
	ASSERT(pStatic);
	pStatic->GetWindowText(strBase);
	strText.Format(strBase, pDevice->GetAxes());
	pStatic->SetWindowText(strText);

	// Botones・ｽ・ｽ
	pStatic = (CStatic*)GetDlgItem(IDC_JOYDET_BUTTONS);
	ASSERT(pStatic);
	pStatic->GetWindowText(strBase);
	strText.Format(strBase, pDevice->GetButtons());
	pStatic->SetWindowText(strText);

	// Tipo (Analogico/Digital)
	if (pDevice->IsAnalog()) {
		pStatic = (CStatic*)GetDlgItem(IDC_JOYDET_TYPES);
		::GetMsg(IDS_JOYDET_ANALOG, strText);
		pStatic->SetWindowText(strText);
	}

	// Numero de datos
	pStatic = (CStatic*)GetDlgItem(IDC_JOYDET_DATASS);
	ASSERT(pStatic);
	pStatic->GetWindowText(strBase);
	strText.Format(strBase, pDevice->GetDatas());
	pStatic->SetWindowText(strText);

	// ・ｽW・ｽ・ｽ・ｽC・ｽX・ｽe・ｽB・ｽb・ｽNEliminar
	delete pDevice;

	return TRUE;
}

//===========================================================================
//
//	Botones・ｽﾝ抵ｿｽy・ｽ[・ｽW
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CBtnSetPage::CBtnSetPage()
{
	CFrmWnd *pFrmWnd;
	int i;

#if defined(_DEBUG)
	ASSERT(CInput::JoyButtons <= (sizeof(m_bButton)/sizeof(BOOL)));
	ASSERT(CInput::JoyButtons <= (sizeof(m_rectLabel)/sizeof(CRect)));
#endif	// _DEBUG

	// Obtener entrada
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pInput = pFrmWnd->GetInput();
	ASSERT(m_pInput);

	// ・ｽW・ｽ・ｽ・ｽC・ｽX・ｽe・ｽB・ｽb・ｽN・ｽﾔ搾ｿｽ・ｽ・ｽLimpiar
	m_nJoy = -1;

	// ・ｽ^・ｽC・ｽv・ｽﾔ搾ｿｽ・ｽ・ｽLimpiar
	for (i=0; i<PPI::PortMax; i++) {
		m_nType[i] = -1;
	}
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CBtnSetPage, CPropertyPage)
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_HSCROLL()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Crear
//
//---------------------------------------------------------------------------
void FASTCALL CBtnSetPage::Init(CPropertySheet *pSheet)
{
	int nID;

	ASSERT(this);

	// Memorizar hoja padre
	ASSERT(pSheet);
	m_pSheet = pSheet;

	// IDAceptar
	nID = IDD_BTNSETPAGE;
	if (!::IsJapanese()) {
		nID += 50;
	}

	// Construccion
	CommonConstruct(MAKEINTRESOURCE(nID), 0);

	// ・ｽe・ｽV・ｽ[・ｽg・ｽ・ｽAgregar
	pSheet->AddPage(this);
}

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CBtnSetPage::OnInitDialog()
{
	CJoySheet *pJoySheet;
	int nButton;
	int nButtons;
	int nPort;
	int nCandidate;
	CStatic *pStatic;
	CComboBox *pComboBox;
	CSliderCtrl *pSlider;
	CString strText;
	CString strBase;
	CInput::JOYCFG cfg;
	DWORD dwData;
	PPI *pPPI;
	JoyDevice *pJoyDevice;
	CString strDesc;

	ASSERT(this);
	ASSERT(m_pSheet);

	// Clase base
	CPropertyPage::OnInitDialog();

	// ・ｽe・ｽN・ｽ・ｽ・ｽX・ｽ・ｽInicializacion(CPropertySheet・ｽ・ｽOnInitDialog・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾈゑｿｽ)
	pJoySheet = (CJoySheet*)m_pSheet;
	pJoySheet->InitSheet();
	ASSERT((m_nJoy >= 0) && (m_nJoy < CInput::JoyDevices));

	// Obtener la configuracion de joystick actual
	m_pInput->GetJoyCfg(m_nJoy, &cfg);

	// Obtener PPI
	pPPI = (PPI*)::GetVM()->SearchDevice(MAKEID('P', 'P', 'I', ' '));
	ASSERT(pPPI);

	// Botones・ｽ・ｽ・ｽ謫ｾ
	nButtons = pJoySheet->GetButtons();

	// Obtener texto base
	::GetMsg(IDS_JOYSET_BTNPORT, strBase);

	// Configuracion de controles
	for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
		// Etiqueta
		pStatic = (CStatic*)GetDlgItem(GetControl(nButton, BtnLabel));
		ASSERT(pStatic);
		if (nButton < nButtons) {
			// ・ｽL・ｽ・ｽ(・ｽE・ｽB・ｽ・ｽ・ｽh・ｽEEliminar)
			pStatic->GetWindowRect(&m_rectLabel[nButton]);
			ScreenToClient(&m_rectLabel[nButton]);
			pStatic->DestroyWindow();
		}
		else {
			// Inactivo (prohibir ventana)
			m_rectLabel[nButton].top = 0;
			m_rectLabel[nButton].left = 0;
			pStatic->EnableWindow(FALSE);
		}

		// Cuadro combinado
		pComboBox = (CComboBox*)GetDlgItem(GetControl(nButton, BtnCombo));
		ASSERT(pComboBox);
		if (nButton < nButtons) {
			// ・ｽL・ｽ・ｽ(・ｽ・ｽ・ｽ・ｽAgregar)
			pComboBox->ResetContent();

			// Configurar "No Assign"
			::GetMsg(IDS_JOYSET_NOASSIGN, strText);
			pComboBox->AddString(strText);

			// ・ｽ|・ｽ[・ｽg・ｽABotones・ｽ・ｽ・ｽ・ｽ・ｽ
			for (nPort=0; nPort<PPI::PortMax; nPort++) {
				// Obtener dispositivo de joystick temporal
				pJoyDevice = pPPI->CreateJoy(0, m_nType[nPort]);

				for (nCandidate=0; nCandidate<PPI::ButtonMax; nCandidate++) {
					// ・ｽW・ｽ・ｽ・ｽC・ｽX・ｽe・ｽB・ｽb・ｽN・ｽf・ｽo・ｽC・ｽX・ｽ・ｽ・ｽ・ｽBotonesObtener nombre
					GetButtonDesc(pJoyDevice->GetButtonDesc(nCandidate), strDesc);

					// Formato
					strText.Format(strBase, nPort + 1, nCandidate + 1, strDesc);
					pComboBox->AddString(strText);
				}

				// ・ｽ・ｽ・ｽW・ｽ・ｽ・ｽC・ｽX・ｽe・ｽB・ｽb・ｽN・ｽf・ｽo・ｽC・ｽX・ｽ・ｽEliminar
				delete pJoyDevice;
			}

			// Cursor・ｽﾝ抵ｿｽ
			pComboBox->SetCurSel(0);
			if ((LOWORD(cfg.dwButton[nButton]) != 0) && (LOWORD(cfg.dwButton[nButton]) <= PPI::ButtonMax)) {
				if (cfg.dwButton[nButton] & 0x10000) {
					// Puerto 2
					pComboBox->SetCurSel(LOWORD(cfg.dwButton[nButton]) + PPI::ButtonMax);
				}
				else {
					// Puerto 1
					pComboBox->SetCurSel(LOWORD(cfg.dwButton[nButton]));
				}
			}
		}
		else {
			// Inactivo (prohibir ventana)
			pComboBox->EnableWindow(FALSE);
		}

		// Disparo rapido・ｽX・ｽ・ｽ・ｽC・ｽ_
		pSlider = (CSliderCtrl*)GetDlgItem(GetControl(nButton, BtnRapid));
		ASSERT(pSlider);
		if (nButton < nButtons) {
			// Activo (configurar rango y valor actual)
			pSlider->SetRange(0, CInput::JoyRapids);
			if (cfg.dwRapid[nButton] <= CInput::JoyRapids) {
				pSlider->SetPos(cfg.dwRapid[nButton]);
			}
		}
		else {
			// Inactivo (prohibir ventana)
			pSlider->EnableWindow(FALSE);
		}

		// Disparo rapido・ｽl
		pStatic = (CStatic*)GetDlgItem(GetControl(nButton, BtnValue));
		ASSERT(pStatic);
		if (nButton < nButtons) {
			// ・ｽL・ｽ・ｽ(・ｽ・ｽ・ｽ・ｽ・ｽlMostrar)
			OnSlider(nButton);
			OnSelChg(nButton);
		}
		else {
			// ・ｽ・ｽ・ｽ・ｽ(Limpiar)
			strText.Empty();
			pStatic->SetWindowText(strText);
		}
	}

	// Botones・ｽ・ｽ・ｽ・ｽ・ｽl・ｽﾇみ趣ｿｽ・ｽ
	for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
		m_bButton[nButton] = FALSE;
		dwData = m_pInput->GetJoyButton(m_nJoy, nButton);
		if ((dwData < 0x10000) && (dwData & 0x80)) {
			m_bButton[nButton] = TRUE;
		}
	}

	// Iniciar temporizador (fuego cada 100ms)
	m_nTimerID = SetTimer(IDD_BTNSETPAGE, 100, NULL);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Dibujar
//
//---------------------------------------------------------------------------
void CBtnSetPage::OnPaint()
{
	CPaintDC dc(this);

	// Dibujar・ｽ・ｽ・ｽC・ｽ・ｽ
	OnDraw(&dc, NULL, TRUE);
}

//---------------------------------------------------------------------------
//
//	Desplazamiento horizontal
//
//---------------------------------------------------------------------------
void CBtnSetPage::OnHScroll(UINT /*nSBCode*/, UINT /*nPos*/, CScrollBar *pBar)
{
	CSliderCtrl *pSlider;
	UINT nID;
	int nButton;

	// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽID・ｽ・ｽ・ｽ謫ｾ
	pSlider = (CSliderCtrl*)pBar;
	nID = pSlider->GetDlgCtrlID();

	// Botones・ｽC・ｽ・ｽ・ｽf・ｽb・ｽN・ｽX・ｽ・ｽBusqueda
	for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
		if (GetControl(nButton, BtnRapid) == nID) {
			// Rutina dedicada・ｽ・ｽ・ｽﾄゑｿｽ
			OnSlider(nButton);
			break;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Notificacion de comando
//
//---------------------------------------------------------------------------
BOOL CBtnSetPage::OnCommand(WPARAM wParam, LPARAM lParam)
{
	int nButton;
	UINT nID;

	ASSERT(this);

	// ・ｽ・ｽ・ｽM・ｽ・ｽObtener ID
	nID = (UINT)LOWORD(wParam);

	// CBN_SELCHANGE
	if (HIWORD(wParam) == CBN_SELCHANGE) {
		// Botones・ｽC・ｽ・ｽ・ｽf・ｽb・ｽN・ｽX・ｽ・ｽBusqueda
		for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
			if (GetControl(nButton, BtnCombo) == nID) {
				OnSelChg(nButton);
				break;
			}
		}
	}

	// Clase base
	return CPropertyPage::OnCommand(wParam, lParam);
}

//---------------------------------------------------------------------------
//
//	Dibujar・ｽ・ｽ・ｽC・ｽ・ｽ
//
//---------------------------------------------------------------------------
void FASTCALL CBtnSetPage::OnDraw(CDC *pDC, BOOL *pButton, BOOL bForce)
{
	int nButton;
	CFont *pFont;
	CString strBase;
	CString strText;

	ASSERT(this);
	ASSERT(pDC);

	// Configuracion de colores
	pDC->SetBkColor(::GetSysColor(COLOR_3DFACE));

	// Configuracion de fuentes
	pFont = (CFont*)pDC->SelectStockObject(DEFAULT_GUI_FONT);
	ASSERT(pFont);

	// Obtener la cadena base
	::GetMsg(IDS_JOYSET_BTNLABEL, strBase);

	// BotonesBucle
	for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
		// ・ｽL・ｽ・ｽ(Mostrar・ｽ・ｽ・ｽﾗゑｿｽ)Botones・ｽ・ｽ・ｽﾛゑｿｽ
		if ((m_rectLabel[nButton].left == 0) && (m_rectLabel[nButton].top == 0)) {
			// Botones・ｽ・ｽ・ｽﾈゑｿｽ・ｽﾌで、・ｽ・ｽ・ｽ・ｽ・ｽﾉゑｿｽ・ｽ黷ｽ・ｽX・ｽ^・ｽe・ｽB・ｽb・ｽN・ｽe・ｽL・ｽX・ｽg
			continue;
		}

		// !bForce・ｽﾈゑｿｽA・ｽ・ｽr・ｽ・ｽ・ｽ・ｽAceptar
		if (!bForce) {
			ASSERT(pButton);
			if (m_bButton[nButton] == pButton[nButton]) {
				// ・ｽ・ｽv・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽﾌゑｿｽDibujar・ｽ・ｽ・ｽﾈゑｿｽ
				continue;
			}
			// Difieren, guardar
			m_bButton[nButton] = pButton[nButton];
		}

		// ・ｽF・ｽ・ｽAceptar
		if (m_bButton[nButton]) {
			// Presionado (Rojo)
			pDC->SetTextColor(RGB(255, 0, 0));
		}
		else {
			// No presionado (Negro)
			pDC->SetTextColor(::GetSysColor(COLOR_BTNTEXT));
		}

		// Mostrar
		strText.Format(strBase, nButton + 1);
		pDC->DrawText(strText, m_rectLabel[nButton],
						DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
	}

	// Restaurar fuente(Objetos・ｽ・ｽEliminar・ｽ・ｽ・ｽﾈゑｿｽ・ｽﾄよい)
	pDC->SelectObject(pFont);
}

//---------------------------------------------------------------------------
//
//	Temporizador
//
//---------------------------------------------------------------------------
void CBtnSetPage::OnTimer(UINT /*nTimerID*/)
{
	int nButton;
	BOOL bButton[CInput::JoyButtons];
	BOOL bFlag;
	DWORD dwData;
	CClientDC *pDC;

	ASSERT(this);

	// ・ｽt・ｽ・ｽ・ｽOInicializacion
	bFlag = FALSE;

	// ・ｽ・ｽ・ｽﾝのジ・ｽ・ｽ・ｽC・ｽX・ｽe・ｽB・ｽb・ｽNBotones・ｽ・ｽ・ｽ・ｽﾇみ趣ｿｽ・ｽA・ｽ・ｽr
	for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
		bButton[nButton] = FALSE;
		dwData = m_pInput->GetJoyButton(m_nJoy, nButton);
		if ((dwData < 0x10000) && (dwData & 0x80)) {
			bButton[nButton] = TRUE;
		}

		// Si es diferente, subir flag
		if (m_bButton[nButton] != bButton[nButton]) {
			bFlag = TRUE;
		}
	}

	// ・ｽt・ｽ・ｽ・ｽO・ｽ・ｽ・ｽ繧ｪ・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽﾎ、・ｽ・ｽDibujar
	if (bFlag) {
		pDC = new CClientDC(this);
		OnDraw(pDC, bButton, FALSE);
		delete pDC;
	}
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CBtnSetPage::OnOK()
{
	CJoySheet *pJoySheet;
	int nButtons;
	int nButton;
	CInput::JOYCFG cfg;
	CComboBox *pComboBox;
	CSliderCtrl *pSlider;
	int nSelect;

	// Temporizador・ｽ・ｽ~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// Obtener hoja padre
	pJoySheet = (CJoySheet*)m_pSheet;
	nButtons = pJoySheet->GetButtons();

	// Obtener los datos de configuracion actuales
	m_pInput->GetJoyCfg(m_nJoy, &cfg);

	// Leer controles y reflejar en la configuracion actual
	for (nButton=0; nButton<CInput::JoyButtons; nButton++) {
		// ・ｽL・ｽ・ｽ・ｽ・ｽBotones・ｽ・ｽ
		if (nButton >= nButtons) {
			// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽBotones・ｽﾈので、Asignacion・ｽEDisparo rapido・ｽﾆゑｿｽ・ｽ・ｽ0
			cfg.dwButton[nButton] = 0;
			cfg.dwRapid[nButton] = 0;
			continue;
		}

		// Leer asignacion
		pComboBox = (CComboBox*)GetDlgItem(GetControl(nButton, BtnCombo));
		ASSERT(pComboBox);
		nSelect = pComboBox->GetCurSel();

		// (Asignacion・ｽﾈゑｿｽ)Verificacion
		if (nSelect == 0) {
			// ・ｽ・ｽ・ｽ・ｽAsignacion・ｽﾈゑｿｽAAsignacion・ｽEDisparo rapido・ｽﾆゑｿｽ・ｽ・ｽ0
			cfg.dwButton[nButton] = 0;
			cfg.dwRapid[nButton] = 0;
			continue;
		}

		// Asignacion normal
		nSelect--;
		if (nSelect >= PPI::ButtonMax) {
			// Puerto 2
			cfg.dwButton[nButton] = (DWORD)(0x10000 | (nSelect - PPI::ButtonMax + 1));
		}
		else {
			// Puerto 1
			cfg.dwButton[nButton] = (DWORD)(nSelect + 1);
		}

		// Disparo rapido
		pSlider = (CSliderCtrl*)GetDlgItem(GetControl(nButton, BtnRapid));
		ASSERT(pSlider);
		cfg.dwRapid[nButton] = pSlider->GetPos();
	}

	// Reflejar datos de configuracion
	m_pInput->SetJoyCfg(m_nJoy, &cfg);

	// Clase base
	CPropertyPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CBtnSetPage::OnCancel()
{
	// Temporizador・ｽ・ｽ~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// Clase base
	CPropertyPage::OnCancel();
}

//---------------------------------------------------------------------------
//
//	・ｽX・ｽ・ｽ・ｽC・ｽ_Modificacion
//
//---------------------------------------------------------------------------
void FASTCALL CBtnSetPage::OnSlider(int nButton)
{
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	CString strText;
	int nPos;

	ASSERT(this);
	ASSERT((nButton >= 0) && (nButton < CInput::JoyButtons));

	// ・ｽ|・ｽW・ｽV・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ謫ｾ
	pSlider = (CSliderCtrl*)GetDlgItem(GetControl(nButton, BtnRapid));
	ASSERT(pSlider);
	nPos = pSlider->GetPos();

	// ・ｽﾎ会ｿｽ・ｽ・ｽ・ｽ・ｽEtiqueta・ｽ・ｽ・ｽ謫ｾ
	pStatic = (CStatic*)GetDlgItem(GetControl(nButton, BtnValue));
	ASSERT(pStatic);

	// Establecer valores desde la tabla
	if ((nPos >= 0) && (nPos <= CInput::JoyRapids)) {
		// ・ｽﾅ定小・ｽ・ｽ・ｽ_Procesamiento
		if (RapidTable[nPos] & 1) {
			strText.Format(_T("%d.5"), RapidTable[nPos] >> 1);
		}
		else {
			strText.Format(_T("%d"), RapidTable[nPos] >> 1);
		}
	}
	else {
		strText.Empty();
	}
	pStatic->SetWindowText(strText);
}

//---------------------------------------------------------------------------
//
//	Cambio en el cuadro combinado
//
//---------------------------------------------------------------------------
void FASTCALL CBtnSetPage::OnSelChg(int nButton)
{
	CComboBox *pComboBox;
	CSliderCtrl *pSlider;
	CStatic *pStatic;
	int nPos;

	ASSERT(this);
	ASSERT((nButton >= 0) && (nButton < CInput::JoyButtons));

	// ・ｽ|・ｽW・ｽV・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ謫ｾ
	pComboBox = (CComboBox*)GetDlgItem(GetControl(nButton, BtnCombo));
	ASSERT(pComboBox);
	nPos = pComboBox->GetCurSel();

	// ・ｽﾎ会ｿｽ・ｽ・ｽ・ｽ・ｽX・ｽ・ｽ・ｽC・ｽ_・ｽAEtiqueta・ｽ・ｽ・ｽ謫ｾ
	pSlider = (CSliderCtrl*)GetDlgItem(GetControl(nButton, BtnRapid));
	ASSERT(pSlider);
	pStatic = (CStatic*)GetDlgItem(GetControl(nButton, BtnValue));
	ASSERT(pStatic);

	// Configurar activacion/desactivacion de la ventana
	if (nPos == 0) {
		pSlider->EnableWindow(FALSE);
		pStatic->EnableWindow(FALSE);
	}
	else {
		pSlider->EnableWindow(TRUE);
		pStatic->EnableWindow(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	BotonesNombre・ｽ謫ｾ
//
//---------------------------------------------------------------------------
void FASTCALL CBtnSetPage::GetButtonDesc(const char *pszDesc, CString& strDesc)
{
	LPCTSTR lpszT;

	ASSERT(this);

	// Inicializacion
	strDesc.Empty();

	// NULL・ｽﾈらリ・ｽ^・ｽ[・ｽ・ｽ
	if (!pszDesc) {
		return;
	}

	// TC・ｽ・ｽConversion
	lpszT = A2CT(pszDesc);

	// Generar cadena entre parentesis
	strDesc = _T("(");
	strDesc += lpszT;
	strDesc += _T(")");
}

//---------------------------------------------------------------------------
//
//	Obtener control
//
//---------------------------------------------------------------------------
UINT FASTCALL CBtnSetPage::GetControl(int nButton, CtrlType ctlType) const
{
	int nType;

	ASSERT(this);
	ASSERT((nButton >= 0) && (nButton < CInput::JoyButtons));

	// Obtener tipo
	nType = (int)ctlType;
	ASSERT((nType >= 0) && (nType < 4));

	// Obtener ID
	return ControlTable[(nButton << 2) + nType];
}

//---------------------------------------------------------------------------
//
//	Tabla de controles
//
//---------------------------------------------------------------------------
const UINT CBtnSetPage::ControlTable[] = {
	IDC_BTNSET_BTNL1, IDC_BTNSET_ASSIGNC1, IDC_BTNSET_RAPIDC1, IDC_BTNSET_RAPIDL1,
	IDC_BTNSET_BTNL2, IDC_BTNSET_ASSIGNC2, IDC_BTNSET_RAPIDC2, IDC_BTNSET_RAPIDL2,
	IDC_BTNSET_BTNL3, IDC_BTNSET_ASSIGNC3, IDC_BTNSET_RAPIDC3, IDC_BTNSET_RAPIDL3,
	IDC_BTNSET_BTNL4, IDC_BTNSET_ASSIGNC4, IDC_BTNSET_RAPIDC4, IDC_BTNSET_RAPIDL4,
	IDC_BTNSET_BTNL5, IDC_BTNSET_ASSIGNC5, IDC_BTNSET_RAPIDC5, IDC_BTNSET_RAPIDL5,
	IDC_BTNSET_BTNL6, IDC_BTNSET_ASSIGNC6, IDC_BTNSET_RAPIDC6, IDC_BTNSET_RAPIDL6,
	IDC_BTNSET_BTNL7, IDC_BTNSET_ASSIGNC7, IDC_BTNSET_RAPIDC7, IDC_BTNSET_RAPIDL7,
	IDC_BTNSET_BTNL8, IDC_BTNSET_ASSIGNC8, IDC_BTNSET_RAPIDC8, IDC_BTNSET_RAPIDL8,
	IDC_BTNSET_BTNL9, IDC_BTNSET_ASSIGNC9, IDC_BTNSET_RAPIDC9, IDC_BTNSET_RAPIDL9,
	IDC_BTNSET_BTNL10,IDC_BTNSET_ASSIGNC10,IDC_BTNSET_RAPIDC10,IDC_BTNSET_RAPIDL10,
	IDC_BTNSET_BTNL11,IDC_BTNSET_ASSIGNC11,IDC_BTNSET_RAPIDC11,IDC_BTNSET_RAPIDL11,
	IDC_BTNSET_BTNL12,IDC_BTNSET_ASSIGNC12,IDC_BTNSET_RAPIDC12,IDC_BTNSET_RAPIDL12
};

//---------------------------------------------------------------------------
//
//	Disparo rapido・ｽe・ｽ[・ｽu・ｽ・ｽ
//	・ｽ・ｽ・ｽﾅ定小・ｽ・ｽ・ｽ_Procesamiento・ｽﾌゑｿｽ・ｽﾟ、2・ｽ{・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽ
//
//---------------------------------------------------------------------------
const int CBtnSetPage::RapidTable[CInput::JoyRapids + 1] = {
	0,
	4,
	6,
	8,
	10,
	15,
	20,
	24,
	30,
	40,
	60
};

//===========================================================================
//
//	Hoja de propiedades del joystick
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CJoySheet::CJoySheet(CWnd *pParent) : CPropertySheet(IDS_JOYSET, pParent)
{
	CFrmWnd *pFrmWnd;
	int i;

	// Ingles・ｽﾂ具ｿｽ・ｽﾖの対会ｿｽ
	if (!::IsJapanese()) {
		::GetMsg(IDS_JOYSET, m_strCaption);
	}

	// ApplyBotones・ｽ・ｽEliminar
	m_psh.dwFlags |= PSH_NOAPPLYNOW;

	// Obtener CInput
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pInput = pFrmWnd->GetInput();
	ASSERT(m_pInput);

	// ・ｽp・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽ^Inicializacion
	m_nJoy = -1;
	m_nCombo = -1;
	for (i=0; i<PPI::PortMax; i++) {
		m_nType[i] = -1;
	}

	// ・ｽy・ｽ[・ｽWInicializacion
	m_BtnSet.Init(this);
}

//---------------------------------------------------------------------------
//
//	・ｽp・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽ^・ｽﾝ抵ｿｽ
//
//---------------------------------------------------------------------------
void FASTCALL CJoySheet::SetParam(int nJoy, int nCombo, int nType[])
{
	int i;

	ASSERT(this);
	ASSERT((nJoy == 0) || (nJoy == 1));
	ASSERT(nJoy < CInput::JoyDevices);
	ASSERT(nCombo >= 1);
	ASSERT(nType);

	// ・ｽL・ｽ・ｽ(Cuadro combinado・ｽ・ｽ-1)
	m_nJoy = nJoy;
	m_nCombo = nCombo - 1;
	for (i=0; i<PPI::PortMax; i++) {
		m_nType[i] = nType[i];
	}

	// CapsLimpiar
	memset(&m_DevCaps, 0, sizeof(m_DevCaps));
}

//---------------------------------------------------------------------------
//
//	・ｽV・ｽ[・ｽgInicializacion
//
//---------------------------------------------------------------------------
void FASTCALL CJoySheet::InitSheet()
{
	int i;
	CString strDesc;
	CString strFmt;
	CString strText;

	ASSERT(this);
	ASSERT(m_pInput);
	ASSERT((m_nJoy == 0) || (m_nJoy == 1));
	ASSERT(m_nJoy < CInput::JoyDevices);
	ASSERT(m_nCombo >= 0);

	// Obtener Caps del dispositivo
	m_pInput->GetJoyCaps(m_nCombo, strDesc, &m_DevCaps);

	// ・ｽE・ｽB・ｽ・ｽ・ｽh・ｽE・ｽe・ｽL・ｽX・ｽgEditar
	GetWindowText(strFmt);
	strText.Format(strFmt, _T('A' + m_nJoy), strDesc);
	SetWindowText(strText);

	// Distribuir parametros a cada pagina
	m_BtnSet.m_nJoy = m_nJoy;
	for (i=0; i<PPI::PortMax; i++) {
		m_BtnSet.m_nType[i] = m_nType[i];
	}
}

//---------------------------------------------------------------------------
//
//	Numero de ejes・ｽ謫ｾ
//
//---------------------------------------------------------------------------
int FASTCALL CJoySheet::GetAxes() const
{
	ASSERT(this);

	return (int)m_DevCaps.dwAxes;
}

//---------------------------------------------------------------------------
//
//	Botones・ｽ・ｽ・ｽ謫ｾ
//
//---------------------------------------------------------------------------
int FASTCALL CJoySheet::GetButtons() const
{
	ASSERT(this);

	return (int)m_DevCaps.dwButtons;
}

//===========================================================================
//
//	Pagina SASI
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSASIPage::CSASIPage()
{
	int i;

	// Configurar siempre ID y Help
	m_dwID = MAKEID('S', 'A', 'S', 'I');
	m_nTemplate = IDD_SASIPAGE;
	m_uHelpID = IDC_SASI_HELP;

	// SASIObtener dispositivo
	m_pSASI = (SASI*)::GetVM()->SearchDevice(MAKEID('S', 'A', 'S', 'I'));
	ASSERT(m_pSASI);

	// ・ｽ・ｽInicializacion
	m_bInit = FALSE;
	m_nDrives = -1;

	ASSERT(SASI::SASIMax <= 16);
	for (i=0; i<SASI::SASIMax; i++) {
		m_szFile[i][0] = _T('\0');
	}
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSASIPage, CConfigPage)
	ON_WM_VSCROLL()
	ON_NOTIFY(NM_CLICK, IDC_SASI_LIST, OnClick)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CSASIPage::OnInitDialog()
{
	CSpinButtonCtrl *pSpin;
	CButton *pButton;
	CListCtrl *pListCtrl;
	CClientDC *pDC;
	CString strCaption;
	CString strFile;
	TEXTMETRIC tm;
	LONG cx;
	int i;

	// Clase base
	CConfigPage::OnInitDialog();

	// Inicializacion・ｽt・ｽ・ｽ・ｽOUp・ｽANumero de unidades・ｽ謫ｾ
	m_bInit = TRUE;
	m_nDrives = m_pConfig->sasi_drives;
	ASSERT((m_nDrives >= 0) && (m_nDrives <= SASI::SASIMax));

	// Cargar cadenas de texto
	::GetMsg(IDS_SASI_DEVERROR, m_strError);

	// Numero de unidades
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SASI_DRIVES);
	ASSERT(pSpin);
	pSpin->SetBase(10);
	pSpin->SetRange(0, SASI::SASIMax);
	pSpin->SetPos(m_nDrives);

	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽX・ｽC・ｽb・ｽ`・ｽ・ｽ・ｽ・ｽActualizacion
	pButton = (CButton*)GetDlgItem(IDC_SASI_MEMSWB);
	ASSERT(pButton);
	if (m_pConfig->sasi_sramsync) {
		pButton->SetCheck(1);
	}
	else {
		pButton->SetCheck(0);
	}

	// Obtener nombre de archivo
	for (i=0; i<SASI::SASIMax; i++) {
		_tcscpy(m_szFile[i], m_pConfig->sasi_file[i]);
	}

	// Obtener metricas de texto
	pDC = new CClientDC(this);
	::GetTextMetrics(pDC->m_hDC, &tm);
	delete pDC;
	cx = tm.tmAveCharWidth;

	// ・ｽ・ｽ・ｽX・ｽgConfiguracion de controles
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SASI_LIST);
	ASSERT(pListCtrl);
	pListCtrl->DeleteAllItems();
	::GetMsg(IDS_SASI_CAPACITY, strCaption);
	::GetMsg(IDS_SASI_FILENAME, strFile);
	if (::IsJapanese()) {
		pListCtrl->InsertColumn(0, _T("No."), LVCFMT_LEFT, cx * 4, 0);
	}
	else {
		pListCtrl->InsertColumn(0, _T("No."), LVCFMT_LEFT, cx * 5, 0);
	}
	pListCtrl->InsertColumn(1, strCaption, LVCFMT_CENTER,  cx * 6, 0);
	pListCtrl->InsertColumn(2, strFile, LVCFMT_LEFT, cx * 28, 0);

	// Opcion de fila completa para el control de lista (COMCTL32.DLL v4.71+)
	pListCtrl->SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion
	UpdateList();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Pagina activa
//
//---------------------------------------------------------------------------
BOOL CSASIPage::OnSetActive()
{
	CSpinButtonCtrl *pSpin;
	CSCSIPage *pSCSIPage;
	BOOL bEnable;

	// Clase base
	if (!CConfigPage::OnSetActive()) {
		return FALSE;
	}

	// Obtener la interfaz SCSI dinamicamente
	ASSERT(m_pSheet);
	pSCSIPage = (CSCSIPage*)m_pSheet->SearchPage(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(pSCSIPage);
	if (pSCSIPage->GetInterface(m_pConfig) == 2) {
		// Interfaz SCSI interna (SASI no disponible)
		bEnable = FALSE;
	}
	else {
		// SASI o interfaz SCSI externa
		bEnable = TRUE;
	}

	// Controles activados/desactivados
	if (bEnable) {
		// ・ｽL・ｽ・ｽ・ｽﾌ場合・ｽA・ｽX・ｽs・ｽ・ｽBotones・ｽ・ｽ・ｽ迪ｻ・ｽﾝゑｿｽNumero de unidades・ｽ・ｽ・ｽ謫ｾ
		pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SASI_DRIVES);
		ASSERT(pSpin);
		if (pSpin->GetPos() > 0 ) {
			// Lista activa / Unidad activa
			EnableControls(TRUE, TRUE);
		}
		else {
			// Lista inactiva / Unidad activa
			EnableControls(FALSE, TRUE);
		}
	}
	else {
		// Lista inactiva / Unidad inactiva
		EnableControls(FALSE, FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CSASIPage::OnOK()
{
	int i;
	TCHAR szPath[FILEPATH_MAX];
	CButton *pButton;
	CListCtrl *pListCtrl;

	// Numero de unidades
	ASSERT((m_nDrives >= 0) && (m_nDrives <= SASI::SASIMax));
	m_pConfig->sasi_drives = m_nDrives;

	// Nombre de archivo
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SASI_LIST);
	ASSERT(pListCtrl);
	for (i=0; i<m_nDrives; i++) {
		pListCtrl->GetItemText(i, 2, szPath, FILEPATH_MAX);
		_tcscpy(m_pConfig->sasi_file[i], szPath);
	}

	// Verificacion・ｽ{・ｽb・ｽN・ｽX(SASI・ｽESCSI・ｽﾆゑｿｽ・ｽ・ｽ・ｽﾊ設抵ｿｽ)
	pButton = (CButton*)GetDlgItem(IDC_SASI_MEMSWB);
	ASSERT(pButton);
	if (pButton->GetCheck() == 1) {
		m_pConfig->sasi_sramsync = TRUE;
		m_pConfig->scsi_sramsync = TRUE;
	}
	else {
		m_pConfig->sasi_sramsync = FALSE;
		m_pConfig->scsi_sramsync = FALSE;
	}

	// Clase base
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Desplazamiento vertical
//
//---------------------------------------------------------------------------
void CSASIPage::OnVScroll(UINT /*nSBCode*/, UINT nPos, CScrollBar* /*pBar*/)
{
	ASSERT(this);
	ASSERT(nPos <= SASI::SASIMax);

	// Numero de unidadesActualizacion
	m_nDrives = nPos;

	// Controles activados/desactivados
	if (m_nDrives > 0) {
		EnableControls(TRUE);
	}
	else {
		EnableControls(FALSE);
	}

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion
	UpdateList();
}

//---------------------------------------------------------------------------
//
//	Clic en control de lista
//
//---------------------------------------------------------------------------
void CSASIPage::OnClick(NMHDR* /*pNMHDR*/, LRESULT* /*pResult*/)
{
	CListCtrl *pListCtrl;
	int i;
	int nID;
	int nCount;
	TCHAR szPath[FILEPATH_MAX];

	// ・ｽ・ｽ・ｽX・ｽgObtener control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SASI_LIST);
	ASSERT(pListCtrl);

	// Obtener conteo
	nCount = pListCtrl->GetItemCount();

	// Obtener ID seleccionado
	nID = -1;
	for (i=0; i<nCount; i++) {
		if (pListCtrl->GetItemState(i, LVIS_SELECTED)) {
			nID = i;
			break;
		}
	}
	if (nID < 0) {
		return;
	}

	// Intentar abrir
	_tcscpy(szPath, m_szFile[nID]);
	if (!::FileOpenDlg(this, szPath, IDS_SASIOPEN)) {
		return;
	}

	// ・ｽp・ｽX・ｽ・ｽActualizacion
	_tcscpy(m_szFile[nID], szPath);

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion
	UpdateList();
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion
//
//---------------------------------------------------------------------------
void FASTCALL CSASIPage::UpdateList()
{
	CListCtrl *pListCtrl;
	int nCount;
	int i;
	CString strID;
	CString strDisk;
	CString strCtrl;
	DWORD dwDisk[SASI::SASIMax];

	// Obtener el numero actual del control de lista
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SASI_LIST);
	ASSERT(pListCtrl);
	nCount = pListCtrl->GetItemCount();

	// Si hay mas elementos en la lista, recortar la parte final
	while (nCount > m_nDrives) {
		pListCtrl->DeleteItem(nCount - 1);
		nCount--;
	}

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾈゑｿｽ・ｽ・ｽ・ｽ・ｽ・ｽﾍ、Agregar・ｽ・ｽ・ｽ・ｽ
	while (m_nDrives > nCount) {
		strID.Format(_T("%d"), nCount + 1);
		pListCtrl->InsertItem(nCount, strID);
		nCount++;
	}

	// ・ｽ・ｽ・ｽf・ｽBVerificacion(m_nDrive・ｽ・ｽ・ｽ・ｽ・ｽﾜとめて行・ｽﾈゑｿｽ)
	CheckSASI(dwDisk);

	// ・ｽ・ｽrBucle
	for (i=0; i<nCount; i++) {
		// ・ｽ・ｽ・ｽf・ｽBVerificacion・ｽﾌ鯉ｿｽ・ｽﾊにゑｿｽ・ｽA・ｽ・ｽ・ｽ・ｽ・ｽ・ｽCrear
		if (dwDisk[i] == 0) {
			// Desconocido
			strDisk = m_strError;
		}
		else {
			// MBMostrar
			strDisk.Format(_T("%uMB"), dwDisk[i]);
		}

		// Comparar y establecer
		strCtrl = pListCtrl->GetItemText(i, 1);
		if (strDisk != strCtrl) {
			pListCtrl->SetItemText(i, 1, strDisk);
		}

		// Nombre de archivo
		strDisk = m_szFile[i];
		strCtrl = pListCtrl->GetItemText(i, 2);
		if (strDisk != strCtrl) {
			pListCtrl->SetItemText(i, 2, strDisk);
		}
	}
}

//---------------------------------------------------------------------------
//
//	SASI・ｽh・ｽ・ｽ・ｽC・ｽuVerificacion
//
//---------------------------------------------------------------------------
void FASTCALL CSASIPage::CheckSASI(DWORD *pDisk)
{
	int i;
	DWORD dwSize;
	Fileio fio;

	ASSERT(this);
	ASSERT(pDisk);

	// Bloqueo de VM
	::LockVM();

	// ・ｽh・ｽ・ｽ・ｽC・ｽuBucle
	for (i=0; i<m_nDrives; i++) {
		// Tama?o 0
		pDisk[i] = 0;

		// Intentar abrir
		if (!fio.Open(m_szFile[i], Fileio::ReadOnly)) {
			continue;
		}

		// Obtener tama?o, cerrar
		dwSize = fio.GetFileSize();
		fio.Close();

		// ・ｽT・ｽC・ｽYVerificacion
		switch (dwSize) {
			case 0x9f5400:
				pDisk[i] = 10;
				break;

			// 20MB
			case 0x13c9800:
				pDisk[i] = 20;
				break;

			// 40MB
			case 0x2793000:
				pDisk[i] = 40;
				break;

			default:
				break;
		}
	}

	// Desbloquear
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	SASINumero de unidades・ｽ謫ｾ
//
//---------------------------------------------------------------------------
int FASTCALL CSASIPage::GetDrives(const Config *pConfig) const
{
	ASSERT(this);
	ASSERT(pConfig);

	// Inicializacion・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽﾈゑｿｽ・ｽ・ｽﾎ、・ｽ^・ｽ・ｽ・ｽ・ｽ黷ｽConfig・ｽ・ｽ・ｽ・ｽ
	if (!m_bInit) {
		return pConfig->sasi_drives;
	}

	// Inicializacion・ｽﾏみなゑｿｽA・ｽ・ｽ・ｽﾝの値・ｽ・ｽ
	return m_nDrives;
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CSASIPage::EnableControls(BOOL bEnable, BOOL bDrive)
{
	CListCtrl *pListCtrl;
	CWnd *pWnd;

	ASSERT(this);

	// Control de lista (bEnable)
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SASI_LIST);
	ASSERT(pListCtrl);
	pListCtrl->EnableWindow(bEnable);

	// Numero de unidades(bDrive)
	pWnd = GetDlgItem(IDC_SASI_DRIVEL);
	ASSERT(pWnd);
	pWnd->EnableWindow(bDrive);
	pWnd = GetDlgItem(IDC_SASI_DRIVEE);
	ASSERT(pWnd);
	pWnd->EnableWindow(bDrive);
	pWnd = GetDlgItem(IDC_SASI_DRIVES);
	ASSERT(pWnd);
	pWnd->EnableWindow(bDrive);
}

//===========================================================================
//
//	Pagina SxSI
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSxSIPage::CSxSIPage()
{
	int i;

	// Configurar siempre ID y Help
	m_dwID = MAKEID('S', 'X', 'S', 'I');
	m_nTemplate = IDD_SXSIPAGE;
	m_uHelpID = IDC_SXSI_HELP;

	// Inicializacion(Otros・ｽf・ｽ[・ｽ^)
	m_nSASIDrives = 0;
	for (i=0; i<8; i++) {
		m_DevMap[i] = DevNone;
	}
	ASSERT(SASI::SCSIMax == 6);
	for (i=0; i<SASI::SCSIMax; i++) {
		m_szFile[i][0] = _T('\0');
	}

	// ・ｽ・ｽInicializacion
	m_bInit = FALSE;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSxSIPage, CConfigPage)
	ON_WM_VSCROLL()
	ON_NOTIFY(NM_CLICK, IDC_SXSI_LIST, OnClick)
	ON_BN_CLICKED(IDC_SXSI_MOCHECK, OnCheck)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	・ｽy・ｽ[・ｽWInicializacion
//
//---------------------------------------------------------------------------
BOOL CSxSIPage::OnInitDialog()
{
	int i;
	int nMax;
	int nDrives;
	CSASIPage *pSASIPage;
	CSpinButtonCtrl *pSpin;
	CButton *pButton;
	CListCtrl *pListCtrl;
	CDC *pDC;
	TEXTMETRIC tm;
	LONG cx;
	CString strCap;
	CString strFile;

	// Clase base
	CConfigPage::OnInitDialog();

	// Inicializacion・ｽt・ｽ・ｽ・ｽOUp
	m_bInit = TRUE;

	// Pagina SASI・ｽ謫ｾ
	ASSERT(m_pSheet);
	pSASIPage = (CSASIPage*)m_pSheet->SearchPage(MAKEID('S', 'A', 'S', 'I'));
	ASSERT(pSASIPage);

	// SASI・ｽﾌ設抵ｿｽNumero de unidades・ｽ・ｽ・ｽ・ｽASCSI・ｽﾉ設抵ｿｽﾅゑｿｽ・ｽ・ｽﾅ托ｿｽNumero de unidades・ｽｾゑｿｽ
	m_nSASIDrives = pSASIPage->GetDrives(m_pConfig);
	nMax = m_nSASIDrives;
	nMax = (nMax + 1) >> 1;
	ASSERT((nMax >= 0) && (nMax <= 8));
	if (nMax >= 7) {
		nMax = 0;
	}
	else {
		nMax = 7 - nMax;
	}

	// SCSI・ｽﾌ最托ｿｽNumero de unidades・ｽｧ鯉ｿｽ
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SXSI_DRIVES);
	pSpin->SetBase(10);
	nDrives = m_pConfig->sxsi_drives;
	if (nDrives > nMax) {
		nDrives = nMax;
	}
	pSpin->SetRange(0, (short)nMax);
	pSpin->SetPos(nDrives);

	// SCSI・ｽ・ｽNombre de archivo・ｽ・ｽ・ｽ謫ｾ
	for (i=0; i<6; i++) {
		_tcscpy(m_szFile[i], m_pConfig->sxsi_file[i]);
	}

	// Configurar flag de prioridad MO
	pButton = (CButton*)GetDlgItem(IDC_SXSI_MOCHECK);
	if (m_pConfig->sxsi_mofirst) {
		pButton->SetCheck(1);
	}
	else {
		pButton->SetCheck(0);
	}

	// Obtener metricas de texto
	pDC = new CClientDC(this);
	::GetTextMetrics(pDC->m_hDC, &tm);
	delete pDC;
	cx = tm.tmAveCharWidth;

	// ・ｽ・ｽ・ｽX・ｽgConfiguracion de controles
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SXSI_LIST);
	ASSERT(pListCtrl);
	pListCtrl->DeleteAllItems();
	if (::IsJapanese()) {
		pListCtrl->InsertColumn(0, _T("ID"), LVCFMT_LEFT, cx * 3, 0);
	}
	else {
		pListCtrl->InsertColumn(0, _T("ID"), LVCFMT_LEFT, cx * 4, 0);
	}
	::GetMsg(IDS_SXSI_CAPACITY, strCap);
	pListCtrl->InsertColumn(1, strCap, LVCFMT_CENTER,  cx * 7, 0);
	::GetMsg(IDS_SXSI_FILENAME, strFile);
	pListCtrl->InsertColumn(2, strFile, LVCFMT_LEFT, cx * 26, 0);

	// Opcion de fila completa para el control de lista (COMCTL32.DLL v4.71+)
	pListCtrl->SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

	// Obtener cadenas para el control de lista
	::GetMsg(IDS_SXSI_SASI, m_strSASI);
	::GetMsg(IDS_SXSI_MO, m_strMO);
	::GetMsg(IDS_SXSI_INIT, m_strInit);
	::GetMsg(IDS_SXSI_NONE, m_strNone);
	::GetMsg(IDS_SXSI_DEVERROR, m_strError);

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion
	UpdateList();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Pagina activa
//
//---------------------------------------------------------------------------
BOOL CSxSIPage::OnSetActive()
{
	int nMax;
	int nPos;
	CSpinButtonCtrl *pSpin;
	BOOL bEnable;
	CSASIPage *pSASIPage;
	CSCSIPage *pSCSIPage;
	CAlterPage *pAlterPage;

	// Clase base
	if (!CConfigPage::OnSetActive()) {
		return FALSE;
	}

	// Obtener pagina
	ASSERT(m_pSheet);
	pSASIPage = (CSASIPage*)m_pSheet->SearchPage(MAKEID('S', 'A', 'S', 'I'));
	ASSERT(pSASIPage);
	pSCSIPage = (CSCSIPage*)m_pSheet->SearchPage(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(pSCSIPage);
	pAlterPage = (CAlterPage*)m_pSheet->SearchPage(MAKEID('A', 'L', 'T', ' '));
	ASSERT(pAlterPage);

	// Obtener dynamicamente el flag de habilitacion SxSI
	bEnable = TRUE;
	if (!pAlterPage->HasParity(m_pConfig)) {
		// Sin paridad configurada. SxSI no disponible
		bEnable = FALSE;
	}
	if (pSCSIPage->GetInterface(m_pConfig) != 0) {
		// Interfaz SCSI interna o externa. SxSI no disponible
		bEnable = FALSE;
	}

	// SASI・ｽ・ｽNumero de unidades・ｽ・ｽ・ｽ謫ｾ・ｽ・ｽ・ｽASCSI・ｽﾌ最托ｿｽNumero de unidades・ｽｾゑｿｽ
	m_nSASIDrives = pSASIPage->GetDrives(m_pConfig);
	nMax = m_nSASIDrives;
	nMax = (nMax + 1) >> 1;
	ASSERT((nMax >= 0) && (nMax <= 8));
	if (nMax >= 7) {
		nMax = 0;
	}
	else {
		nMax = 7 - nMax;
	}

	// SCSI・ｽﾌ最托ｿｽNumero de unidades・ｽｧ鯉ｿｽ
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SXSI_DRIVES);
	ASSERT(pSpin);
	nPos = LOWORD(pSpin->GetPos());
	if (nPos > nMax) {
		nPos = nMax;
		pSpin->SetPos(nPos);
	}
	pSpin->SetRange(0, (short)nMax);

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion
	UpdateList();

	// Controles activados/desactivados
	if (bEnable) {
		if (nPos > 0) {
			// Lista activa / Unidad activa
			EnableControls(TRUE, TRUE);
		}
		else {
			// ・ｽ・ｽ・ｽX・ｽg・ｽL・ｽ・ｽ・ｽE・ｽh・ｽ・ｽ・ｽC・ｽu・ｽ・ｽ・ｽ・ｽ
			EnableControls(FALSE, TRUE);
		}
	}
	else {
		// Lista inactiva / Unidad inactiva
		EnableControls(FALSE, FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Desplazamiento vertical
//
//---------------------------------------------------------------------------
void CSxSIPage::OnVScroll(UINT /*nSBCode*/, UINT nPos, CScrollBar* /*pBar*/)
{
	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion(・ｽ・ｽ・ｽ・ｽ・ｽ・ｽBuildMap・ｽ・ｽ・ｽs・ｽ・ｽ)
	UpdateList();

	// Controles activados/desactivados
	if (nPos > 0) {
		EnableControls(TRUE);
	}
	else {
		EnableControls(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Clic en control de lista
//
//---------------------------------------------------------------------------
void CSxSIPage::OnClick(NMHDR* /*pNMHDR*/, LRESULT* /*pResult*/)
{
	CListCtrl *pListCtrl;
	int i;
	int nID;
	int nCount;
	int nDrive;
	TCHAR szPath[FILEPATH_MAX];

	// ・ｽ・ｽ・ｽX・ｽgObtener control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SXSI_LIST);
	ASSERT(pListCtrl);

	// Obtener conteo
	nCount = pListCtrl->GetItemCount();

	// Obtener ID seleccionado
	nID = -1;
	for (i=0; i<nCount; i++) {
		if (pListCtrl->GetItemState(i, LVIS_SELECTED)) {
			nID = i;
			break;
		}
	}
	if (nID < 0) {
		return;
	}

	// Identificar tipo segun el mapa
	if (m_DevMap[nID] != DevSCSI) {
		return;
	}

	// Obtener indice de unidad desde ID (sin considerar MO)
	nDrive = 0;
	for (i=0; i<8; i++) {
		if (i == nID) {
			break;
		}
		if (m_DevMap[i] == DevSCSI) {
			nDrive++;
		}
	}
	ASSERT((nDrive >= 0) && (nDrive < SASI::SCSIMax));

	// Intentar abrir
	_tcscpy(szPath, m_szFile[nDrive]);
	if (!::FileOpenDlg(this, szPath, IDS_SCSIOPEN)) {
		return;
	}

	// ・ｽp・ｽX・ｽ・ｽActualizacion
	_tcscpy(m_szFile[nDrive], szPath);

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion
	UpdateList();
}

//---------------------------------------------------------------------------
//
//	Verificacion・ｽ{・ｽb・ｽN・ｽXModificacion
//
//---------------------------------------------------------------------------
void CSxSIPage::OnCheck()
{
	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion(・ｽ・ｽ・ｽ・ｽ・ｽ・ｽBuildMap・ｽ・ｽ・ｽs・ｽ・ｽ)
	UpdateList();
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CSxSIPage::OnOK()
{
	CSpinButtonCtrl *pSpin;
	CButton *pButton;
	int i;

	// Numero de unidades
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SXSI_DRIVES);
	ASSERT(pSpin);
	m_pConfig->sxsi_drives = LOWORD(pSpin->GetPos());

	// Flag de prioridad MO
	pButton = (CButton*)GetDlgItem(IDC_SXSI_MOCHECK);
	ASSERT(pButton);
	if (pButton->GetCheck() == 1) {
		m_pConfig->sxsi_mofirst = TRUE;
	}
	else {
		m_pConfig->sxsi_mofirst = FALSE;
	}

	// Nombre de archivo
	for (i=0; i<SASI::SCSIMax; i++) {
		_tcscpy(m_pConfig->sxsi_file[i], m_szFile[i]);
	}

	// Clase base
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion
//
//---------------------------------------------------------------------------
void FASTCALL CSxSIPage::UpdateList()
{
	int i;
	int nDrive;
	int nDev;
	int nCount;
	int nCap;
	CListCtrl *pListCtrl;
	CString strCtrl;
	CString strID;
	CString strSize;
	CString strFile;

	ASSERT(this);

	// Construir mapa
	BuildMap();

	// ・ｽ・ｽ・ｽX・ｽgObtener control・ｽA・ｽJ・ｽE・ｽ・ｽ・ｽg・ｽ謫ｾ
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SXSI_LIST);
	ASSERT(pListCtrl);
	nCount = pListCtrl->GetItemCount();

	// Contar elementos que no son "None" en el mapa
	nDev = 0;
	for (i=0; i<8; i++) {
		if (m_DevMap[i] != DevNone) {
			nDev++;
		}
	}

	// Crear items para nDev
	while (nCount > nDev) {
		pListCtrl->DeleteItem(nCount - 1);
		nCount--;
	}
	while (nDev > nCount) {
		strID.Format(_T("%d"), nCount + 1);
		pListCtrl->InsertItem(nCount, strID);
		nCount++;
	}

	// ・ｽ・ｽrBucle
	nDrive = 0;
	nDev = 0;
	for (i=0; i<8; i++) {
		// Crear cadena segun el tipo
		switch (m_DevMap[i]) {
			// Disco duro SASI
			case DevSASI:
				strSize = m_strNone;
				strFile = m_strSASI;
				break;

			// Disco duro SCSI
			case DevSCSI:
				nCap = CheckSCSI(nDrive);
				if (nCap > 0) {
					strSize.Format("%dMB", nCap);
				}
				else {
					strSize = m_strError;
				}
				strFile = m_szFile[nDrive];
				nDrive++;
				break;

			// Disco MO SCSI
			case DevMO:
				strSize = m_strNone;
				strFile = m_strMO;
				break;

			// Iniciador (Host)
			case DevInit:
				strSize = m_strNone;
				strFile = m_strInit;
				break;

			// Sin dispositivo
			case DevNone:
				// ・ｽ・ｽ・ｽﾉ進・ｽ・ｽ
				continue;

			// Otros(・ｽ・ｽ・ｽ闢ｾ・ｽﾈゑｿｽ)
			default:
				ASSERT(FALSE);
				return;
		}

		// ID
		strID.Format(_T("%d"), i);
		strCtrl = pListCtrl->GetItemText(nDev, 0);
		if (strID != strCtrl) {
			pListCtrl->SetItemText(nDev, 0, strID);
		}

		// Capacidad
		strCtrl = pListCtrl->GetItemText(nDev, 1);
		if (strSize != strCtrl) {
			pListCtrl->SetItemText(nDev, 1, strSize);
		}

		// Nombre de archivo
		strCtrl = pListCtrl->GetItemText(nDev, 2);
		if (strFile != strCtrl) {
			pListCtrl->SetItemText(nDev, 2, strFile);
		}

		// Siguiente
		nDev++;
	}
}

//---------------------------------------------------------------------------
//
//	・ｽ}・ｽb・ｽvCrear
//
//---------------------------------------------------------------------------
void FASTCALL CSxSIPage::BuildMap()
{
	int nSASI;
	int nMO;
	int nSCSI;
	int nInit;
	int nMax;
	int nID;
	int i;
	BOOL bMOFirst;
	CButton *pButton;
	CSpinButtonCtrl *pSpin;

	ASSERT(this);

	// Inicializacion
	nSASI = 0;
	nMO = 0;
	nSCSI = 0;
	nInit = 0;

	// Flag de prioridad MO・ｽ・ｽ・ｽ謫ｾ
	pButton = (CButton*)GetDlgItem(IDC_SXSI_MOCHECK);
	ASSERT(pButton);
	bMOFirst = FALSE;
	if (pButton->GetCheck() != 0) {
		bMOFirst = TRUE;
	}

	// SASINumero de unidades・ｽ・ｽ・ｽ・ｽASASI・ｽﾌ撰ｿｽLID・ｽ・ｽ・ｽｾゑｿｽ
	ASSERT((m_nSASIDrives >= 0) && (m_nSASIDrives <= 0x10));
	nSASI = m_nSASIDrives;
	nSASI = (nSASI + 1) >> 1;

	// Obtener maximo de MO, SCSI, INIT desde SASI
	if (nSASI <= 6) {
		nMO = 1;
		nSCSI = 6 - nSASI;
	}
	if (nSASI <= 7) {
		nInit = 1;
	}

	// SxSINumero de unidades・ｽﾌ設抵ｿｽ・ｽ・ｽ・ｽ・ｽﾄ、・ｽl・ｽｲ撰ｿｽ
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SXSI_DRIVES);
	ASSERT(pSpin);
	nMax = LOWORD(pSpin->GetPos());
	ASSERT((nMax >= 0) && (nMax <= (nSCSI + nMO)));
	if (nMax == 0) {
		// SxSINumero de unidades・ｽ・ｽ0
		nMO = 0;
		nSCSI = 0;
	}
	else {
		// Agrupar temporalmente HD+MO en nSCSI
		nSCSI = nMax;

		// Si es 1, solo MO
		if (nMax == 1) {
			nMO = 1;
			nSCSI = 0;
		}
		else {
			// Si es 2 o mas, asignar uno a MO
			nSCSI--;
			nMO = 1;
		}
	}

	// Reiniciar ID
	nID = 0;

	// ・ｽI・ｽ[・ｽ・ｽLimpiar
	for (i=0; i<8; i++) {
		m_DevMap[i] = DevNone;
	}

	// Establecer SASI
	for (i=0; i<nSASI; i++) {
		m_DevMap[nID] = DevSASI;
		nID++;
	}

	// Establecer SCSI, MO
	if (bMOFirst) {
		// Prioridad MO
		for (i=0; i<nMO; i++) {
			m_DevMap[nID] = DevMO;
			nID++;
		}
		for (i=0; i<nSCSI; i++) {
			m_DevMap[nID] = DevSCSI;
			nID++;
		}
	}
	else {
		// Prioridad HD
		for (i=0; i<nSCSI; i++) {
			m_DevMap[nID] = DevSCSI;
			nID++;
		}
		for (i=0; i<nMO; i++) {
			m_DevMap[nID] = DevMO;
			nID++;
		}
	}

	// Establecer iniciador
	for (i=0; i<nInit; i++) {
		ASSERT(nID <= 7);
		m_DevMap[7] = DevInit;
	}
}

//---------------------------------------------------------------------------
//
//	SCSI・ｽn・ｽ[・ｽh・ｽf・ｽB・ｽX・ｽNCapacidadVerificacion
//	* Devuelve 0 en caso de error de dispositivo
//
//---------------------------------------------------------------------------
int FASTCALL CSxSIPage::CheckSCSI(int nDrive)
{
	Fileio fio;
	DWORD dwSize;

	ASSERT(this);
	ASSERT((nDrive >= 0) && (nDrive <= 5));

	// Bloquear
	::LockVM();

	// Abrir archivo
	if (!fio.Open(m_szFile[nDrive], Fileio::ReadOnly)) {
		// Error, devuelve 0
		fio.Close();
		::UnlockVM();
		return 0;
	}

	// Capacidad・ｽ謫ｾ
	dwSize = fio.GetFileSize();

	// Desbloquear
	fio.Close();
	::UnlockVM();

	// ・ｽt・ｽ@・ｽC・ｽ・ｽ・ｽT・ｽC・ｽY・ｽ・ｽVerificacion(512・ｽo・ｽC・ｽg・ｽP・ｽ・ｽ)
	if ((dwSize & 0x1ff) != 0) {
		return 0;
	}

	// ・ｽt・ｽ@・ｽC・ｽ・ｽ・ｽT・ｽC・ｽY・ｽ・ｽVerificacion(10MB・ｽﾈ擾ｿｽ)
	if (dwSize < 10 * 0x400 * 0x400) {
		return 0;
	}

	// ・ｽt・ｽ@・ｽC・ｽ・ｽ・ｽT・ｽC・ｽY・ｽ・ｽVerificacion(1016MB・ｽﾈ会ｿｽ)
	if (dwSize > 1016 * 0x400 * 0x400) {
		return 0;
	}

	// Devolver tama?o
	dwSize >>= 20;
	return dwSize;
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void CSxSIPage::EnableControls(BOOL bEnable, BOOL bDrive)
{
	int i;
	CWnd *pWnd;
	CListCtrl *pListCtrl;

	ASSERT(this);

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽEMOVerificacion・ｽﾈ外・ｽﾌ全・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ・ｽﾝ抵ｿｽ
	for (i=0; ; i++) {
		// Obtener control
		if (!ControlTable[i]) {
			break;
		}
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);

		// ・ｽﾝ抵ｿｽ
		pWnd->EnableWindow(bDrive);
	}

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ・ｽﾝ抵ｿｽ
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SXSI_LIST);
	ASSERT(pListCtrl);
	pListCtrl->EnableWindow(bEnable);

	// MOVerificacion・ｽ・ｽﾝ抵ｿｽ
	pWnd = GetDlgItem(IDC_SXSI_MOCHECK);
	ASSERT(pWnd);
	pWnd->EnableWindow(bEnable);
}

//---------------------------------------------------------------------------
//
//	Numero de unidades・ｽ謫ｾ
//
//---------------------------------------------------------------------------
int FASTCALL CSxSIPage::GetDrives(const Config *pConfig) const
{
	BOOL bEnable;
	CSASIPage *pSASIPage;
	CSCSIPage *pSCSIPage;
	CAlterPage *pAlterPage;
	CSpinButtonCtrl *pSpin;
	int nPos;

	ASSERT(this);
	ASSERT(pConfig);

	// Obtener pagina
	ASSERT(m_pSheet);
	pSASIPage = (CSASIPage*)m_pSheet->SearchPage(MAKEID('S', 'A', 'S', 'I'));
	ASSERT(pSASIPage);
	pSCSIPage = (CSCSIPage*)m_pSheet->SearchPage(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(pSCSIPage);
	pAlterPage = (CAlterPage*)m_pSheet->SearchPage(MAKEID('A', 'L', 'T', ' '));
	ASSERT(pAlterPage);

	// Obtener dynamicamente el flag de habilitacion SxSI
	bEnable = TRUE;
	if (!pAlterPage->HasParity(pConfig)) {
		// Sin paridad configurada. SxSI no disponible
		bEnable = FALSE;
	}
	if (pSCSIPage->GetInterface(pConfig) != 0) {
		// Interfaz SCSI interna o externa. SxSI no disponible
		bEnable = FALSE;
	}
	if (pSASIPage->GetDrives(pConfig) >= 12) {
		// SASINumero de unidades・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽBSxSI・ｽﾍ使・ｽp・ｽﾅゑｿｽ・ｽﾈゑｿｽ
		bEnable = FALSE;
	}

	// Si no esta disponible, es 0
	if (!bEnable) {
		return 0;
	}

	// ・ｽ・ｽInicializacion・ｽﾌ場合・ｽA・ｽﾝ抵ｿｽl・ｽ・ｽﾔゑｿｽ
	if (!m_bInit) {
		return pConfig->sxsi_drives;
	}

	// ・ｽ・ｽ・ｽ・ｽEditar・ｽ・ｽ・ｽﾌ値・ｽ・ｽﾔゑｿｽ
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SXSI_DRIVES);
	ASSERT(pSpin);
	nPos = LOWORD(pSpin->GetPos());
	return nPos;
}

//---------------------------------------------------------------------------
//
//	Tabla de controles
//
//---------------------------------------------------------------------------
const UINT CSxSIPage::ControlTable[] = {
	IDC_SXSI_GROUP,
	IDC_SXSI_DRIVEL,
	IDC_SXSI_DRIVEE,
	IDC_SXSI_DRIVES,
	NULL
};

//===========================================================================
//
//	Pagina SCSI
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSCSIPage::CSCSIPage()
{
	int i;

	// Configurar siempre ID y Help
	m_dwID = MAKEID('S', 'C', 'S', 'I');
	m_nTemplate = IDD_SCSIPAGE;
	m_uHelpID = IDC_SCSI_HELP;

	// Obtener SCSI
	m_pSCSI = (SCSI*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(m_pSCSI);

	// Inicializacion(Otros・ｽf・ｽ[・ｽ^)
	m_bInit = FALSE;
	m_nDrives = 0;
	m_bMOFirst = FALSE;

	// Mapa de dispositivos
	ASSERT(SCSI::DeviceMax == 8);
	for (i=0; i<SCSI::DeviceMax; i++) {
		m_DevMap[i] = DevNone;
	}

	// Rutas de archivos
	ASSERT(SCSI::HDMax == 5);
	for (i=0; i<SCSI::HDMax; i++) {
		m_szFile[i][0] = _T('\0');
	}
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSCSIPage, CConfigPage)
	ON_WM_VSCROLL()
	ON_NOTIFY(NM_CLICK, IDC_SCSI_LIST, OnClick)
	ON_BN_CLICKED(IDC_SCSI_NONEB, OnButton)
	ON_BN_CLICKED(IDC_SCSI_INTB, OnButton)
	ON_BN_CLICKED(IDC_SCSI_EXTB, OnButton)
	ON_BN_CLICKED(IDC_SCSI_MOCHECK, OnCheck)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	・ｽy・ｽ[・ｽWInicializacion
//
//---------------------------------------------------------------------------
BOOL CSCSIPage::OnInitDialog()
{
	int i;
	BOOL bAvail;
	BOOL bEnable[2];
	CButton *pButton;
	CSpinButtonCtrl *pSpin;
	CDC *pDC;
	TEXTMETRIC tm;
	LONG cx;
	CListCtrl *pListCtrl;
	CString strCap;
	CString strFile;

	// Clase base
	CConfigPage::OnInitDialog();

	// Inicializacion・ｽt・ｽ・ｽ・ｽOUp
	m_bInit = TRUE;

	// ROM・ｽﾌ有・ｽ・ｽ・ｽﾉ会ｿｽ・ｽ・ｽ・ｽﾄ、・ｽC・ｽ・ｽ・ｽ^・ｽt・ｽF・ｽ[・ｽX・ｽ・ｽ・ｽW・ｽIBotones・ｽ・ｽ・ｽﾖ止
	pButton = (CButton*)GetDlgItem(IDC_SCSI_EXTB);
	ASSERT(pButton);
	bEnable[0] = CheckROM(1);
	pButton->EnableWindow(bEnable[0]);
	pButton = (CButton*)GetDlgItem(IDC_SCSI_INTB);
	ASSERT(pButton);
	bEnable[1] = CheckROM(2);
	pButton->EnableWindow(bEnable[1]);

	// Tipo de interfaz
	pButton = (CButton*)GetDlgItem(IDC_SCSI_NONEB);
	bAvail = FALSE;
	switch (m_pConfig->mem_type) {
		// No instalar
		case Memory::None:
		case Memory::SASI:
			break;

		// Externo
		case Memory::SCSIExt:
			// ExternoROM・ｽ・ｽ・ｽ・ｽ・ｽﾝゑｿｽ・ｽ・ｽ鼾・ｿｽﾌゑｿｽ
			if (bEnable[0]) {
				pButton = (CButton*)GetDlgItem(IDC_SCSI_EXTB);
				bAvail = TRUE;
			}
			break;

		// Otros(Interno)
		default:
			// InternoROM・ｽ・ｽ・ｽ・ｽ・ｽﾝゑｿｽ・ｽ・ｽ鼾・ｿｽﾌゑｿｽ
			if (bEnable[1]) {
				pButton = (CButton*)GetDlgItem(IDC_SCSI_INTB);
				bAvail = TRUE;
			}
			break;
	}
	ASSERT(pButton);
	pButton->SetCheck(1);

	// Numero de unidades
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SCSI_DRIVES);
	pSpin->SetBase(10);
	pSpin->SetRange(0, 7);
	m_nDrives = m_pConfig->scsi_drives;
	ASSERT((m_nDrives >= 0) && (m_nDrives <= 7));
	pSpin->SetPos(m_nDrives);

	// Flag de prioridad MO
	pButton = (CButton*)GetDlgItem(IDC_SCSI_MOCHECK);
	ASSERT(pButton);
	if (m_pConfig->scsi_mofirst) {
		pButton->SetCheck(1);
		m_bMOFirst = TRUE;
	}
	else {
		pButton->SetCheck(0);
		m_bMOFirst = FALSE;
	}

	// SCSI-HDRutas de archivos
	for (i=0; i<SCSI::HDMax; i++) {
		_tcscpy(m_szFile[i], m_pConfig->scsi_file[i]);
	}

	// Obtener metricas de texto
	pDC = new CClientDC(this);
	::GetTextMetrics(pDC->m_hDC, &tm);
	delete pDC;
	cx = tm.tmAveCharWidth;

	// ・ｽ・ｽ・ｽX・ｽgConfiguracion de controles
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SCSI_LIST);
	ASSERT(pListCtrl);
	pListCtrl->DeleteAllItems();
	if (::IsJapanese()) {
		pListCtrl->InsertColumn(0, _T("ID"), LVCFMT_LEFT, cx * 3, 0);
	}
	else {
		pListCtrl->InsertColumn(0, _T("ID"), LVCFMT_LEFT, cx * 4, 0);
	}
	::GetMsg(IDS_SCSI_CAPACITY, strCap);
	pListCtrl->InsertColumn(1, strCap, LVCFMT_CENTER,  cx * 7, 0);
	::GetMsg(IDS_SCSI_FILENAME, strFile);
	pListCtrl->InsertColumn(2, strFile, LVCFMT_LEFT, cx * 26, 0);

	// Opcion de fila completa para el control de lista (COMCTL32.DLL v4.71+)
	pListCtrl->SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

	// Obtener cadenas para el control de lista
	::GetMsg(IDS_SCSI_MO, m_strMO);
	::GetMsg(IDS_SCSI_CD, m_strCD);
	::GetMsg(IDS_SCSI_INIT, m_strInit);
	::GetMsg(IDS_SCSI_NONE, m_strNone);
	::GetMsg(IDS_SCSI_DEVERROR, m_strError);

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion(・ｽ・ｽ・ｽ・ｽ・ｽ・ｽBuildMap・ｽ・ｽ・ｽs・ｽ・ｽ)
	UpdateList();

	// Controles activados/desactivados
	if (bAvail) {
		if (m_nDrives > 0) {
			// Lista activa / Unidad activa
			EnableControls(TRUE, TRUE);
		}
		else {
			// Lista inactiva / Unidad activa
			EnableControls(FALSE, TRUE);
		}
	}
	else {
		// Lista inactiva / Unidad inactiva
		EnableControls(FALSE, FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CSCSIPage::OnOK()
{
	int i;

	// Tipo de interfaz・ｽ・ｽ・ｽ辜・ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽﾊ設抵ｿｽ
	switch (GetIfCtrl()) {
		// No instalar
		case 0:
			m_pConfig->mem_type = Memory::SASI;
			break;

		// Externo
		case 1:
			m_pConfig->mem_type = Memory::SCSIExt;
			break;

		// Interno
		case 2:
			// ・ｽ^・ｽC・ｽv・ｽ・ｽ・ｽ痰､・ｽ鼾・ｿｽﾌみ、SCSIInt・ｽ・ｽModificacion
			if ((m_pConfig->mem_type == Memory::SASI) || (m_pConfig->mem_type == Memory::SCSIExt)) {
				m_pConfig->mem_type = Memory::SCSIInt;
			}
			break;

		// Otros(・ｽ・ｽ・ｽ閧ｦ・ｽﾈゑｿｽ)
		default:
			ASSERT(FALSE);
	}

	// Numero de unidades
	m_pConfig->scsi_drives = m_nDrives;

	// Flag de prioridad MO
	m_pConfig->scsi_mofirst = m_bMOFirst;

	// SCSI-HDRutas de archivos
	for (i=0; i<SCSI::HDMax; i++) {
		_tcscpy(m_pConfig->scsi_file[i], m_szFile[i]);
	}

	// Clase base
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Desplazamiento vertical
//
//---------------------------------------------------------------------------
void CSCSIPage::OnVScroll(UINT /*nSBCode*/, UINT nPos, CScrollBar* /*pBar*/)
{
	// Numero de unidades・ｽ謫ｾ
	m_nDrives = nPos;

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion(・ｽ・ｽ・ｽ・ｽ・ｽ・ｽBuildMap・ｽ・ｽ・ｽs・ｽ・ｽ)
	UpdateList();

	// Controles activados/desactivados
	if (nPos > 0) {
		EnableControls(TRUE);
	}
	else {
		EnableControls(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Clic en control de lista
//
//---------------------------------------------------------------------------
void CSCSIPage::OnClick(NMHDR* /*pNMHDR*/, LRESULT* /*pResult*/)
{
	CListCtrl *pListCtrl;
	int i;
	int nID;
	int nCount;
	int nDrive;
	TCHAR szPath[FILEPATH_MAX];

	// ・ｽ・ｽ・ｽX・ｽgObtener control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SCSI_LIST);
	ASSERT(pListCtrl);

	// Obtener conteo
	nCount = pListCtrl->GetItemCount();

	// Obtener items seleccionados
	nID = -1;
	for (i=0; i<nCount; i++) {
		if (pListCtrl->GetItemState(i, LVIS_SELECTED)) {
			nID = i;
			break;
		}
	}
	if (nID < 0) {
		return;
	}

	// Obtener ID de los datos del item
	nID = (int)pListCtrl->GetItemData(nID);

	// Identificar tipo segun el mapa
	if (m_DevMap[nID] != DevSCSI) {
		return;
	}

	// Obtener indice de unidad desde ID (sin considerar MO)
	nDrive = 0;
	for (i=0; i<SCSI::DeviceMax; i++) {
		if (i == nID) {
			break;
		}
		if (m_DevMap[i] == DevSCSI) {
			nDrive++;
		}
	}
	ASSERT((nDrive >= 0) && (nDrive < SCSI::HDMax));

	// Intentar abrir
	_tcscpy(szPath, m_szFile[nDrive]);
	if (!::FileOpenDlg(this, szPath, IDS_SCSIOPEN)) {
		return;
	}

	// ・ｽp・ｽX・ｽ・ｽActualizacion
	_tcscpy(m_szFile[nDrive], szPath);

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion
	UpdateList();
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽW・ｽIBotonesModificacion
//
//---------------------------------------------------------------------------
void CSCSIPage::OnButton()
{
	CButton *pButton;

	// ・ｽC・ｽ・ｽ・ｽ^・ｽt・ｽF・ｽ[・ｽX・ｽ・ｽ・ｽ・ｽ・ｽ・ｽVerificacion・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽ驍ｩ
	pButton = (CButton*)GetDlgItem(IDC_SCSI_NONEB);
	ASSERT(pButton);
	if (pButton->GetCheck() != 0) {
		// Lista inactiva / Unidad inactiva
		EnableControls(FALSE, FALSE);
		return;
	}

	if (m_nDrives > 0) {
		// Lista activa / Unidad activa
		EnableControls(TRUE, TRUE);
	}
	else {
		// Lista inactiva / Unidad activa
		EnableControls(FALSE, TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	Verificacion・ｽ{・ｽb・ｽN・ｽXModificacion
//
//---------------------------------------------------------------------------
void CSCSIPage::OnCheck()
{
	CButton *pButton;

	// Obtener estado actual
	pButton = (CButton*)GetDlgItem(IDC_SCSI_MOCHECK);
	ASSERT(pButton);
	if (pButton->GetCheck() != 0) {
		m_bMOFirst = TRUE;
	}
	else {
		m_bMOFirst = FALSE;
	}

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion(・ｽ・ｽ・ｽ・ｽ・ｽ・ｽBuildMap・ｽ・ｽ・ｽs・ｽ・ｽ)
	UpdateList();
}

//---------------------------------------------------------------------------
//
//	Tipo de interfaz・ｽ謫ｾ
//
//---------------------------------------------------------------------------
int FASTCALL CSCSIPage::GetInterface(const Config *pConfig) const
{
	ASSERT(this);
	ASSERT(pConfig);

	// Inicializacion・ｽt・ｽ・ｽ・ｽO
	if (!m_bInit) {
		// Inicializacion・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽﾈゑｿｽ・ｽﾌで、Config・ｽ・ｽ・ｽ・ｽ謫ｾ
		switch (pConfig->mem_type) {
			// No instalar
			case Memory::None:
			case Memory::SASI:
				return 0;

			// Externo
			case Memory::SCSIExt:
				return 1;

			// Otros(Interno)
			default:
				return 2;
		}
	}

	// Inicializacion・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽﾌで、・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ謫ｾ
	return GetIfCtrl();
}

//---------------------------------------------------------------------------
//
//	Tipo de interfaz・ｽ謫ｾ(・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ・ｽ・ｽ)
//
//---------------------------------------------------------------------------
int FASTCALL CSCSIPage::GetIfCtrl() const
{
	CButton *pButton;

	ASSERT(this);

	// No instalar
	pButton = (CButton*)GetDlgItem(IDC_SCSI_NONEB);
	ASSERT(pButton);
	if (pButton->GetCheck() != 0) {
		return 0;
	}

	// Externo
	pButton = (CButton*)GetDlgItem(IDC_SCSI_EXTB);
	ASSERT(pButton);
	if (pButton->GetCheck() != 0) {
		return 1;
	}

	// Interno
	pButton = (CButton*)GetDlgItem(IDC_SCSI_INTB);
	ASSERT(pButton);
	ASSERT(pButton->GetCheck() != 0);
	return 2;
}

//---------------------------------------------------------------------------
//
//	ROMVerificacion
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSCSIPage::CheckROM(int nType) const
{
	Filepath path;
	Fileio fio;
	DWORD dwSize;

	ASSERT(this);
	ASSERT((nType >= 0) && (nType <= 2));

	// 0:Interno・ｽﾌ場合・ｽﾍ厄ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽOK
	if (nType == 0) {
		return TRUE;
	}

	// Rutas de archivosCrear
	if (nType == 1) {
		// Externo
		path.SysFile(Filepath::SCSIExt);
	}
	else {
		// Interno
		path.SysFile(Filepath::SCSIInt);
	}

	// Bloquear
	::LockVM();

	// Intentar abrir
	if (!fio.Open(path, Fileio::ReadOnly)) {
		::UnlockVM();
		return FALSE;
	}

	// ・ｽt・ｽ@・ｽC・ｽ・ｽ・ｽT・ｽC・ｽY・ｽ謫ｾ
	dwSize = fio.GetFileSize();
	fio.Close();
	::UnlockVM();

	if (nType == 1) {
		// Externo・ｽﾍ、0x2000・ｽo・ｽC・ｽg・ｽﾜゑｿｽ・ｽ・ｽ0x1fe0・ｽo・ｽC・ｽg(WinX68k・ｽ・ｽ・ｽ・ｽ・ｽﾅと互奇ｿｽ・ｽ・ｽ・ｽﾆゑｿｽ)
		if ((dwSize == 0x2000) || (dwSize == 0x1fe0)) {
			return TRUE;
		}
	}
	else {
		// Interno・ｽﾍ、0x2000・ｽo・ｽC・ｽg・ｽﾌゑｿｽ
		if (dwSize == 0x2000) {
			return TRUE;
		}
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion
//
//---------------------------------------------------------------------------
void FASTCALL CSCSIPage::UpdateList()
{
	int i;
	int nDrive;
	int nDev;
	int nCount;
	int nCap;
	CListCtrl *pListCtrl;
	CString strCtrl;
	CString strID;
	CString strSize;
	CString strFile;

	ASSERT(this);

	// Construir mapa
	BuildMap();

	// ・ｽ・ｽ・ｽX・ｽgObtener control・ｽA・ｽJ・ｽE・ｽ・ｽ・ｽg・ｽ謫ｾ
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SCSI_LIST);
	ASSERT(pListCtrl);
	nCount = pListCtrl->GetItemCount();

	// Contar elementos que no son "None" en el mapa
	nDev = 0;
	for (i=0; i<8; i++) {
		if (m_DevMap[i] != DevNone) {
			nDev++;
		}
	}

	// Crear items para nDev
	while (nCount > nDev) {
		pListCtrl->DeleteItem(nCount - 1);
		nCount--;
	}
	while (nDev > nCount) {
		strID.Format(_T("%d"), nCount + 1);
		pListCtrl->InsertItem(nCount, strID);
		nCount++;
	}

	// ・ｽ・ｽrBucle
	nDrive = 0;
	nDev = 0;
	for (i=0; i<SCSI::DeviceMax; i++) {
		// Crear cadena segun el tipo
		switch (m_DevMap[i]) {
			// Disco duro SCSI
			case DevSCSI:
				nCap = CheckSCSI(nDrive);
				if (nCap > 0) {
					strSize.Format("%dMB", nCap);
				}
				else {
					strSize = m_strError;
				}
				strFile = m_szFile[nDrive];
				nDrive++;
				break;

			// Disco MO SCSI
			case DevMO:
				strSize = m_strNone;
				strFile = m_strMO;
				break;

			// CD-ROM SCSI
			case DevCD:
				strSize = m_strNone;
				strFile = m_strCD;
				break;

			// Iniciador (Host)
			case DevInit:
				strSize = m_strNone;
				strFile = m_strInit;
				break;

			// Sin dispositivo
			case DevNone:
				// ・ｽ・ｽ・ｽﾉ進・ｽ・ｽ
				continue;

			// Otros(・ｽ・ｽ・ｽ闢ｾ・ｽﾈゑｿｽ)
			default:
				ASSERT(FALSE);
				return;
		}

		// ・ｽA・ｽC・ｽe・ｽ・ｽ・ｽf・ｽ[・ｽ^
		if ((int)pListCtrl->GetItemData(nDev) != i) {
			pListCtrl->SetItemData(nDev, (DWORD)i);
		}

		// ID
		strID.Format(_T("%d"), i);
		strCtrl = pListCtrl->GetItemText(nDev, 0);
		if (strID != strCtrl) {
			pListCtrl->SetItemText(nDev, 0, strID);
		}

		// Capacidad
		strCtrl = pListCtrl->GetItemText(nDev, 1);
		if (strSize != strCtrl) {
			pListCtrl->SetItemText(nDev, 1, strSize);
		}

		// Nombre de archivo
		strCtrl = pListCtrl->GetItemText(nDev, 2);
		if (strFile != strCtrl) {
			pListCtrl->SetItemText(nDev, 2, strFile);
		}

		// Siguiente
		nDev++;
	}
}

//---------------------------------------------------------------------------
//
//	・ｽ}・ｽb・ｽvCrear
//
//---------------------------------------------------------------------------
void FASTCALL CSCSIPage::BuildMap()
{
	int i;
	int nID;
	int nInit;
	int nHD;
	BOOL bMO;
	BOOL bCD;

	ASSERT(this);

	// Inicializacion
	nHD = 0;
	bMO = FALSE;
	bCD = FALSE;

	// ・ｽf・ｽB・ｽX・ｽN・ｽ・ｽ・ｽ・ｽAceptar
	switch (m_nDrives) {
		// 0・ｽ・ｽ
		case 0:
			break;

		// 1・ｽ・ｽ
		case 1:
			// Prioridad MO・ｽ・ｽ・ｽAPrioridad HD・ｽ・ｽ・ｽﾅ包ｿｽ・ｽ・ｽ・ｽ・ｽ
			if (m_bMOFirst) {
				bMO = TRUE;
			}
			else {
				nHD = 1;
			}
			break;

		// 2・ｽ・ｽ
		case 2:
			// Una unidad HD y una MO
			nHD = 1;
			bMO = TRUE;
			break;

		// 3・ｽ・ｽ
		case 3:
			// Una unidad HD, una MO y una CD
			nHD = 1;
			bMO = TRUE;
			bCD = TRUE;
			break;

		// 4 o mas unidades
		default:
			ASSERT(m_nDrives <= 7);
			nHD= m_nDrives - 2;
			bMO = TRUE;
			bCD = TRUE;
			break;
	}

	// ・ｽI・ｽ[・ｽ・ｽLimpiar
	for (i=0; i<8; i++) {
		m_DevMap[i] = DevNone;
	}

	// Configurar iniciador primero
	ASSERT(m_pSCSI);
	nInit = m_pSCSI->GetSCSIID();
	ASSERT((nInit >= 0) && (nInit <= 7));
	m_DevMap[nInit] = DevInit;

	// Configuracion MO (solo si flag de prioridad activo)
	if (bMO && m_bMOFirst) {
		for (nID=0; nID<SCSI::DeviceMax; nID++) {
			if (m_DevMap[nID] == DevNone) {
				m_DevMap[nID] = DevMO;
				bMO = FALSE;
				break;
			}
		}
	}

	// Configuracion HD
	for (i=0; i<nHD; i++) {
		for (nID=0; nID<SCSI::DeviceMax; nID++) {
			if (m_DevMap[nID] == DevNone) {
				m_DevMap[nID] = DevSCSI;
				break;
			}
		}
	}

	// Configuracion MO
	if (bMO) {
		for (nID=0; nID<SCSI::DeviceMax; nID++) {
			if (m_DevMap[nID] == DevNone) {
				m_DevMap[nID] = DevMO;
				break;
			}
		}
	}

	// Configuracion CD (fijo ID=6, o 7 si esta en uso)
	if (bCD) {
		if (m_DevMap[6] == DevNone) {
			m_DevMap[6] = DevCD;
		}
		else {
			ASSERT(m_DevMap[7] == DevNone);
			m_DevMap[7] = DevCD;
		}
	}
}

//---------------------------------------------------------------------------
//
//	SCSI・ｽn・ｽ[・ｽh・ｽf・ｽB・ｽX・ｽNCapacidadVerificacion
//	* Devuelve 0 en caso de error de dispositivo
//
//---------------------------------------------------------------------------
int FASTCALL CSCSIPage::CheckSCSI(int nDrive)
{
	Fileio fio;
	DWORD dwSize;

	ASSERT(this);
	ASSERT((nDrive >= 0) && (nDrive <= SCSI::HDMax));

	// Bloquear
	::LockVM();

	// Abrir archivo
	if (!fio.Open(m_szFile[nDrive], Fileio::ReadOnly)) {
		// Error, devuelve 0
		fio.Close();
		::UnlockVM();
		return 0;
	}

	// Capacidad・ｽ謫ｾ
	dwSize = fio.GetFileSize();

	// Desbloquear
	fio.Close();
	::UnlockVM();

	// ・ｽt・ｽ@・ｽC・ｽ・ｽ・ｽT・ｽC・ｽY・ｽ・ｽVerificacion(512・ｽo・ｽC・ｽg・ｽP・ｽ・ｽ)
	if ((dwSize & 0x1ff) != 0) {
		return 0;
	}

	// ・ｽt・ｽ@・ｽC・ｽ・ｽ・ｽT・ｽC・ｽY・ｽ・ｽVerificacion(10MB・ｽﾈ擾ｿｽ)
	if (dwSize < 10 * 0x400 * 0x400) {
		return 0;
	}

	// ・ｽt・ｽ@・ｽC・ｽ・ｽ・ｽT・ｽC・ｽY・ｽ・ｽVerificacion(4095MB・ｽﾈ会ｿｽ)
	if (dwSize > 0xfff00000) {
		return 0;
	}

	// Devolver tama?o
	dwSize >>= 20;
	return dwSize;
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CSCSIPage::EnableControls(BOOL bEnable, BOOL bDrive)
{
	int i;
	CWnd *pWnd;
	CListCtrl *pListCtrl;

	ASSERT(this);

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽEMOVerificacion・ｽﾈ外・ｽﾌ全・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ・ｽﾝ抵ｿｽ
	for (i=0; ; i++) {
		// Obtener control
		if (!ControlTable[i]) {
			break;
		}
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);

		// ・ｽﾝ抵ｿｽ
		pWnd->EnableWindow(bDrive);
	}

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ・ｽﾝ抵ｿｽ
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_SCSI_LIST);
	ASSERT(pListCtrl);
	pListCtrl->EnableWindow(bEnable);

	// MOVerificacion・ｽ・ｽﾝ抵ｿｽ
	pWnd = GetDlgItem(IDC_SCSI_MOCHECK);
	ASSERT(pWnd);
	pWnd->EnableWindow(bEnable);
}

//---------------------------------------------------------------------------
//
//	Tabla de controles
//
//---------------------------------------------------------------------------
const UINT CSCSIPage::ControlTable[] = {
	IDC_SCSI_GROUP,
	IDC_SCSI_DRIVEL,
	IDC_SCSI_DRIVEE,
	IDC_SCSI_DRIVES,
	NULL
};

//===========================================================================
//
//	Pagina de puertos
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CPortPage::CPortPage()
{
	// Configurar siempre ID y Help
	m_dwID = MAKEID('P', 'O', 'R', 'T');
	m_nTemplate = IDD_PORTPAGE;
	m_uHelpID = IDC_PORT_HELP;
}

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CPortPage::OnInitDialog()
{
	int i;
	CComboBox *pComboBox;
	CString strText;
	CButton *pButton;
	CEdit *pEdit;

	// Clase base
	CConfigPage::OnInitDialog();

	// COMCuadro combinado
	pComboBox = (CComboBox*)GetDlgItem(IDC_PORT_COMC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	::GetMsg(IDS_PORT_NOASSIGN, strText);
	pComboBox->AddString(strText);
	for (i=1; i<=9; i++) {
		strText.Format(_T("COM%d"), i);
		pComboBox->AddString(strText);
	}
	pComboBox->SetCurSel(m_pConfig->port_com);

	// Log de recepcion
	pEdit = (CEdit*)GetDlgItem(IDC_PORT_RECVE);
	ASSERT(pEdit);
	pEdit->SetWindowText(m_pConfig->port_recvlog);

	// Forzar 38400bps
	pButton = (CButton*)GetDlgItem(IDC_PORT_BAUDRATE);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->port_384);

	// LPTCuadro combinado
	pComboBox = (CComboBox*)GetDlgItem(IDC_PORT_LPTC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	::GetMsg(IDS_PORT_NOASSIGN, strText);
	pComboBox->AddString(strText);
	for (i=1; i<=9; i++) {
		strText.Format(_T("LPT%d"), i);
		pComboBox->AddString(strText);
	}
	pComboBox->SetCurSel(m_pConfig->port_lpt);

	// Log de envio
	pEdit = (CEdit*)GetDlgItem(IDC_PORT_SENDE);
	ASSERT(pEdit);
	pEdit->SetWindowText(m_pConfig->port_sendlog);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CPortPage::OnOK()
{
	CComboBox *pComboBox;
	CEdit *pEdit;
	CButton *pButton;

	// COMCuadro combinado
	pComboBox = (CComboBox*)GetDlgItem(IDC_PORT_COMC);
	ASSERT(pComboBox);
	m_pConfig->port_com = pComboBox->GetCurSel();

	// Log de recepcion
	pEdit = (CEdit*)GetDlgItem(IDC_PORT_RECVE);
	ASSERT(pEdit);
	pEdit->GetWindowText(m_pConfig->port_recvlog, sizeof(m_pConfig->port_recvlog));

	// Forzar 38400bps
	pButton = (CButton*)GetDlgItem(IDC_PORT_BAUDRATE);
	ASSERT(pButton);
	m_pConfig->port_384 = pButton->GetCheck();

	// LPTCuadro combinado
	pComboBox = (CComboBox*)GetDlgItem(IDC_PORT_LPTC);
	ASSERT(pComboBox);
	m_pConfig->port_lpt = pComboBox->GetCurSel();

	// Log de envio
	pEdit = (CEdit*)GetDlgItem(IDC_PORT_SENDE);
	ASSERT(pEdit);
	pEdit->GetWindowText(m_pConfig->port_sendlog, sizeof(m_pConfig->port_sendlog));

	// Clase base
	CConfigPage::OnOK();
}

//===========================================================================
//
//	Pagina MIDI
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CMIDIPage::CMIDIPage()
{
	// Configurar siempre ID y Help
	m_dwID = MAKEID('M', 'I', 'D', 'I');
	m_nTemplate = IDD_MIDIPAGE;
	m_uHelpID = IDC_MIDI_HELP;

	// Objetos
	m_pMIDI = NULL;
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CMIDIPage, CConfigPage)
	ON_WM_VSCROLL()
	ON_BN_CLICKED(IDC_MIDI_BID0, OnBIDClick)
	ON_BN_CLICKED(IDC_MIDI_BID1, OnBIDClick)
	ON_BN_CLICKED(IDC_MIDI_BID2, OnBIDClick)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CMIDIPage::OnInitDialog()
{
	CButton *pButton;
	CComboBox *pComboBox;
	CSpinButtonCtrl *pSpin;
	CFrmWnd *pFrmWnd;
	CString strDesc;
	int nNum;
	int i;

	// Clase base
	CConfigPage::OnInitDialog();

	// Obtener componente MIDI
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pMIDI = pFrmWnd->GetMIDI();
	ASSERT(m_pMIDI);

	// Controles activados/desactivados
	m_bEnableCtrl = TRUE;
	EnableControls(FALSE);
	if (m_pConfig->midi_bid != 0) {
		EnableControls(TRUE);
	}

	// ・ｽ{・ｽ[・ｽhID
	pButton = (CButton*)GetDlgItem(IDC_MIDI_BID0 + m_pConfig->midi_bid);
	ASSERT(pButton);
	pButton->SetCheck(1);

	// Nivel de interrupcion
	pButton = (CButton*)GetDlgItem(IDC_MIDI_ILEVEL4 + m_pConfig->midi_ilevel);
	ASSERT(pButton);
	pButton->SetCheck(1);

	// Reinicio de sintetizador
	pButton = (CButton*)GetDlgItem(IDC_MIDI_RSTGM + m_pConfig->midi_reset);
	ASSERT(pButton);
	pButton->SetCheck(1);

	// Dispositivo (ENTRADA)
	pComboBox = (CComboBox*)GetDlgItem(IDC_MIDI_INC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	::GetMsg(IDS_MIDI_NOASSIGN, strDesc);
	pComboBox->AddString(strDesc);
	nNum = (int)m_pMIDI->GetInDevs();
	for (i=0; i<nNum; i++) {
		m_pMIDI->GetInDevDesc(i, strDesc);
		pComboBox->AddString(strDesc);
	}

	// Cuadro combinado・ｽ・ｽCursor・ｽ・ｽﾝ抵ｿｽ
	if (m_pConfig->midiin_device <= nNum) {
		pComboBox->SetCurSel(m_pConfig->midiin_device);
	}
	else {
		pComboBox->SetCurSel(0);
	}

	// Dispositivo (SALIDA)
	pComboBox = (CComboBox*)GetDlgItem(IDC_MIDI_OUTC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	::GetMsg(IDS_MIDI_NOASSIGN, strDesc);
	pComboBox->AddString(strDesc);
	nNum = (int)m_pMIDI->GetOutDevs();
	if (nNum >= 1) {
		::GetMsg(IDS_MIDI_MAPPER, strDesc);
		pComboBox->AddString(strDesc);
		for (i=0; i<nNum; i++) {
			m_pMIDI->GetOutDevDesc(i, strDesc);
			pComboBox->AddString(strDesc);
		}
	}

	// Cuadro combinado・ｽ・ｽCursor・ｽ・ｽﾝ抵ｿｽ
	if (m_pConfig->midiout_device < (nNum + 2)) {
		pComboBox->SetCurSel(m_pConfig->midiout_device);
	}
	else {
		pComboBox->SetCurSel(0);
	}

	// Retraso (ENTRADA)
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_MIDI_DLYIS);
	ASSERT(pSpin);
	pSpin->SetBase(10);
	pSpin->SetRange(0, 200);
	pSpin->SetPos(m_pConfig->midiin_delay);

	// Retraso (SALIDA)
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_MIDI_DLYOS);
	ASSERT(pSpin);
	pSpin->SetBase(10);
	pSpin->SetRange(20, 200);
	pSpin->SetPos(m_pConfig->midiout_delay);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CMIDIPage::OnOK()
{
	int i;
	CButton *pButton;
	CComboBox *pComboBox;
	CSpinButtonCtrl *pSpin;

	// ・ｽ{・ｽ[・ｽhID
	for (i=0; i<3; i++) {
		pButton = (CButton*)GetDlgItem(IDC_MIDI_BID0 + i);
		ASSERT(pButton);
		if (pButton->GetCheck() == 1) {
			m_pConfig->midi_bid = i;
			break;
		}
	}

	// Nivel de interrupcion
	for (i=0; i<2; i++) {
		pButton = (CButton*)GetDlgItem(IDC_MIDI_ILEVEL4 + i);
		ASSERT(pButton);
		if (pButton->GetCheck() == 1) {
			m_pConfig->midi_ilevel = i;
			break;
		}
	}

	// Reinicio de sintetizador
	for (i=0; i<4; i++) {
		pButton = (CButton*)GetDlgItem(IDC_MIDI_RSTGM + i);
		ASSERT(pButton);
		if (pButton->GetCheck() == 1) {
			m_pConfig->midi_reset = i;
			break;
		}
	}

	// Dispositivo (ENTRADA)
	pComboBox = (CComboBox*)GetDlgItem(IDC_MIDI_INC);
	ASSERT(pComboBox);
	m_pConfig->midiin_device = pComboBox->GetCurSel();

	// Dispositivo (SALIDA)
	pComboBox = (CComboBox*)GetDlgItem(IDC_MIDI_OUTC);
	ASSERT(pComboBox);
	m_pConfig->midiout_device = pComboBox->GetCurSel();

	// Retraso (ENTRADA)
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_MIDI_DLYIS);
	ASSERT(pSpin);
	m_pConfig->midiin_delay = LOWORD(pSpin->GetPos());

	// Retraso (SALIDA)
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_MIDI_DLYOS);
	ASSERT(pSpin);
	m_pConfig->midiout_delay = LOWORD(pSpin->GetPos());

	// Clase base
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CMIDIPage::OnCancel()
{
	// Restaurar retraso MIDI (IN)
	m_pMIDI->SetInDelay(m_pConfig->midiin_delay);

	// Restaurar retraso MIDI (OUT)
	m_pMIDI->SetOutDelay(m_pConfig->midiout_delay);

	// Clase base
	CConfigPage::OnCancel();
}

//---------------------------------------------------------------------------
//
//	Desplazamiento vertical
//
//---------------------------------------------------------------------------
void CMIDIPage::OnVScroll(UINT /*nSBCode*/, UINT nPos, CScrollBar *pBar)
{
	CSpinButtonCtrl *pSpin;

	// IN
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_MIDI_DLYIS);
	ASSERT(pSpin);
	if ((CWnd*)pSpin == (CWnd*)pBar) {
		m_pMIDI->SetInDelay(nPos);
	}

	// OUT
	pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_MIDI_DLYOS);
	ASSERT(pSpin);
	if ((CWnd*)pSpin == (CWnd*)pBar) {
		m_pMIDI->SetOutDelay(nPos);
	}
}

//---------------------------------------------------------------------------
//
//	Clic en ID de placa
//
//---------------------------------------------------------------------------
void CMIDIPage::OnBIDClick()
{
	CButton *pButton;

	// Obtener control "Sin ID" de placa
	pButton = (CButton*)GetDlgItem(IDC_MIDI_BID0);
	ASSERT(pButton);

	// Verificacion・ｽ・ｽ・ｽﾂゑｿｽ・ｽﾄゑｿｽ・ｽ驍ｩ・ｽﾇゑｿｽ・ｽ・ｽ・ｽﾅ抵ｿｽ・ｽﾗゑｿｽ
	if (pButton->GetCheck() == 1) {
		EnableControls(FALSE);
	}
	else {
		EnableControls(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CMIDIPage::EnableControls(BOOL bEnable) 
{
	int i;
	CWnd *pWnd;

	ASSERT(this);

	// Verificacion de flags
	if (m_bEnableCtrl == bEnable) {
		return;
	}
	m_bEnableCtrl = bEnable;

	// Configurar todos los controles excepto ID de placa y Ayuda
	for(i=0; ; i++) {
		// FinVerificacion
		if (ControlTable[i] == NULL) {
			break;
		}

		// Obtener control
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);
		pWnd->EnableWindow(bEnable);
	}
}

//---------------------------------------------------------------------------
//
//	Tabla de IDs de controles
//
//---------------------------------------------------------------------------
const UINT CMIDIPage::ControlTable[] = {
	IDC_MIDI_ILEVELG,
	IDC_MIDI_ILEVEL4,
	IDC_MIDI_ILEVEL2,
	IDC_MIDI_RSTG,
	IDC_MIDI_RSTGM,
	IDC_MIDI_RSTGS,
	IDC_MIDI_RSTXG,
	IDC_MIDI_RSTLA,
	IDC_MIDI_DEVG,
	IDC_MIDI_INS,
	IDC_MIDI_INC,
	IDC_MIDI_DLYIL,
	IDC_MIDI_DLYIE,
	IDC_MIDI_DLYIS,
	IDC_MIDI_DLYIG,
	IDC_MIDI_OUTS,
	IDC_MIDI_OUTC,
	IDC_MIDI_DLYOL,
	IDC_MIDI_DLYOE,
	IDC_MIDI_DLYOS,
	IDC_MIDI_DLYOG,
	NULL
};

//===========================================================================
//
//	Pagina de modificaciones
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CAlterPage::CAlterPage()
{
	// Configurar siempre ID y Help
	m_dwID = MAKEID('A', 'L', 'T', ' ');
	m_nTemplate = IDD_ALTERPAGE;
	m_uHelpID = IDC_ALTER_HELP;

	// Inicializacion
	m_bInit = FALSE;
	m_bParity = FALSE;
}

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CAlterPage::OnInitDialog()
{
	// Clase base
	CConfigPage::OnInitDialog();

	// Inicializacion・ｽﾏみ、・ｽp・ｽ・ｽ・ｽe・ｽB・ｽt・ｽ・ｽ・ｽO・ｽ・ｽ・ｽ謫ｾ・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽ
	m_bInit = TRUE;
	m_bParity = m_pConfig->sasi_parity;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Moverse de pagina
//
//---------------------------------------------------------------------------
BOOL CAlterPage::OnKillActive()
{
	CButton *pButton;

	ASSERT(this);

	// Verificacion・ｽ{・ｽb・ｽN・ｽX・ｽ・ｽ・ｽp・ｽ・ｽ・ｽe・ｽB・ｽt・ｽ・ｽ・ｽO・ｽﾉ費ｿｽ・ｽf・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	pButton = (CButton*)GetDlgItem(IDC_ALTER_PARITY);
	ASSERT(pButton);
	if (pButton->GetCheck() == 1) {
		m_bParity = TRUE;
	}
	else {
		m_bParity = FALSE;
	}

	// Clase base
	return CConfigPage::OnKillActive();
}

//---------------------------------------------------------------------------
//
//	Intercambio de datos
//
//---------------------------------------------------------------------------
void CAlterPage::DoDataExchange(CDataExchange *pDX)
{
	ASSERT(this);
	ASSERT(pDX);

	// Clase base
	CConfigPage::DoDataExchange(pDX);

	// Intercambio de datos
	DDX_Check(pDX, IDC_ALTER_SRAM, m_pConfig->sram_64k);
	DDX_Check(pDX, IDC_ALTER_SCC, m_pConfig->scc_clkup);
	DDX_Check(pDX, IDC_ALTER_POWERLED, m_pConfig->power_led);
	DDX_Check(pDX, IDC_ALTER_2DD, m_pConfig->dual_fdd);
	DDX_Check(pDX, IDC_ALTER_PARITY, m_pConfig->sasi_parity);
}

//---------------------------------------------------------------------------
//
//	SASI・ｽp・ｽ・ｽ・ｽe・ｽB・ｽ@・ｽ\Verificacion
//
//---------------------------------------------------------------------------
BOOL FASTCALL CAlterPage::HasParity(const Config *pConfig) const
{
	ASSERT(this);
	ASSERT(pConfig);

	// Inicializacion・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽﾈゑｿｽ・ｽ・ｽﾎ、・ｽ^・ｽ・ｽ・ｽ黷ｽConfig・ｽf・ｽ[・ｽ^・ｽ・ｽ・ｽ・ｽ
	if (!m_bInit) {
		return pConfig->sasi_parity;
	}

	// Inicializacion・ｽﾏみなゑｿｽA・ｽﾅ新・ｽ・ｽEditar・ｽ・ｽ・ｽﾊゑｿｽm・ｽ轤ｹ・ｽ・ｽ
	return m_bParity;
}

//===========================================================================
//
//	Pagina de reanudacion (Resume)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CResumePage::CResumePage()
{
	// Configurar siempre ID y Help
	m_dwID = MAKEID('R', 'E', 'S', 'M');
	m_nTemplate = IDD_RESUMEPAGE;
	m_uHelpID = IDC_RESUME_HELP;
}

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CResumePage::OnInitDialog()
{
	// Clase base
	CConfigPage::OnInitDialog();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Intercambio de datos
//
//---------------------------------------------------------------------------
void CResumePage::DoDataExchange(CDataExchange *pDX)
{
	ASSERT(this);
	ASSERT(pDX);

	// Clase base
	CConfigPage::DoDataExchange(pDX);

	// Intercambio de datos
	DDX_Check(pDX, IDC_RESUME_FDC, m_pConfig->resume_fd);
	DDX_Check(pDX, IDC_RESUME_MOC, m_pConfig->resume_mo);
	DDX_Check(pDX, IDC_RESUME_CDC, m_pConfig->resume_cd);
	DDX_Check(pDX, IDC_RESUME_XM6C, m_pConfig->resume_state);
	DDX_Check(pDX, IDC_RESUME_SCREENC, m_pConfig->resume_screen);
	DDX_Check(pDX, IDC_RESUME_DIRC, m_pConfig->resume_dir);
}

//===========================================================================
//
//	Dialogo TrueKey
//
//===========================================================================
 
//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CTKeyDlg::CTKeyDlg(CWnd *pParent) : CDialog(IDD_KEYINDLG, pParent)
{
	CFrmWnd *pFrmWnd;

	// Ingles・ｽﾂ具ｿｽ・ｽﾖの対会ｿｽ
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_KEYINDLG);
		m_nIDHelp = IDD_US_KEYINDLG;
	}

	// Obtener TrueKey
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pTKey = pFrmWnd->GetTKey();
	ASSERT(m_pTKey);
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CTKeyDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_GETDLGCODE()
	ON_WM_TIMER()
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	・ｽ_・ｽC・ｽA・ｽ・ｽ・ｽOInicializacion
//
//---------------------------------------------------------------------------
BOOL CTKeyDlg::OnInitDialog()
{
	CString strText;
	CStatic *pStatic;
	LPCSTR lpszKey;

	// Clase base
	CDialog::OnInitDialog();

	// Desactivar IME
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// ・ｽK・ｽC・ｽh・ｽ・ｽ`・ｽ・ｽProcesamiento
	pStatic = (CStatic*)GetDlgItem(IDC_KEYIN_LABEL);
	ASSERT(pStatic);
	pStatic->GetWindowText(strText);
	m_strGuide.Format(strText, m_nTarget);
	pStatic->GetWindowRect(&m_rectGuide);
	ScreenToClient(&m_rectGuide);
	pStatic->DestroyWindow();

	// Asignacion・ｽ・ｽ`・ｽ・ｽProcesamiento
	pStatic = (CStatic*)GetDlgItem(IDC_KEYIN_STATIC);
	ASSERT(pStatic);
	pStatic->GetWindowText(m_strAssign);
	pStatic->GetWindowRect(&m_rectAssign);
	ScreenToClient(&m_rectAssign);
	pStatic->DestroyWindow();

	// ・ｽL・ｽ[・ｽ・ｽ`・ｽ・ｽProcesamiento
	pStatic = (CStatic*)GetDlgItem(IDC_KEYIN_KEY);
	ASSERT(pStatic);
	pStatic->GetWindowText(m_strKey);
	if (m_nKey != 0) {
		lpszKey = m_pTKey->GetKeyID(m_nKey);
		if (lpszKey) {
			m_strKey = lpszKey;
		}
	}
	pStatic->GetWindowRect(&m_rectKey);
	ScreenToClient(&m_rectKey);
	pStatic->DestroyWindow();

	// Teclado・ｽ・ｽﾔゑｿｽ・ｽ謫ｾ
	::GetKeyboardState(m_KeyState);

	// Temporizador・ｽ・ｽ・ｽﾍゑｿｽ
	m_nTimerID = SetTimer(IDD_KEYINDLG, 100, NULL);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	OK
//
//---------------------------------------------------------------------------
void CTKeyDlg::OnOK()
{
	// [CR]・ｽﾉゑｿｽ・ｽFin・ｽ・ｽ}・ｽ・ｽ
}

//---------------------------------------------------------------------------
//
//	Cancelar
//
//---------------------------------------------------------------------------
void CTKeyDlg::OnCancel()
{
	// Temporizador・ｽ・ｽ~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// Clase base
	CDialog::OnCancel();
}

//---------------------------------------------------------------------------
//
//	Dibujar
//
//---------------------------------------------------------------------------
void CTKeyDlg::OnPaint()
{
	CPaintDC dc(this);
	CDC mDC;
	CRect rect;
	CBitmap Bitmap;
	CBitmap *pBitmap;

	// Crear DC en memoria
	VERIFY(mDC.CreateCompatibleDC(&dc));

	// Crear mapa de bits compatible
	GetClientRect(&rect);
	VERIFY(Bitmap.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height()));
	pBitmap = mDC.SelectObject(&Bitmap);
	ASSERT(pBitmap);

	// Limpiar fondo
	mDC.FillSolidRect(&rect, ::GetSysColor(COLOR_3DFACE));

	// Dibujar
	OnDraw(&mDC);

	// BitBlt
	VERIFY(dc.BitBlt(0, 0, rect.Width(), rect.Height(), &mDC, 0, 0, SRCCOPY));

	// ・ｽr・ｽb・ｽg・ｽ}・ｽb・ｽvFin
	VERIFY(mDC.SelectObject(pBitmap));
	VERIFY(Bitmap.DeleteObject());

	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽDCFin
	VERIFY(mDC.DeleteDC());
}

//---------------------------------------------------------------------------
//
//	Dibujar・ｽ・ｽ・ｽC・ｽ・ｽ
//
//---------------------------------------------------------------------------
void FASTCALL CTKeyDlg::OnDraw(CDC *pDC)
{
	HFONT hFont;
	CFont *pFont;
	CFont *pDefFont;

	ASSERT(this);
	ASSERT(pDC);

	// Configuracion de colores
	pDC->SetTextColor(::GetSysColor(COLOR_BTNTEXT));
	pDC->SetBkColor(::GetSysColor(COLOR_3DFACE));

	// Configuracion de fuentes
	hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	ASSERT(hFont);
	pFont = CFont::FromHandle(hFont);
	pDefFont = pDC->SelectObject(pFont);
	ASSERT(pDefFont);

	// Mostrar
	pDC->DrawText(m_strGuide, m_rectGuide,
					DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
	pDC->DrawText(m_strAssign, m_rectAssign,
					DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
	pDC->DrawText(m_strKey, m_rectKey,
					DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);

	// Restaurar fuente(Objetos・ｽ・ｽEliminar・ｽ・ｽ・ｽﾈゑｿｽ・ｽﾄよい)
	pDC->SelectObject(pDefFont);
}

//---------------------------------------------------------------------------
//
//	・ｽ_・ｽC・ｽA・ｽ・ｽ・ｽOObtener codigo
//
//---------------------------------------------------------------------------
UINT CTKeyDlg::OnGetDlgCode()
{
	// Habilitar la recepcion de mensajes de teclado
	return DLGC_WANTMESSAGE;
}

//---------------------------------------------------------------------------
//
//	Temporizador
//
//---------------------------------------------------------------------------
#if _MFC_VER >= 0x700
void CTKeyDlg::OnTimer(UINT_PTR nID)
#else
void CTKeyDlg::OnTimer(UINT nID)
#endif
{
	BYTE state[0x100];
	int nKey;
	int nTarget;
	LPCTSTR lpszKey;

	// IDVerificacion
	if (m_nTimerID != nID) {
		return;
	}

	// Obtener tecla
	GetKeyboardState(state);

	// Si coincide, no hay cambio
	if (memcmp(state, m_KeyState, sizeof(state)) == 0) {
		return;
	}

	// Hacer copia de respaldo del objetivo
	nTarget = m_nKey;

	// Buscar cambios, excluyendo LBUTTON y RBUTTON
	for (nKey=3; nKey<0x100; nKey++) {
		// No pulsado anteriormente
		if ((m_KeyState[nKey] & 0x80) == 0) {
			// Pulsado esta vez
			if (state[nKey] & 0x80) {
				// ・ｽ^・ｽ[・ｽQ・ｽb・ｽg・ｽﾝ抵ｿｽ
				nTarget = nKey;
				break;
			}
		}
	}

	// ・ｽX・ｽe・ｽ[・ｽgActualizacion
	memcpy(m_KeyState, state, sizeof(state));

	// Si el objetivo no ha cambiado, no hacer nada
	if (m_nKey == nTarget) {
		return;
	}

	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ謫ｾ
	lpszKey = m_pTKey->GetKeyID(nTarget);
	if (lpszKey) {
		// Hay cadena de tecla, nueva configuracion
		m_nKey = nTarget;

		// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾉ設抵ｿｽA・ｽ・ｽDibujar
		m_strKey = lpszKey;
		Invalidate(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Clic derecho
//
//---------------------------------------------------------------------------
void CTKeyDlg::OnRButtonDown(UINT /*nFlags*/, CPoint /*point*/)
{
	// Temporizador・ｽ・ｽ~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// ・ｽ_・ｽC・ｽA・ｽ・ｽ・ｽOFin
	EndDialog(IDOK);
}

//===========================================================================
//
//	Pagina TrueKey
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CTKeyPage::CTKeyPage()
{
	CFrmWnd *pFrmWnd;

	// Configurar siempre ID y Help
	m_dwID = MAKEID('T', 'K', 'E', 'Y');
	m_nTemplate = IDD_TKEYPAGE;
	m_uHelpID = IDC_TKEY_HELP;

	// Obtener entrada
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pInput = pFrmWnd->GetInput();
	ASSERT(m_pInput);

	// Obtener TrueKey
	m_pTKey = pFrmWnd->GetTKey();
	ASSERT(m_pTKey);
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CTKeyPage, CConfigPage)
	ON_BN_CLICKED(IDC_TKEY_WINC, OnSelChange)
	ON_CBN_SELCHANGE(IDC_TKEY_COMC, OnSelChange)
	ON_NOTIFY(NM_CLICK, IDC_TKEY_LIST, OnClick)
	ON_NOTIFY(NM_RCLICK, IDC_TKEY_LIST, OnRClick)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CTKeyPage::OnInitDialog()
{
	CComboBox *pComboBox;
	CButton *pButton;
	CString strText;
	int i;
	CDC *pDC;
	TEXTMETRIC tm;
	LONG cx;
	CListCtrl *pListCtrl;
	int nItem;
	LPCTSTR lpszKey;

	// Clase base
	CConfigPage::OnInitDialog();

	// Cuadro combinado de puertos
	pComboBox = (CComboBox*)GetDlgItem(IDC_TKEY_COMC);
	ASSERT(pComboBox);
	pComboBox->ResetContent();
	::GetMsg(IDS_TKEY_NOASSIGN, strText);
	pComboBox->AddString(strText);
	for (i=1; i<=9; i++) {
		strText.Format(_T("COM%d"), i);
		pComboBox->AddString(strText);
	}
	pComboBox->SetCurSel(m_pConfig->tkey_com);

	// Inversion RTS
	pButton = (CButton*)GetDlgItem(IDC_TKEY_RTS);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->tkey_rts);

	// Modo
	pButton = (CButton*)GetDlgItem(IDC_TKEY_VMC);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->tkey_mode & 1);
	pButton = (CButton*)GetDlgItem(IDC_TKEY_WINC);
	ASSERT(pButton);
	pButton->SetCheck(m_pConfig->tkey_mode >> 1);

	// Obtener metricas de texto
	pDC = new CClientDC(this);
	::GetTextMetrics(pDC->m_hDC, &tm);
	delete pDC;
	cx = tm.tmAveCharWidth;

	// Columnas del control de lista
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_TKEY_LIST);
	ASSERT(pListCtrl);
	if (::IsJapanese()) {
		// Japones
		::GetMsg(IDS_TKEY_NO, strText);
		pListCtrl->InsertColumn(0, strText, LVCFMT_LEFT, cx * 4, 0);
		::GetMsg(IDS_TKEY_KEYTOP, strText);
		pListCtrl->InsertColumn(1, strText, LVCFMT_LEFT, cx * 10, 1);
		::GetMsg(IDS_TKEY_VK, strText);
		pListCtrl->InsertColumn(2, strText, LVCFMT_LEFT, cx * 22, 2);
	}
	else {
		// Ingles
		::GetMsg(IDS_TKEY_NO, strText);
		pListCtrl->InsertColumn(0, strText, LVCFMT_LEFT, cx * 5, 0);
		::GetMsg(IDS_TKEY_KEYTOP, strText);
		pListCtrl->InsertColumn(1, strText, LVCFMT_LEFT, cx * 12, 1);
		::GetMsg(IDS_TKEY_VK, strText);
		pListCtrl->InsertColumn(2, strText, LVCFMT_LEFT, cx * 18, 2);
	}

	// Crear items (la informacion del lado X68000 es fija independientemente del mapeo)
	pListCtrl->DeleteAllItems();
	nItem = 0;
	for (i=0; i<=0x73; i++) {
		// Obtener el nombre de la tecla desde CKeyDispWnd
		lpszKey = m_pInput->GetKeyName(i);
		if (lpszKey) {
			// Esta tecla es valida
			strText.Format(_T("%02X"), i);
			pListCtrl->InsertItem(nItem, strText);
			pListCtrl->SetItemText(nItem, 1, lpszKey);
			pListCtrl->SetItemData(nItem, (DWORD)i);
			nItem++;
		}
	}

	// Opcion de fila completa para el control de lista (COMCTL32.DLL v4.71+)
	pListCtrl->SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

	// Obtener mapeo VK
	m_pTKey->GetKeyMap(m_nKey);

	// ・ｽ・ｽ・ｽX・ｽg・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽActualizacion
	UpdateReport();

	// Configuracion de activacion de controles
	if (m_pConfig->tkey_com == 0) {
		m_bEnableCtrl = TRUE;
		EnableControls(FALSE);
	}
	else {
		m_bEnableCtrl = FALSE;
		EnableControls(TRUE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aceptar
//
//---------------------------------------------------------------------------
void CTKeyPage::OnOK()
{
	CComboBox *pComboBox;
	CButton *pButton;

	// Cuadro combinado de puertos
	pComboBox = (CComboBox*)GetDlgItem(IDC_TKEY_COMC);
	ASSERT(pComboBox);
	m_pConfig->tkey_com = pComboBox->GetCurSel();

	// Inversion RTS
	pButton = (CButton*)GetDlgItem(IDC_TKEY_RTS);
	ASSERT(pButton);
	m_pConfig->tkey_rts = pButton->GetCheck();

	// Asignacion
	m_pConfig->tkey_mode = 0;
	pButton = (CButton*)GetDlgItem(IDC_TKEY_VMC);
	ASSERT(pButton);
	if (pButton->GetCheck() == 1) {
		m_pConfig->tkey_mode |= 1;
	}
	pButton = (CButton*)GetDlgItem(IDC_TKEY_WINC);
	ASSERT(pButton);
	if (pButton->GetCheck() == 1) {
		m_pConfig->tkey_mode |= 2;
	}

	// Configurar mapa de teclas
	m_pTKey->SetKeyMap(m_nKey);

	// Clase base
	CConfigPage::OnOK();
}

//---------------------------------------------------------------------------
//
//	Cambio en el cuadro combinado
//
//---------------------------------------------------------------------------
void CTKeyPage::OnSelChange()
{
	CComboBox *pComboBox;

	pComboBox = (CComboBox*)GetDlgItem(IDC_TKEY_COMC);
	ASSERT(pComboBox);

	// Controles activados/desactivados
	if (pComboBox->GetCurSel() > 0) {
		EnableControls(TRUE);
	}
	else {
		EnableControls(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Clic en item
//
//---------------------------------------------------------------------------
void CTKeyPage::OnClick(NMHDR* /*pNMHDR*/, LRESULT* /*pResult*/)
{
	CListCtrl *pListCtrl;
	int nItem;
	int nCount;
	int nKey;
	CTKeyDlg dlg(this);

	// ・ｽ・ｽ・ｽX・ｽgObtener control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_TKEY_LIST);
	ASSERT(pListCtrl);

	// Obtener conteo
	nCount = pListCtrl->GetItemCount();

	// Obtener el indice seleccionado
	for (nItem=0; nItem<nCount; nItem++) {
		if (pListCtrl->GetItemState(nItem, LVIS_SELECTED)) {
			break;
		}
	}
	if (nItem >= nCount) {
		return;
	}

	// Obtener los datos apuntados por ese indice(1・ｽ`0x73)
	nKey = (int)pListCtrl->GetItemData(nItem);
	ASSERT((nKey >= 1) && (nKey <= 0x73));

	// Iniciar configuracion
	dlg.m_nTarget = nKey;
	dlg.m_nKey = m_nKey[nKey - 1];

	// Ejecutar dialogo
	if (dlg.DoModal() != IDOK) {
		return;
	}

	// Configurar el mapa de teclas
	m_nKey[nKey - 1] = dlg.m_nKey;

	// Mostrar・ｽ・ｽActualizacion
	UpdateReport();
}

//---------------------------------------------------------------------------
//
//	Clic derecho en item
//
//---------------------------------------------------------------------------
void CTKeyPage::OnRClick(NMHDR* /*pNMHDR*/, LRESULT* /*pResult*/)
{
	CString strText;
	CString strMsg;
	CListCtrl *pListCtrl;
	int nItem;
	int nCount;
	int nKey;

	// ・ｽ・ｽ・ｽX・ｽgObtener control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_TKEY_LIST);
	ASSERT(pListCtrl);

	// Obtener conteo
	nCount = pListCtrl->GetItemCount();

	// Obtener el indice seleccionado
	for (nItem=0; nItem<nCount; nItem++) {
		if (pListCtrl->GetItemState(nItem, LVIS_SELECTED)) {
			break;
		}
	}
	if (nItem >= nCount) {
		return;
	}

	// Obtener los datos apuntados por ese indice(1・ｽ`0x73)
	nKey = (int)pListCtrl->GetItemData(nItem);
	ASSERT((nKey >= 1) && (nKey <= 0x73));

	// ・ｽ・ｽ・ｽﾅゑｿｽEliminar・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽ・ｽNo hacer nada
	if (m_nKey[nKey - 1] == 0) {
		return;
	}

	// ・ｽ・ｽ・ｽb・ｽZ・ｽ[・ｽW・ｽ{・ｽb・ｽN・ｽX・ｽﾅ、Eliminar・ｽﾌ有・ｽ・ｽ・ｽ・ｽVerificacion
	::GetMsg(IDS_KBD_DELMSG, strText);
	strMsg.Format(strText, nKey, m_pTKey->GetKeyID(m_nKey[nKey - 1]));
	::GetMsg(IDS_KBD_DELTITLE, strText);
	if (MessageBox(strMsg, strText, MB_ICONQUESTION | MB_YESNO) != IDYES) {
		return;
	}

	// ・ｽ・ｽ・ｽ・ｽ
	m_nKey[nKey - 1] = 0;

	// Mostrar・ｽ・ｽActualizacion
	UpdateReport();
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽ|・ｽ[・ｽgActualizacion
//
//---------------------------------------------------------------------------
void FASTCALL CTKeyPage::UpdateReport()
{
	CListCtrl *pListCtrl;
	LPCTSTR lpszKey;
	int nItem;
	int nKey;
	int nVK;
	CString strNext;
	CString strPrev;

	ASSERT(this);

	// Obtener control
	pListCtrl = (CListCtrl*)GetDlgItem(IDC_TKEY_LIST);
	ASSERT(pListCtrl);

	// Fila del control de lista
	nItem = 0;
	for (nKey=1; nKey<=0x73; nKey++) {
		// Obtener el nombre de la tecla desde CKeyDispWnd
		lpszKey = m_pInput->GetKeyName(nKey);
		if (lpszKey) {
			// ・ｽL・ｽ・ｽ・ｽﾈキ・ｽ[・ｽ・ｽ・ｽ・ｽ・ｽ・ｽBInicializacion
			strNext.Empty();

			// VKAsignacion・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾎ、Obtener nombre
			nVK = m_nKey[nKey - 1];
			if (nVK != 0) {
				lpszKey = m_pTKey->GetKeyID(nVK);
				strNext = lpszKey;
			}

			// Sobreescribir si es diferente
			strPrev = pListCtrl->GetItemText(nItem, 2);
			if (strPrev != strNext) {
				pListCtrl->SetItemText(nItem, 2, strNext);
			}

			// Al siguiente item
			nItem++;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Cambio de estado de los controles
//
//---------------------------------------------------------------------------
void FASTCALL CTKeyPage::EnableControls(BOOL bEnable) 
{
	int i;
	CWnd *pWnd;
	CButton *pButton;
	BOOL bCheck;

	ASSERT(this);

	// WindowsVerificacion・ｽ{・ｽb・ｽN・ｽX・ｽ謫ｾ
	pButton = (CButton*)GetDlgItem(IDC_TKEY_WINC);
	ASSERT(pButton);
	bCheck = FALSE;
	if (pButton->GetCheck() != 0) {
		bCheck = TRUE;
	}

	// Verificacion de flags
	if (m_bEnableCtrl == bEnable) {
		// Retornar solo en caso de FALSE -> FALSE
		if (!m_bEnableCtrl) {
			return;
		}
	}
	m_bEnableCtrl = bEnable;

	// Configurar todos los controles excepto Dispositivo y Ayuda
	for(i=0; ; i++) {
		// FinVerificacion
		if (ControlTable[i] == NULL) {
			break;
		}

		// Obtener control
		pWnd = GetDlgItem(ControlTable[i]);
		ASSERT(pWnd);

		// ControlTable[i]・ｽ・ｽIDC_TKEY_MAPG, IDC_TKEY_LIST・ｽﾍ難ｿｽ・ｽ・ｽ
		switch (ControlTable[i]) {
			// En cuanto a los controles de mapeo de teclas de Windows
			case IDC_TKEY_MAPG:
			case IDC_TKEY_LIST:
				// Solo si bEnable y Windows activo
				if (bEnable && bCheck) {
					pWnd->EnableWindow(TRUE);
				}
				else {
					pWnd->EnableWindow(FALSE);
				}
				break;

			// Otros・ｽ・ｽbEnable・ｽﾉ従・ｽ・ｽ
			default:
				pWnd->EnableWindow(bEnable);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Tabla de IDs de controles
//
//---------------------------------------------------------------------------
const UINT CTKeyPage::ControlTable[] = {
	IDC_TKEY_RTS,
	IDC_TKEY_ASSIGNG,
	IDC_TKEY_VMC,
	IDC_TKEY_WINC,
	IDC_TKEY_MAPG,
	IDC_TKEY_LIST,
	NULL
};

//===========================================================================
//
//	Otros・ｽy・ｽ[・ｽW
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CMiscPage::CMiscPage()
{
	// Configurar siempre ID y Help
	m_dwID = MAKEID('M', 'I', 'S', 'C');
	m_nTemplate = IDD_MISCPAGE;
	m_uHelpID = IDC_MISC_HELP;
}



BEGIN_MESSAGE_MAP(CMiscPage, CConfigPage)
	ON_BN_CLICKED(IDC_BUSCAR, OnBuscarFolder)
END_MESSAGE_MAP()


/* AQU? SE INICIALIZA EN EL CAMPO DE TEXTO LA RUTA DE GUARDADOS R?PIDOS */
BOOL CMiscPage::OnInitDialog()
{
	CComboBox *pCombo;
	CEdit *pEdit;	
	int nRendererID;
	int nRendererLblID;
	CConfigPage::OnInitDialog();
	CFrmWnd *pFrmWnd;
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	
	// Opciones
	pEdit = (CEdit*)GetDlgItem(IDC_EDIT1);
	pEdit->SetWindowTextA(pFrmWnd->RutaSaveStates);
		
	// Determinar IDs segun idioma
	if (::IsJapanese()) {
		nRendererID = IDC_MISC_RENDERER;
		nRendererLblID = IDC_MISC_RENDERERL;
	} else {
		nRendererID = IDC_US_MISC_RENDERER;
		nRendererLblID = IDC_US_MISC_RENDERERL;
	}

	// Inicializar combobox de renderizador
	pCombo = (CComboBox*)GetDlgItem(nRendererID);
	if (pCombo) {
		pCombo->AddString(_T("GDI"));
		pCombo->AddString(_T("DirectX 9"));
		pCombo->SetCurSel(m_pConfig->render_mode);
	}
		
	/*int msgboxID = MessageBox(
       m_pConfig->ruta_savestate,"saves",
         2 );	*/

	return TRUE;
}


/* AQU? SE ABRE EL DI?LOGO PARA SELECCIONAR UNA CARPETA DEL SISTEMA  */
void CMiscPage::OnBuscarFolder()
{ 	
	CEdit *pEdit;	
	
/*	CFolderPickerDialog folderPickerDialog("c:\\", OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_ENABLESIZING, this,
        sizeof(OPENFILENAME));
    CString folderPath;
			

    if (folderPickerDialog.DoModal() == IDOK)
    {
        POSITION pos = folderPickerDialog.GetStartPosition();
        while (pos)
        {
            folderPath = folderPickerDialog.GetNextPathName(pos);
        }
    }*/


   CFolderPickerDialog m_dlg;
   CString folderPath;
 
	m_dlg.m_ofn.lpstrTitle = _T("Buscar folder");
	m_dlg.m_ofn.lpstrInitialDir = _T("C:\\");
	if (m_dlg.DoModal() == IDOK) {
       folderPath = m_dlg.GetPathName();   // Use this to get the selected folder name 
                                         // after the dialog has closed
 
       // May need to add a '\' for usage in GUI and for later file saving, 
       // as there is no '\' on the returned name
       folderPath += _T("\\");
       UpdateData(FALSE);   // To show updated folder in GUI
 
       // Debug    
}


	pEdit = (CEdit*)GetDlgItem(IDC_EDIT1);
	pEdit->SetWindowTextA(folderPath);
}


/* AQU? SE GUARDA LA RUTA DE GUARDADO R?PIDO DE ESTADOS */
void CMiscPage::OnOK()
{
	CComboBox *pCombo;
	CEdit *pEdit;	
	CString  folderDestino;
	int nRendererID;
	pEdit = (CEdit*)GetDlgItem(IDC_EDIT1);
	pEdit->GetWindowTextA(folderDestino);
	_tcscpy(m_pConfig->ruta_savestate, folderDestino);

	// Determinar ID segun idioma
	if (::IsJapanese()) {
		nRendererID = IDC_MISC_RENDERER;
	} else {
		nRendererID = IDC_US_MISC_RENDERER;
	}

	// Guardar valor del renderizador
	pCombo = (CComboBox*)GetDlgItem(nRendererID);
	if (pCombo) {
		m_pConfig->render_mode = pCombo->GetCurSel();
	}

	CConfigPage::OnOK();
}


//---------------------------------------------------------------------------
//
 //	Intercambio de datos
 //
 //---------------------------------------------------------------------------
void CMiscPage::DoDataExchange(CDataExchange *pDX)
 {
 	int nVSyncID;
 
 	// Determinar ID de VSync segun idioma (los demas controles usan los mismos IDs en ambas versiones)
 	if (::IsJapanese()) {
 		nVSyncID = IDC_MISC_VSYNC;
 	} else {
 		nVSyncID = IDC_US_MISC_VSYNC;
 	}
 
 	CConfigPage::DoDataExchange(pDX);

 	// Intercambio de datos
 	DDX_Check(pDX, IDC_MISC_FDSPEED, m_pConfig->floppy_speed);
 	DDX_Check(pDX, IDC_MISC_FDLED, m_pConfig->floppy_led);
 	DDX_Check(pDX, IDC_MISC_POPUP, m_pConfig->popup_swnd);
 	DDX_Check(pDX, IDC_MISC_AUTOMOUSE, m_pConfig->auto_mouse);
 	DDX_Check(pDX, IDC_MISC_POWEROFF, m_pConfig->power_off);
 	DDX_Check(pDX, nVSyncID, m_pConfig->render_vsync);
 }

//===========================================================================
//
//	Configuracion・ｽv・ｽ・ｽ・ｽp・ｽe・ｽB・ｽV・ｽ[・ｽg
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CConfigSheet::CConfigSheet(CWnd *pParent) : CPropertySheet(IDS_OPTIONS, pParent)
{
	// En este punto los datos de configuracion son NULL
	m_pConfig = NULL;

	// Ingles・ｽﾂ具ｿｽ・ｽﾖの対会ｿｽ
	if (!::IsJapanese()) {
		::GetMsg(IDS_OPTIONS, m_strCaption);
	}

	// ApplyBotones・ｽ・ｽEliminar
	m_psh.dwFlags |= PSH_NOAPPLYNOW;

	// Memorizar ventana padre
	m_pFrmWnd = (CFrmWnd*)pParent;

	// Temporizador・ｽﾈゑｿｽ
	m_nTimerID = NULL;

	// ・ｽy・ｽ[・ｽWInicializacion
	m_Basic.Init(this);
	m_Sound.Init(this);
	m_Vol.Init(this);
	m_Kbd.Init(this);
	m_Mouse.Init(this);
	m_Joy.Init(this);
	m_SASI.Init(this);
	m_SxSI.Init(this);
	m_SCSI.Init(this);
	m_Port.Init(this);
	m_MIDI.Init(this);
	m_Alter.Init(this);
	m_Resume.Init(this);
	m_TKey.Init(this);
	m_Misc.Init(this);
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CConfigSheet, CPropertySheet)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_TIMER()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	・ｽy・ｽ[・ｽWBusqueda
//
//---------------------------------------------------------------------------
CConfigPage* FASTCALL CConfigSheet::SearchPage(DWORD dwID) const
{
	int nPage;
	int nCount;
	CConfigPage *pPage;

	ASSERT(this);
	ASSERT(dwID != 0);

	// Obtener numero de paginas
	nCount = GetPageCount();
	ASSERT(nCount >= 0);

	// ・ｽy・ｽ[・ｽWBucle
	for (nPage=0; nPage<nCount; nPage++) {
		// Obtener pagina
		pPage = (CConfigPage*)GetPage(nPage);
		ASSERT(pPage);

		// IDVerificacion
		if (pPage->GetID() == dwID) {
			return pPage;
		}
	}

	// No se encontro
	return NULL;
}

//---------------------------------------------------------------------------
//
//	・ｽE・ｽB・ｽ・ｽ・ｽh・ｽECrear
//
//---------------------------------------------------------------------------
int CConfigSheet::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	// Clase base
	if (CPropertySheet::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// Temporizador・ｽ・ｽ・ｽC・ｽ・ｽ・ｽX・ｽg・ｽ[・ｽ・ｽ
	m_nTimerID = SetTimer(IDM_OPTIONS, 100, NULL);

	return 0;
}

//---------------------------------------------------------------------------
//
//	・ｽE・ｽB・ｽ・ｽ・ｽh・ｽEEliminar
//
//---------------------------------------------------------------------------
void CConfigSheet::OnDestroy()
{
	// Temporizador・ｽ・ｽ~
	if (m_nTimerID) {
		KillTimer(m_nTimerID);
		m_nTimerID = NULL;
	}

	// Clase base
	CPropertySheet::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	Temporizador
//
//---------------------------------------------------------------------------
#if _MFC_VER >= 0x700
void CConfigSheet::OnTimer(UINT_PTR nID)
#else
void CConfigSheet::OnTimer(UINT nID)
#endif
{
	CInfo *pInfo;

	ASSERT(m_pFrmWnd);

	// IDVerificacion
	if (m_nTimerID != nID) {
		return;
	}

	// Temporizador・ｽ・ｽ~
	KillTimer(m_nTimerID);
	m_nTimerID = NULL;

	// Info・ｽ・ｽ・ｽ・ｽ・ｽﾝゑｿｽ・ｽ・ｽﾎ、Actualizacion
	pInfo = m_pFrmWnd->GetInfo();
	if (pInfo) {
		pInfo->UpdateStatus();
		pInfo->UpdateCaption();
		pInfo->UpdateInfo();
	}

	// Temporizador・ｽﾄ開(Mostrar・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ100ms・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ)
	m_nTimerID = SetTimer(IDM_OPTIONS, 100, NULL);
}

#endif	// _WIN32
