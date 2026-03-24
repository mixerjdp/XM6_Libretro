//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ・ｽo・ｽh・ｽD(ytanaka@ipc-tokai.or.jp)
//	[ MFC ・ｽT・ｽE・ｽ・ｽ・ｽh ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "opmif.h"
#include "opm.h"
#include "adpcm.h"
#include "scsi.h"
#include "schedule.h"
#include "config.h"
#include "mfc_frm.h"
#include "mfc_com.h"
#include "mfc_asm.h"
#include "mfc_snd.h"

//===========================================================================
//
//	・ｽT・ｽE・ｽ・ｽ・ｽh
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	・ｽR・ｽ・ｽ・ｽX・ｽg・ｽ・ｽ・ｽN・ｽ^
//
//---------------------------------------------------------------------------
CSound::CSound(CFrmWnd *pWnd) : CComponent(pWnd)
{
	// ・ｽR・ｽ・ｽ・ｽ|・ｽ[・ｽl・ｽ・ｽ・ｽg・ｽp・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽ^
	m_dwID = MAKEID('S', 'N', 'D', ' ');
	m_strDesc = _T("Sound Renderer");

	// ・ｽ・ｽ・ｽ[・ｽN・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ(・ｽﾝ抵ｿｽp・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽ^)
	m_uRate = 0;
	m_uTick = 90;
	m_uPoll = 7;
	m_uBufSize = 0;
	m_bPlay = FALSE;
	m_uCount = 0;
	m_dwWrite = 0;
	m_nMaster = 90;
	m_nFMVol = 54;
	m_nADPCMVol = 52;

	// ・ｽ・ｽ・ｽ[・ｽN・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ(DirectSound・ｽﾆオ・ｽu・ｽW・ｽF・ｽN・ｽg)
	m_lpDS = NULL;
	m_lpDSp = NULL;
	m_lpDSb = NULL;
	m_lpBuf = NULL;
	m_pScheduler = NULL;
	m_pOPM = NULL;
	m_pOPMIF = NULL;
	m_pADPCM = NULL;
	m_pSCSI = NULL;
	m_nDeviceNum = 0;
	m_nSelectDevice = 0;

	// ・ｽ・ｽ・ｽ[・ｽN・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ(WAV・ｽ^・ｽ・ｽ)
	m_pWav = NULL;
	m_nWav = 0;
	m_dwWav = 0;
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSound::Init()
{
	// ・ｽ・ｽ{・ｽN・ｽ・ｽ・ｽX
	if (!CComponent::Init()) {
		return FALSE;
	}

	// ・ｽX・ｽP・ｽW・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ謫ｾ
	m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pScheduler);

	// OPMIF・ｽ謫ｾ
	m_pOPMIF = (OPMIF*)::GetVM()->SearchDevice(MAKEID('O', 'P', 'M', ' '));
	ASSERT(m_pOPMIF);

	// ADPCM・ｽ謫ｾ
	m_pADPCM = (ADPCM*)::GetVM()->SearchDevice(MAKEID('A', 'P', 'C', 'M'));
	ASSERT(m_pADPCM);

	// SCSI・ｽ謫ｾ
	m_pSCSI = (SCSI*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(m_pSCSI);

	// ・ｽf・ｽo・ｽC・ｽX・ｽ・ｽ
	EnumDevice();

	// ・ｽ・ｽ・ｽ・ｽ・ｽﾅは擾ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾈゑｿｽ(ApplyCfg・ｽﾉ任・ｽ・ｽ・ｽ・ｽ)
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽT・ｽu
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSound::InitSub()
{
	PCMWAVEFORMAT pcmwf;
	DSBUFFERDESC dsbd;
	WAVEFORMATEX wfex;

	// rate==0・ｽﾈゑｿｽA・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾈゑｿｽ
	if (m_uRate == 0) {
		return TRUE;
	}

	ASSERT(!m_lpDS);
	ASSERT(!m_lpDSp);
	ASSERT(!m_lpDSb);
	ASSERT(!m_lpBuf);
	ASSERT(!m_pOPM);

	// ・ｽf・ｽo・ｽC・ｽX・ｽ・ｽ・ｽﾈゑｿｽ・ｽ・ｽ・ｽ0・ｽﾅ趣ｿｽ・ｽ・ｽ・ｽA・ｽ・ｽ・ｽ・ｽﾅゑｿｽ・ｽﾈゑｿｽ・ｽ・ｽ・ｽreturn
	if (m_nDeviceNum <= m_nSelectDevice) {
		if (m_nDeviceNum == 0) {
			return TRUE;
		}
		m_nSelectDevice = 0;
	}

	// DiectSound・ｽI・ｽu・ｽW・ｽF・ｽN・ｽg・ｽ・ｬ
	if (FAILED(DirectSoundCreate(m_lpGUID[m_nSelectDevice], &m_lpDS, NULL))) {
		// ・ｽf・ｽo・ｽC・ｽX・ｽﾍ使・ｽp・ｽ・ｽ
		return TRUE;
	}

	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽx・ｽ・ｽ・ｽ・ｽﾝ抵ｿｽ(・ｽD・ｽ諡ｦ・ｽ・ｽ)
	if (FAILED(m_lpDS->SetCooperativeLevel(m_pFrmWnd->m_hWnd, DSSCL_PRIORITY))) {
		return FALSE;
	}

	// ・ｽv・ｽ・ｽ・ｽC・ｽ}・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@・ｽ・ｽ・ｽ・ｬ
	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize = sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
	if (FAILED(m_lpDS->CreateSoundBuffer(&dsbd, &m_lpDSp, NULL))) {
		return FALSE;
	}

	// ・ｽv・ｽ・ｽ・ｽC・ｽ}・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@・ｽﾌフ・ｽH・ｽ[・ｽ}・ｽb・ｽg・ｽ・ｽ・ｽw・ｽ・ｽ
	memset(&wfex, 0, sizeof(wfex));
	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nChannels = 2;
	wfex.nSamplesPerSec = m_uRate;
	wfex.nBlockAlign = 4;
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;
	wfex.wBitsPerSample = 16;
	if (FAILED(m_lpDSp->SetFormat(&wfex))) {
		return FALSE;
	}

	// ・ｽZ・ｽJ・ｽ・ｽ・ｽ_・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@・ｽ・ｽ・ｽ・ｬ
	memset(&pcmwf, 0, sizeof(pcmwf));
	pcmwf.wf.wFormatTag = WAVE_FORMAT_PCM;
	pcmwf.wf.nChannels = 2;
	pcmwf.wf.nSamplesPerSec = m_uRate;
	pcmwf.wf.nBlockAlign = 4;
	pcmwf.wf.nAvgBytesPerSec = pcmwf.wf.nSamplesPerSec * pcmwf.wf.nBlockAlign;
	pcmwf.wBitsPerSample = 16;
	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize = sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_STICKYFOCUS | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME;
	dsbd.dwBufferBytes = (pcmwf.wf.nAvgBytesPerSec * m_uTick) / 1000;
	dsbd.dwBufferBytes = ((dsbd.dwBufferBytes + 7) >> 3) << 3;	// 8・ｽo・ｽC・ｽg・ｽ・ｽ・ｽE
	m_uBufSize = dsbd.dwBufferBytes;
	dsbd.lpwfxFormat = (LPWAVEFORMATEX)&pcmwf;
	if (FAILED(m_lpDS->CreateSoundBuffer(&dsbd, &m_lpDSb, NULL))) {
		return FALSE;
	}

	// ・ｽT・ｽE・ｽ・ｽ・ｽh・ｽo・ｽb・ｽt・ｽ@・ｽ・ｽ・ｽ・ｬ(・ｽZ・ｽJ・ｽ・ｽ・ｽ_・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@・ｽﾆ難ｿｽ・ｽ・ｽﾌ抵ｿｽ・ｽ・ｽ・ｽA1・ｽP・ｽ・ｽDWORD)
	try {
		m_lpBuf = new DWORD [ m_uBufSize / 2 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!m_lpBuf) {
		return FALSE;
	}
	memset(m_lpBuf, sizeof(DWORD) * (m_uBufSize / 2), m_uBufSize);

	// OPM・ｽf・ｽo・ｽC・ｽX(・ｽW・ｽ・ｽ)・ｽ・ｽ・ｽ・ｬ
	m_pOPM = new FM::OPM;
	m_pOPM->Init(4000000, m_uRate, true);
	m_pOPM->Reset();
	m_pOPM->SetVolume(m_nFMVol);

	// OPMIF・ｽﾖ通知
	m_pOPMIF->InitBuf(m_uRate);
	m_pOPMIF->SetEngine(m_pOPM);

	// ・ｽC・ｽl・ｽ[・ｽu・ｽ・ｽ・ｽﾈら演・ｽt・ｽJ・ｽn
	if (m_bEnable) {
		Play();
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽA・ｽb・ｽv
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Cleanup()
{
	// ・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽA・ｽb・ｽv・ｽT・ｽu
	CleanupSub();

	// ・ｽ・ｽ{・ｽN・ｽ・ｽ・ｽX
	CComponent::Cleanup();
}

//---------------------------------------------------------------------------
//
//	・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽA・ｽb・ｽv・ｽT・ｽu
//
//---------------------------------------------------------------------------
void FASTCALL CSound::CleanupSub()
{
	// ・ｽT・ｽE・ｽ・ｽ・ｽh・ｽ・ｽ~
	Stop();

	// OPMIF・ｽﾖ通知
	if (m_pOPMIF) {
		m_pOPMIF->SetEngine(NULL);
	}

	// OPM・ｽ・ｽ・ｽ・ｽ・ｽ
	if (m_pOPM) {
		delete m_pOPM;
		m_pOPM = NULL;
	}

	// ・ｽT・ｽE・ｽ・ｽ・ｽh・ｽ・ｬ・ｽo・ｽb・ｽt・ｽ@・ｽ・ｽ・ｽ・ｽ・ｽ
	if (m_lpBuf) {
		delete[] m_lpBuf;
		m_lpBuf = NULL;
	}

	// DirectSoundBuffer・ｽ・ｽ・ｽ・ｽ・ｽ
	if (m_lpDSb) {
		m_lpDSb->Release();
		m_lpDSb = NULL;
	}
	if (m_lpDSp) {
		m_lpDSp->Release();
		m_lpDSp = NULL;
	}

	// DirectSound・ｽ・ｽ・ｽ・ｽ・ｽ
	if (m_lpDS) {
		m_lpDS->Release();
		m_lpDS = NULL;
	}

	// uRate・ｽ・ｽ・ｽN・ｽ・ｽ・ｽA
	m_uRate = 0;
}

//---------------------------------------------------------------------------
//
//	・ｽﾝ抵ｿｽK・ｽp
//
//---------------------------------------------------------------------------
void FASTCALL CSound::ApplyCfg(const Config *pConfig)
{
	BOOL bFlag;

	ASSERT(this);
	ASSERT(pConfig);

	// ・ｽﾄ擾ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽ`・ｽF・ｽb・ｽN
	bFlag = FALSE;
	if (m_nSelectDevice != pConfig->sound_device) {
		bFlag = TRUE;
	}
	if (m_uRate != RateTable[pConfig->sample_rate]) {
		bFlag = TRUE;
	}
	if (m_uTick != (UINT)(pConfig->primary_buffer * 10)) {
		bFlag = TRUE;
	}

	// ・ｽﾄ擾ｿｽ・ｽ・ｽ・ｽ・ｽ
	if (bFlag) {
		CleanupSub();
		m_nSelectDevice = pConfig->sound_device;
		m_uRate = RateTable[pConfig->sample_rate];
		m_uTick = pConfig->primary_buffer * 10;

		// 62.5kHz・ｽﾌ場合・ｽﾍ、・ｽ・ｽx96kHz・ｽﾉセ・ｽb・ｽg・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽ(Prodigy7.1・ｽﾎ搾ｿｽ)
		if (m_uRate == 62500) {
			// 96kHz・ｽﾅ擾ｿｽ・ｽ・ｽ・ｽ・ｽ
			m_uRate = 96000;
			InitSub();

			// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾅゑｿｽ・ｽ・ｽ・ｽ鼾・ｿｽﾍ、・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽt・ｽ・ｽ・ｽ・ｽ
			if (m_lpDSb) {
				// ・ｽX・ｽ^・ｽ[・ｽg
				if (!m_bEnable) {
					m_lpDSb->Play(0, 0, DSBPLAY_LOOPING);
				}

				// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
				::Sleep(20);

				// ・ｽ~・ｽﾟゑｿｽ
				if (!m_bEnable) {
					m_lpDSb->Stop();
				}
			}

			// 62.5kHz・ｽﾅ擾ｿｽ・ｽ・ｽ・ｽ・ｽ
			CleanupSub();
			m_uRate = 62500;
		}
		InitSub();
	}

	// ・ｽ・ｽﾉ設抵ｿｽ
	if (m_pOPM) {
		SetVolume(pConfig->master_volume);
		m_pOPMIF->EnableFM(pConfig->fm_enable);
		SetFMVol(pConfig->fm_volume);
		m_pADPCM->EnableADPCM(pConfig->adpcm_enable);
		SetADPCMVol(pConfig->adpcm_volume);
	}
	m_nMaster = pConfig->master_volume;
	m_uPoll = (UINT)pConfig->polling_buffer;
}

//---------------------------------------------------------------------------
//
//	・ｽT・ｽ・ｽ・ｽv・ｽ・ｽ・ｽ・ｽ・ｽO・ｽ・ｽ・ｽ[・ｽg・ｽe・ｽ[・ｽu・ｽ・ｽ
//
//---------------------------------------------------------------------------
const UINT CSound::RateTable[] = {
	0,
	44100,
	48000,
	88200,
	96000,
	62500
};

//---------------------------------------------------------------------------
//
//	・ｽL・ｽ・ｽ・ｽt・ｽ・ｽ・ｽO・ｽﾝ抵ｿｽ
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Enable(BOOL bEnable)
{
	if (bEnable) {
		// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽL・ｽ・ｽ ・ｽ・ｽ・ｽt・ｽJ・ｽn
		if (!m_bEnable) {
			m_bEnable = TRUE;
			Play();
		}
	}
	else {
		// ・ｽL・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ ・ｽ・ｽ・ｽt・ｽ・ｽ~
		if (m_bEnable) {
			m_bEnable = FALSE;
			Stop();
		}
	}
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽt・ｽJ・ｽn
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Play()
{
	ASSERT(m_bEnable);

	// ・ｽ・ｽ・ｽﾉ会ｿｽ・ｽt・ｽJ・ｽn・ｽﾈゑｿｽK・ｽv・ｽﾈゑｿｽ
	if (m_bPlay) {
		return;
	}

	// ・ｽ|・ｽC・ｽ・ｽ・ｽ^・ｽ・ｽ・ｽL・ｽ・ｽ・ｽﾈら演・ｽt・ｽJ・ｽn
	if (m_pOPM) {
		m_lpDSb->Play(0, 0, DSBPLAY_LOOPING);
		m_bPlay = TRUE;
		m_uCount = 0;
		m_dwWrite = 0;
	}
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽt・ｽ・ｽ~
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Stop()
{
	// ・ｽ・ｽ・ｽﾉ会ｿｽ・ｽt・ｽ・ｽ~・ｽﾈゑｿｽK・ｽv・ｽﾈゑｿｽ
	if (!m_bPlay) {
		return;
	}

	// WAV・ｽZ・ｽ[・ｽu・ｽI・ｽ・ｽ
	if (m_pWav) {
		EndSaveWav();
	}

	// ・ｽ|・ｽC・ｽ・ｽ・ｽ^・ｽ・ｽ・ｽL・ｽ・ｽ・ｽﾈら演・ｽt・ｽ・ｽ~
	if (m_pOPM) {
		m_lpDSb->Stop();
		m_bPlay = FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	・ｽi・ｽs
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Process(BOOL bRun)
{
	HRESULT hr;
	DWORD dwOffset;
	DWORD dwWrite;
	DWORD dwRequest;
	DWORD dwReady;
	WORD *pBuf1;
	WORD *pBuf2;
	DWORD dwSize1;
	DWORD dwSize2;

	ASSERT(this);

	// ・ｽJ・ｽE・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ・ｽ(m_nPoll・ｽ・ｽﾉ１・ｽ・ｽA・ｽ・ｽ・ｽ・ｽ・ｽ・ｽVM・ｽ・ｽ~・ｽ・ｽ・ｽﾍ常時)
	m_uCount++;
	if ((m_uCount < m_uPoll) && bRun) {
		return;
	}
	m_uCount = 0;

	// ・ｽf・ｽB・ｽZ・ｽ[・ｽu・ｽ・ｽ・ｽﾈゑｿｽA・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾈゑｿｽ
	if (!m_bEnable) {
		return;
	}

	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽﾈゑｿｽ・ｽ・ｽﾎ、・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾈゑｿｽ
	if (!m_pOPM) {
		m_pScheduler->SetSoundTime(0);
		return;
	}

	// ・ｽv・ｽ・ｽ・ｽC・ｽ・ｽﾔでなゑｿｽ・ｽ・ｽﾎ、・ｽﾖ係・ｽﾈゑｿｽ
	if (!m_bPlay) {
		m_pScheduler->SetSoundTime(0);
		return;
	}

	// ・ｽ・ｽ・ｽﾝのプ・ｽ・ｽ・ｽC・ｽﾊ置・ｽｾゑｿｽ(・ｽo・ｽC・ｽg・ｽP・ｽ・ｽ)
	ASSERT(m_lpDSb);
	ASSERT(m_lpBuf);
	if (FAILED(m_lpDSb->GetCurrentPosition(&dwOffset, &dwWrite))) {
		return;
	}
	ASSERT(m_lpDSb);
	ASSERT(m_lpBuf);

	// ・ｽO・ｽ曹・ｽ・ｽ・ｽ・ｽｾ位置・ｽ・ｽ・ｽ・ｽA・ｽｫサ・ｽC・ｽY・ｽ・ｽ・ｽv・ｽZ(・ｽo・ｽC・ｽg・ｽP・ｽ・ｽ)
	if (m_dwWrite <= dwOffset) {
		dwRequest = dwOffset - m_dwWrite;
	}
	else {
		dwRequest = m_uBufSize - m_dwWrite;
		dwRequest += dwOffset;
	}

	// ・ｽｫサ・ｽC・ｽY・ｽ・ｽ・ｽS・ｽﾌゑｿｽ1/4・ｽｴゑｿｽ・ｽﾄゑｿｽ・ｽﾈゑｿｽ・ｽ・ｽﾎ、・ｽ・ｽ・ｽﾌ機・ｽ・ｽ・ｽ
	if (dwRequest < (m_uBufSize / 4)) {
		return;
	}

	// ・ｽｫサ・ｽ・ｽ・ｽv・ｽ・ｽ・ｽﾉ奇ｿｽ・ｽZ(L,R・ｽ・ｽ1・ｽﾂと撰ｿｽ・ｽ・ｽ・ｽ・ｽ)
	ASSERT((dwRequest & 3) == 0);
	dwRequest /= 4;

	// m_lpBuf・ｽﾉバ・ｽb・ｽt・ｽ@・ｽf・ｽ[・ｽ^・ｽ・ｽ・ｽ・ｬ・ｽB・ｽﾜゑｿｽbRun・ｽ`・ｽF・ｽb・ｽN
	if (!bRun) {
		memset(m_lpBuf, 0, m_uBufSize * 2);
		m_pOPMIF->InitBuf(m_uRate);
	}
	else {
		// OPM・ｽﾉ対ゑｿｽ・ｽﾄ、・ｽ・ｽ・ｽ・ｽ・ｽv・ｽ・ｽ・ｽﾆ托ｿｽ・ｽx・ｽ・ｽ・ｽ・ｽ
		dwReady = m_pOPMIF->ProcessBuf();
		m_pOPMIF->GetBuf(m_lpBuf, (int)dwRequest);
		if (dwReady < dwRequest) {
			dwRequest = dwReady;
		}

		// ADPCM・ｽﾉ対ゑｿｽ・ｽﾄ、・ｽf・ｽ[・ｽ^・ｽ・ｽv・ｽ・ｽ(・ｽ・ｽ・ｽZ・ｽ・ｽ・ｽ驍ｱ・ｽ・ｽ)
		m_pADPCM->GetBuf(m_lpBuf, (int)dwRequest);

		// ADPCM・ｽﾌ難ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
		if (dwReady > dwRequest) {
			m_pADPCM->Wait(dwReady - dwRequest);
		}
		else {
			m_pADPCM->Wait(0);
		}

		// SCSI・ｽﾉ対ゑｿｽ・ｽﾄ、・ｽf・ｽ[・ｽ^・ｽ・ｽv・ｽ・ｽ(・ｽ・ｽ・ｽZ・ｽ・ｽ・ｽ驍ｱ・ｽ・ｽ)
		m_pSCSI->GetBuf(m_lpBuf, (int)dwRequest, m_uRate);
	}

	// ・ｽ・ｽ・ｽ・ｽ・ｽﾅ・ｿｽ・ｽb・ｽN
	hr = m_lpDSb->Lock(m_dwWrite, (dwRequest * 4),
						(void**)&pBuf1, &dwSize1,
						(void**)&pBuf2, &dwSize2,
						0);
	// ・ｽo・ｽb・ｽt・ｽ@・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽﾎ、・ｽ・ｽ・ｽX・ｽg・ｽA
	if (hr == DSERR_BUFFERLOST) {
		m_lpDSb->Restore();
	}
	// ・ｽ・ｽ・ｽb・ｽN・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾈゑｿｽ・ｽ・ｽﾎ、・ｽ・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽﾓ厄ｿｽ・ｽ・ｽ・ｽﾈゑｿｽ
	if (FAILED(hr)) {
		m_dwWrite = dwOffset;
		return;
	}

	// ・ｽﾊ子・ｽ・ｽbit=16・ｽ・ｽO・ｽ・ｽﾆゑｿｽ・ｽ・ｽ
	ASSERT((dwSize1 & 1) == 0);
	ASSERT((dwSize2 & 1) == 0);

	// MMX・ｽ・ｽ・ｽﾟにゑｿｽ・ｽp・ｽb・ｽN(dwSize1+dwSize2・ｽﾅ、・ｽ・ｽ・ｽ・ｽ5000・ｽ`15000・ｽ・ｽ・ｽx・ｽﾍ擾ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ)
	SoundMMXPortable(m_lpBuf, pBuf1, dwSize1);
	if (dwSize2 > 0) {
		SoundMMXPortable(&m_lpBuf[dwSize1 / 2], pBuf2, dwSize2);
	}
	SoundEMMSPortable();

	// ・ｽA・ｽ・ｽ・ｽ・ｽ・ｽb・ｽN
	m_lpDSb->Unlock(pBuf1, dwSize1, pBuf2, dwSize2);

	// m_dwWrite・ｽX・ｽV
	m_dwWrite += dwSize1;
	m_dwWrite += dwSize2;
	if (m_dwWrite >= m_uBufSize) {
		m_dwWrite -= m_uBufSize;
	}
	ASSERT(m_dwWrite < m_uBufSize);

	// ・ｽ・ｽ・ｽ・・ｿｽﾈゑｿｽWAV・ｽX・ｽV
	if (bRun && m_pWav) {
		ProcessSaveWav((int*)m_lpBuf, (dwSize1 + dwSize2));
	}
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽﾊセ・ｽb・ｽg
//
//---------------------------------------------------------------------------
void FASTCALL CSound::SetVolume(int nVolume)
{
	LONG lVolume;

	ASSERT(this);
	ASSERT((nVolume >= 0) && (nVolume <= 100));

	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽﾈゑｿｽ・ｽ・ｽﾎ設定し・ｽﾈゑｿｽ
	if (!m_pOPM) {
		return;
	}

	// ・ｽl・ｽ・ｽﾏ奇ｿｽ
	lVolume = 100 - nVolume;
	lVolume *= (DSBVOLUME_MAX - DSBVOLUME_MIN);
	lVolume /= -200;

	// ・ｽﾝ抵ｿｽ
	m_lpDSb->SetVolume(lVolume);
}

//---------------------------------------------------------------------------
//
//	・ｽ・ｽ・ｽﾊ取得
//
//---------------------------------------------------------------------------
int FASTCALL CSound::GetVolume()
{
	LONG lVolume;

	ASSERT(this);

	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽﾈゑｿｽ・ｽ・ｽﾎ、・ｽ・ｽ・ｽ・ｽﾌ値・ｽ・ｽ・ｽｯ趣ｿｽ・ｽ
	if (!m_pOPM) {
		return m_nMaster;
	}

	// ・ｽl・ｽ・ｽ・ｽ謫ｾ
	ASSERT(m_lpDSb);
	if (FAILED(m_lpDSb->GetVolume(&lVolume))) {
		return 0;
	}

	// ・ｽl・ｽ・ｽ竦ｳ
	lVolume *= -200;
	lVolume /= (DSBVOLUME_MAX - DSBVOLUME_MIN);
	ASSERT((lVolume >= 0) && (lVolume <= 200));
	if (lVolume >= 100) {
		lVolume = 0;
	}
	else {
		lVolume = 100 - lVolume;
	}

	return (int)lVolume;
}

//---------------------------------------------------------------------------
//
//	FM・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾊセ・ｽb・ｽg
//
//---------------------------------------------------------------------------
void FASTCALL CSound::SetFMVol(int nVol)
{
	ASSERT(this);
	ASSERT((nVol >= 0) && (nVol <= 100));

	// ・ｽR・ｽs・ｽ[
	m_nFMVol = nVol;

	// ・ｽﾝ抵ｿｽ
	if (m_pOPM) {
		m_pOPM->SetVolume(m_nFMVol);
	}
}

//---------------------------------------------------------------------------
//
//	ADPCM・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾊセ・ｽb・ｽg
//
//---------------------------------------------------------------------------
void FASTCALL CSound::SetADPCMVol(int nVol)
{
	ASSERT(this);
	ASSERT((nVol >= 0) && (nVol <= 100));

	// ・ｽR・ｽs・ｽ[
	m_nADPCMVol = nVol;

	// ・ｽﾝ抵ｿｽ
	ASSERT(m_pADPCM);
	m_pADPCM->SetVolume(m_nADPCMVol);
}

//---------------------------------------------------------------------------
//
//	・ｽf・ｽo・ｽC・ｽX・ｽ・ｽ
//
//---------------------------------------------------------------------------
void FASTCALL CSound::EnumDevice()
{
	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	m_nDeviceNum = 0;

	// ・ｽ投J・ｽn
	DirectSoundEnumerate(EnumCallback, this);
}

//---------------------------------------------------------------------------
//
//	・ｽf・ｽo・ｽC・ｽX・ｽ塔R・ｽ[・ｽ・ｽ・ｽo・ｽb・ｽN
//
//---------------------------------------------------------------------------
BOOL CALLBACK CSound::EnumCallback(LPGUID lpGuid, LPCTSTR lpDescr, LPCTSTR /*lpModule*/, LPVOID lpContext)
{
	CSound *pSound;
	int index;

	// this・ｽ|・ｽC・ｽ・ｽ・ｽ^・ｽｯ趣ｿｽ・ｽ
	pSound = (CSound*)lpContext;
	ASSERT(pSound);

	// ・ｽJ・ｽ・ｽ・ｽ・ｽ・ｽg・ｽ・ｽ16・ｽ・ｽ・ｽ・ｽ・ｽﾈゑｿｽL・ｽ・ｽ
	if (pSound->m_nDeviceNum < 16) {
		index = pSound->m_nDeviceNum;

		// ・ｽo・ｽ^
		pSound->m_lpGUID[index] = lpGuid;
		pSound->m_DeviceDescr[index] = lpDescr;
		pSound->m_nDeviceNum++;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	・ｽ}・ｽX・ｽ^・ｽ・ｽ・ｽﾊ取得
//
//---------------------------------------------------------------------------
int FASTCALL CSound::GetMasterVol(int& nMaximum)
{
	MMRESULT mmResult;
	HMIXER hMixer;
	MIXERLINE mixLine;
	MIXERLINECONTROLS mixLCs;
	MIXERCONTROL mixCtrl;
	MIXERCONTROLDETAILS mixDetail;
	MIXERCONTROLDETAILS_UNSIGNED *pData;
	int nNum;
	int nValue;

	ASSERT(this);

	// ・ｽg・ｽp・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽf・ｽo・ｽC・ｽX・ｽﾔ搾ｿｽ・ｽ・ｽ0・ｽﾅゑｿｽ・ｽ驍ｱ・ｽﾆゑｿｽ・ｽK・ｽv
	if ((m_nSelectDevice != 0) || (m_uRate == 0)) {
		return -1;
	}

	// ・ｽ~・ｽL・ｽT・ｽ・ｽ・ｽI・ｽ[・ｽv・ｽ・ｽ
	mmResult = ::mixerOpen(&hMixer, 0, 0, 0,
					MIXER_OBJECTF_MIXER);
	if (mmResult != MMSYSERR_NOERROR) {
		// ・ｽ~・ｽL・ｽT・ｽI・ｽ[・ｽv・ｽ・ｽ・ｽG・ｽ・ｽ・ｽ[
		return -1;
	}

	// ・ｽ・ｽ・ｽC・ｽ・ｽ・ｽ・ｽ・ｽｾゑｿｽ
	memset(&mixLine, 0, sizeof(mixLine));
	mixLine.cbStruct = sizeof(mixLine);
	mixLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
	mmResult = ::mixerGetLineInfo((HMIXEROBJ)hMixer, &mixLine,
					MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE);
	if (mmResult != MMSYSERR_NOERROR) {
		// ・ｽN・ｽ・ｽ・ｽ[・ｽY・ｽ・ｽ・ｽﾄ終・ｽ・ｽ
		::mixerClose(hMixer);
		return -1;
	}

	// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ・ｽ・ｽｾゑｿｽ
	memset(&mixLCs, 0, sizeof(mixLCs));
	mixLCs.cbStruct = sizeof(mixLCs);
	mixLCs.dwLineID = mixLine.dwLineID;
	mixLCs.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
	mixLCs.cbmxctrl = sizeof(mixCtrl);
	mixLCs.pamxctrl = &mixCtrl;
	memset(&mixCtrl, 0, sizeof(mixCtrl));
	mixCtrl.cbStruct = sizeof(mixCtrl);
	mmResult = ::mixerGetLineControls((HMIXEROBJ)hMixer, &mixLCs,
					MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE);
	if (mmResult != MMSYSERR_NOERROR) {
		// ・ｽN・ｽ・ｽ・ｽ[・ｽY・ｽ・ｽ・ｽﾄ終・ｽ・ｽ
		::mixerClose(hMixer);
		return -1;
	}

	// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾌ個撰ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽﾄ、・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽm・ｽ・ｽ
	nNum = 1;
	if (mixLine.cChannels > 0) {
		nNum *= mixLine.cChannels;
	}
	if (mixCtrl.cMultipleItems > 0) {
		nNum *= mixCtrl.cMultipleItems;
	}
	pData = new MIXERCONTROLDETAILS_UNSIGNED[nNum];

	// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾌ値・ｽｾゑｿｽ
	memset(&mixDetail, 0, sizeof(mixDetail));
	mixDetail.cbStruct = sizeof(mixDetail);
	mixDetail.dwControlID = mixCtrl.dwControlID;
	mixDetail.cChannels = mixLine.cChannels;
	mixDetail.cMultipleItems = mixCtrl.cMultipleItems;
	mixDetail.cbDetails = nNum * sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	mixDetail.paDetails = pData;
	mmResult = ::mixerGetControlDetails((HMIXEROBJ)hMixer, &mixDetail,
					MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE);
	if (mmResult != MMSYSERR_NOERROR) {
		// ・ｽN・ｽ・ｽ・ｽ[・ｽY・ｽ・ｽ・ｽﾄ終・ｽ・ｽ
		delete[] pData;
		::mixerClose(hMixer);
		return -1;
	}

	// ・ｽﾅ擾ｿｽ・ｽl・ｽ・ｽ0・ｽﾌ場合・ｽﾌゑｿｽ
	if (mixCtrl.Bounds.lMinimum != 0) {
		// ・ｽN・ｽ・ｽ・ｽ[・ｽY・ｽ・ｽ・ｽﾄ終・ｽ・ｽ
		delete[] pData;
		::mixerClose(hMixer);
		return -1;
	}

	// ・ｽl・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	nValue = pData[0].dwValue;
	nMaximum = mixCtrl.Bounds.lMaximum;

	// ・ｽ・ｽ・ｽ・ｽ
	delete[] pData;
	::mixerClose(hMixer);

	return nValue;
}

//---------------------------------------------------------------------------
//
//	・ｽ}・ｽX・ｽ^・ｽ・ｽ・ｽﾊセ・ｽb・ｽg
//
//---------------------------------------------------------------------------
void FASTCALL CSound::SetMasterVol(int nVolume)
{
	MMRESULT mmResult;
	HMIXER hMixer;
	MIXERLINE mixLine;
	MIXERLINECONTROLS mixLCs;
	MIXERCONTROL mixCtrl;
	MIXERCONTROLDETAILS mixDetail;
	MIXERCONTROLDETAILS_UNSIGNED *pData;
	int nIndex;
	int nNum;

	ASSERT(this);

	// ・ｽg・ｽp・ｽ・ｽ・ｽﾄゑｿｽ・ｽ・ｽf・ｽo・ｽC・ｽX・ｽﾔ搾ｿｽ・ｽ・ｽ0・ｽﾅゑｿｽ・ｽ驍ｱ・ｽﾆゑｿｽ・ｽK・ｽv
	if ((m_nSelectDevice != 0) || (m_uRate == 0)) {
		return;
	}

	// ・ｽ~・ｽL・ｽT・ｽ・ｽ・ｽI・ｽ[・ｽv・ｽ・ｽ
	mmResult = ::mixerOpen(&hMixer, 0, 0, 0,
					MIXER_OBJECTF_MIXER);
	if (mmResult != MMSYSERR_NOERROR) {
		// ・ｽ~・ｽL・ｽT・ｽI・ｽ[・ｽv・ｽ・ｽ・ｽG・ｽ・ｽ・ｽ[
		return;
	}

	// ・ｽ・ｽ・ｽC・ｽ・ｽ・ｽ・ｽ・ｽｾゑｿｽ
	memset(&mixLine, 0, sizeof(mixLine));
	mixLine.cbStruct = sizeof(mixLine);
	mixLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
	mmResult = ::mixerGetLineInfo((HMIXEROBJ)hMixer, &mixLine,
					MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE);
	if (mmResult != MMSYSERR_NOERROR) {
		// ・ｽN・ｽ・ｽ・ｽ[・ｽY・ｽ・ｽ・ｽﾄ終・ｽ・ｽ
		::mixerClose(hMixer);
		return;
	}

	// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ・ｽ・ｽｾゑｿｽ
	memset(&mixLCs, 0, sizeof(mixLCs));
	mixLCs.cbStruct = sizeof(mixLCs);
	mixLCs.dwLineID = mixLine.dwLineID;
	mixLCs.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
	mixLCs.cbmxctrl = sizeof(mixCtrl);
	mixLCs.pamxctrl = &mixCtrl;
	memset(&mixCtrl, 0, sizeof(mixCtrl));
	mixCtrl.cbStruct = sizeof(mixCtrl);
	mmResult = ::mixerGetLineControls((HMIXEROBJ)hMixer, &mixLCs,
					MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE);
	if (mmResult != MMSYSERR_NOERROR) {
		// ・ｽN・ｽ・ｽ・ｽ[・ｽY・ｽ・ｽ・ｽﾄ終・ｽ・ｽ
		::mixerClose(hMixer);
		return;
	}

	// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾌ個撰ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽﾄ、・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽm・ｽ・ｽ
	nNum = 1;
	if (mixLine.cChannels > 0) {
		nNum *= mixLine.cChannels;
	}
	if (mixCtrl.cMultipleItems > 0) {
		nNum *= mixCtrl.cMultipleItems;
	}
	pData = new MIXERCONTROLDETAILS_UNSIGNED[nNum];

	// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾌ値・ｽｾゑｿｽ
	memset(&mixDetail, 0, sizeof(mixDetail));
	mixDetail.cbStruct = sizeof(mixDetail);
	mixDetail.dwControlID = mixCtrl.dwControlID;
	mixDetail.cChannels = mixLine.cChannels;
	mixDetail.cMultipleItems = mixCtrl.cMultipleItems;
	mixDetail.cbDetails = nNum * sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	mixDetail.paDetails = pData;
	mmResult = ::mixerGetControlDetails((HMIXEROBJ)hMixer, &mixDetail,
					MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE);
	if (mmResult != MMSYSERR_NOERROR) {
		// ・ｽN・ｽ・ｽ・ｽ[・ｽY・ｽ・ｽ・ｽﾄ終・ｽ・ｽ
		delete[] pData;
		::mixerClose(hMixer);
		return;
	}

	// ・ｽl・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	ASSERT(mixCtrl.Bounds.lMinimum <= nVolume);
	ASSERT(nVolume <= mixCtrl.Bounds.lMaximum);
	for (nIndex=0; nIndex<nNum; nIndex++) {
		pData[nIndex].dwValue = (DWORD)nVolume;
	}

	// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾌ値・ｽ・ｽﾝ抵ｿｽ
	mmResult = mixerSetControlDetails((HMIXEROBJ)hMixer, &mixDetail,
					MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE);
	if (mmResult != MMSYSERR_NOERROR) {
		delete[] pData;
		::mixerClose(hMixer);
		return;
	}

	// ・ｽ・ｽ・ｽ・ｽ
	::mixerClose(hMixer);
	delete[] pData;
}

//---------------------------------------------------------------------------
//
//	WAV・ｽZ・ｽ[・ｽu・ｽJ・ｽn
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSound::StartSaveWav(LPCTSTR lpszWavFile)
{
	DWORD dwSize;
	WAVEFORMATEX wfex;

	ASSERT(this);
	ASSERT(lpszWavFile);

	// ・ｽ・ｽ・ｽﾉ録・ｽ・ｽ・ｽ・ｽ・ｽﾈゑｿｽG・ｽ・ｽ・ｽ[
	if (m_pWav) {
		return FALSE;
	}
	// ・ｽﾄ撰ｿｽ・ｽ・ｽ・ｽﾅなゑｿｽ・ｽ・ｽﾎエ・ｽ・ｽ・ｽ[
	if (!m_bEnable || !m_bPlay) {
		return FALSE;
	}

	// ・ｽt・ｽ@・ｽC・ｽ・ｽ・ｽ・ｬ・ｽ・ｽ・ｽ・ｽ・ｽﾝゑｿｽ
	if (!m_WavFile.Open(lpszWavFile, Fileio::WriteOnly)) {
		return FALSE;
	}

	// RIFF・ｽw・ｽb・ｽ_・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	if (!m_WavFile.Write((BYTE*)"RIFF0123WAVEfmt ", 16)) {
		m_WavFile.Close();
		return FALSE;
	}

	// WAVEFORMATEX・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	dwSize = sizeof(wfex);
	if (!m_WavFile.Write((BYTE*)&dwSize, sizeof(dwSize))) {
		m_WavFile.Close();
		return FALSE;
	}
	memset(&wfex, 0, sizeof(wfex));
	wfex.cbSize = sizeof(wfex);
	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nChannels = 2;
	wfex.nSamplesPerSec = m_uRate;
	wfex.nBlockAlign = 4;
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;
	wfex.wBitsPerSample = 16;
	if (!m_WavFile.Write((BYTE*)&wfex, sizeof(wfex))) {
		m_WavFile.Close();
		return FALSE;
	}

	// data・ｽT・ｽu・ｽw・ｽb・ｽ_・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	if (!m_WavFile.Write((BYTE*)"data0123", 8)) {
		m_WavFile.Close();
		return FALSE;
	}

	// ・ｽ^・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@・ｽ・ｽ・ｽm・ｽ・ｽ
	try {
		m_pWav = new WORD[0x20000];
	}
	catch (...) {
		return FALSE;
	}
	if (!m_pWav) {
		return FALSE;
	}

	// ・ｽ・ｽ・ｽ[・ｽN・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	m_nWav = 0;
	m_dwWav = 0;

	// ・ｽR・ｽ・ｽ・ｽ|・ｽ[・ｽl・ｽ・ｽ・ｽg・ｽﾌフ・ｽ・ｽ・ｽO・ｽ・ｽ・ｽN・ｽ・ｽ・ｽA
	m_pOPMIF->ClrStarted();
	m_pADPCM->ClrStarted();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	WAV・ｽZ・ｽ[・ｽu・ｽ・ｽ・ｽ・ｽ
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSound::IsSaveWav() const
{
	ASSERT(this);

	// ・ｽo・ｽb・ｽt・ｽ@・ｽﾅチ・ｽF・ｽb・ｽN
	if (m_pWav) {
		return TRUE;
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	WAV・ｽZ・ｽ[・ｽu
//
//---------------------------------------------------------------------------
void FASTCALL CSound::ProcessSaveWav(int *pStream, DWORD dwLength)
{
	DWORD dwPrev;
	int i;
	int nLen;
	int rawData;

	ASSERT(this);
	ASSERT(pStream);
	ASSERT(dwLength > 0);
	ASSERT((dwLength & 3) == 0);

	// Started・ｽt・ｽ・ｽ・ｽO・ｽｲべ、・ｽﾆゑｿｽ・ｽﾉク・ｽ・ｽ・ｽA・ｽﾈゑｿｽX・ｽL・ｽb・ｽv
	if (!m_pOPMIF->IsStarted() && !m_pADPCM->IsStarted()) {
		return;
	}

	// ・ｽO・ｽ・ｽ・ｽE・ｽ續ｼ・ｽﾉ包ｿｽ・ｽ・ｽ・ｽ・ｽK・ｽv・ｽ・ｽ・ｽ・ｽ・ｽ驍ｩ・ｽ`・ｽF・ｽb・ｽN
	dwPrev = 0;
	if ((dwLength + m_nWav) >= 256 * 1024) {
		dwPrev = 256 * 1024 - m_nWav;
		dwLength -= dwPrev;
	}

	// ・ｽO・ｽ・ｽ
	if (dwPrev > 0) {
		nLen = (int)(dwPrev >> 1);
		for (i=0; i<nLen; i++) {
			rawData = *pStream++;
			if (rawData > 0x7fff) {
				rawData = 0x7fff;
			}
			if (rawData < -0x8000) {
				rawData = -0x8000;
			}
			m_pWav[(m_nWav >> 1) + i] = (WORD)rawData;
		}

		m_WavFile.Write(m_pWav, 256 * 1024);
		m_nWav = 0;
	}

	// ・ｽ續ｼ
	nLen = (int)(dwLength >> 1);
	for (i=0; i<nLen; i++) {
		rawData = *pStream++;
		if (rawData > 0x7fff) {
			rawData = 0x7fff;
		}
		if (rawData < -0x8000) {
			rawData = -0x8000;
		}
		m_pWav[(m_nWav >> 1) + i] = (WORD)rawData;
	}
	m_nWav += dwLength;
	m_dwWav += dwPrev;
	m_dwWav += dwLength;
}

//---------------------------------------------------------------------------
//
//	WAV・ｽZ・ｽ[・ｽu・ｽI・ｽ・ｽ
//
//---------------------------------------------------------------------------
void FASTCALL CSound::EndSaveWav()
{
	DWORD dwLength;

	// ・ｽc・ｽ・ｽ・ｽ・ｽ・ｽf・ｽ[・ｽ^・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	if (m_nWav > 0) {
		m_WavFile.Write(m_pWav, m_nWav);
		m_nWav = 0;
	}

	// ・ｽw・ｽb・ｽ_・ｽC・ｽ・ｽ
	m_WavFile.Seek(4);
	dwLength = m_dwWav + sizeof(WAVEFORMATEX) + 20;
	m_WavFile.Write((BYTE*)&dwLength, sizeof(dwLength));
	m_WavFile.Seek(sizeof(WAVEFORMATEX) + 24);
	m_WavFile.Write((BYTE*)&m_dwWav, sizeof(m_dwWav));

	// ・ｽt・ｽ@・ｽC・ｽ・ｽ・ｽN・ｽ・ｽ・ｽ[・ｽY
	m_WavFile.Close();

	// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	delete[] m_pWav;
	m_pWav = NULL;
}

#endif	// _WIN32
