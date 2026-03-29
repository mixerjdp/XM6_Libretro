//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ DMAC(HD63450) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "cpu.h"
#include "memory.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "fdc.h"
#include "fileio.h"
#include "dmac.h"
#include "x68sound_bridge.h"

//===========================================================================
//
//	DMAC
//
//===========================================================================
//#define DMAC_LOG

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
DMAC::DMAC(VM *p) : MemDevice(p)
{
	// �f�o�C�XID��������
	dev.id = MAKEID('D', 'M', 'A', 'C');
	dev.desc = "DMAC (HD63450)";

	// �J�n�A�h���X�A�I���A�h���X
	memdev.first = 0xe84000;
	memdev.last = 0xe85fff;

	// ���̑�
	memory = NULL;
	fdc = NULL;
	legacy_cnt_mode = FALSE;
}

//---------------------------------------------------------------------------
//
//	������
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::Init()
{
	int ch;

	ASSERT(this);

	// ��{�N���X
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// �������擾
	memory = (Memory*)vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(memory);

	// FDC�擾
	fdc = (FDC*)vm->SearchDevice(MAKEID('F', 'D', 'C', ' '));
	ASSERT(fdc);

	// �`���l�����[�N��������
	for (ch=0; ch<4; ch++) {
		memset(&dma[ch], 0, sizeof(dma[ch]));
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�N���[���A�b�v
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::Cleanup()
{
	ASSERT(this);

	// ��{�N���X��
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	���Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::Reset()
{
	int ch;

	ASSERT(this);
	LOG0(Log::Normal, "���Z�b�g");

	// �O���[�o��
	dmactrl.transfer = 0;
	dmactrl.load = 0;
	dmactrl.exec = FALSE;
	dmactrl.current_ch = 0;
	dmactrl.cpu_cycle = 0;
	dmactrl.vector = -1;

	// DMAC�`���l�������
	for (ch=0; ch<4; ch++) {
		ResetDMA(ch);
	}
}

//---------------------------------------------------------------------------
//
//	�Z�[�u
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;
	int i;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "�Z�[�u");

	// �`���l����
	sz = sizeof(dma_t);
	for (i=0; i<4; i++) {
		if (!fio->Write(&sz, sizeof(sz))) {
			return FALSE;
		}
		if (!fio->Write(&dma[i], (int)sz)) {
			return FALSE;
		}
	}

	// �O���[�o��
	sz = sizeof(dmactrl_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (!fio->Write(&dmactrl, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	���[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::Load(Fileio *fio, int /*ver*/)
{
	int i;
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "���[�h");

	// �`���l����
	for (i=0; i<4; i++) {
		// �T�C�Y�����[�h�A�ƍ�
		if (!fio->Read(&sz, sizeof(sz))) {
			return FALSE;
		}
		if (sz != sizeof(dma_t)) {
			return FALSE;
		}

		// ���̂����[�h
		if (!fio->Read(&dma[i], (int)sz)) {
			return FALSE;
		}
	}

	// �O���[�o��
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(dmactrl_t)) {
		return FALSE;
	}

	if (!fio->Read(&dmactrl, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�ݒ�K�p
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::ApplyCfg(const Config* /*config*/)
{
	ASSERT(this);
//	ASSERT(config);
	LOG0(Log::Normal, "�ݒ�K�p");
}

//---------------------------------------------------------------------------
//
//	�o�C�g�ǂݍ���
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::ReadByte(DWORD addr)
{
	int ch;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// �E�F�C�g
	scheduler->Wait(7);

	// �`���l���Ɋ��蓖��
	ch = (int)(addr >> 6);
	ch &= 3;
	addr &= 0x3f;

	// �`���l���P�ʂōs��
	return ReadDMA(ch, addr);
}

//---------------------------------------------------------------------------
//
//	���[�h�ǂݍ���
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::ReadWord(DWORD addr)
{
	int ch;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);

	// �E�F�C�g
	scheduler->Wait(7);

	// �`���l���Ɋ��蓖��
	ch = (int)(addr >> 6);
	ch &= 3;
	addr &= 0x3f;

	// �`���l���P�ʂōs��
	return ((ReadDMA(ch, addr) << 8) | ReadDMA(ch, addr + 1));
}

//---------------------------------------------------------------------------
//
//	�o�C�g��������
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::WriteByte(DWORD addr, DWORD data)
{
	int ch;
	bool mirror_to_x68sound;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);

	scheduler->Wait(7);

	ch = (int)(addr >> 6);
	ch &= 3;
	addr &= 0x3f;
	mirror_to_x68sound = (ch == 3) && !(dma[ch].act && (addr == 0x04 || addr == 0x05 || addr == 0x06 || addr == 0x29 || addr == 0x31 || (addr == 0x07 && data == 0x48)));

	WriteDMA(ch, addr, data);
	if (mirror_to_x68sound) {
		Xm6X68Sound::WriteDma(static_cast<unsigned char>(addr), static_cast<unsigned char>(data));
	}
}

void FASTCALL DMAC::WriteWord(DWORD addr, DWORD data)
{
	int ch;
	bool mirror_to_x68sound;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	scheduler->Wait(7);

	ch = (int)(addr >> 6);
	ch &= 3;
	addr &= 0x3f;

	WriteDMA(ch, addr, (BYTE)(data >> 8));
	mirror_to_x68sound = (ch == 3) && !(dma[ch].act && (addr == 0x04 || addr == 0x05 || addr == 0x06 || addr == 0x29 || addr == 0x31 || (addr == 0x07 && data == 0x48)));
	if (mirror_to_x68sound) {
		Xm6X68Sound::WriteDma(static_cast<unsigned char>(addr), static_cast<unsigned char>(data >> 8));
	}
	WriteDMA(ch, addr + 1, (BYTE)data);
	mirror_to_x68sound = (ch == 3) && !(dma[ch].act && (addr + 1 == 0x04 || addr + 1 == 0x05 || addr + 1 == 0x06 || addr + 1 == 0x29 || addr + 1 == 0x31 || (addr + 1 == 0x07 && (data & 0xff) == 0x48)));
	if (mirror_to_x68sound) {
		Xm6X68Sound::WriteDma(static_cast<unsigned char>(addr + 1), static_cast<unsigned char>(data));
	}
}
DWORD FASTCALL DMAC::ReadOnly(DWORD addr) const
{
	int ch;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// �`���l���Ɋ��蓖��
	ch = (int)(addr >> 6);
	ch &= 3;
	addr &= 0x3f;

	// �`���l���P�ʂōs��
	return ReadDMA(ch, addr);
}

//---------------------------------------------------------------------------
//
//	DMA�ǂݍ���
//	����ʃ[���ۏ�
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::ReadDMA(int ch, DWORD addr) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(addr <= 0x3f);

	switch (addr) {
		// CSR
		case 0x00:
			return GetCSR(ch);

		// CER
		case 0x01:
			return dma[ch].ecode;

		// DCR
		case 0x04:
			return GetDCR(ch);

		// OCR
		case 0x05:
			return GetOCR(ch);

		// SCR
		case 0x06:
			return GetSCR(ch);

		// CCR
		case 0x07:
			return GetCCR(ch);

		// MTC
		case 0x0a:
			return (BYTE)(dma[ch].mtc >> 8);
		case 0x0b:
			return (BYTE)dma[ch].mtc;

		// MAR
		case 0x0c:
			return 0;
		case 0x0d:
			return (BYTE)(dma[ch].mar >> 16);
		case 0x0e:
			return (BYTE)(dma[ch].mar >> 8);
		case 0x0f:
			return (BYTE)dma[ch].mar;

		// DAR
		case 0x14:
			return 0;
		case 0x15:
			return (BYTE)(dma[ch].dar >> 16);
		case 0x16:
			return (BYTE)(dma[ch].dar >> 8);
		case 0x17:
			return (BYTE)dma[ch].dar;

		// BTC
		case 0x1a:
			return (BYTE)(dma[ch].btc >> 8);
		case 0x1b:
			return (BYTE)dma[ch].btc;

		// BAR
		case 0x1c:
			return 0;
		case 0x1d:
			return (BYTE)(dma[ch].bar >> 16);
		case 0x1e:
			return (BYTE)(dma[ch].bar >> 8);
		case 0x1f:
			return (BYTE)dma[ch].bar;

		// NIV
		case 0x25:
			return dma[ch].niv;

		// EIV
		case 0x27:
			return dma[ch].eiv;

		// MFC
		case 0x29:
			return dma[ch].mfc;

		// CPR
		case 0x2d:
			return dma[ch].cp;

		// DFC
		case 0x31:
			return dma[ch].dfc;

		// BFC
		case 0x39:
			return dma[ch].bfc;

		// GCR
		case 0x3f:
			if (ch == 3) {
				// �`���l��3�̂݃o�[�X�g�]������Ԃ�
				ASSERT(dma[ch].bt <= 3);
				ASSERT(dma[ch].br <= 3);

				data = dma[ch].bt;
				data <<= 2;
				data |= dma[ch].br;
				return data;
			}
			return 0xff;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	DMA��������
//	����ʃ[���ۏ؂�v��
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::WriteDMA(int ch, DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(addr <= 0x3f);
	ASSERT(data < 0x100);

	switch (addr) {
		// CSR
		case 0x00:
			SetCSR(ch, data);
			return;

		// CER(Read Only)
		case 0x01:
			return;

		// DCR
		case 0x04:
			SetDCR(ch, data);
			return;

		// OCR
		case 0x05:
			SetOCR(ch, data);
			return;

		// SCR
		case 0x06:
			SetSCR(ch, data);
			return;

		// CCR
		case 0x07:
			SetCCR(ch, data);
			return;

		// MTC
		case 0x0a:
			dma[ch].mtc &= 0x00ff;
			dma[ch].mtc |= (data << 8);
			return;
		case 0x0b:
			dma[ch].mtc &= 0xff00;
			dma[ch].mtc |= data;
			return;

		// MAR
		case 0x0c:
			return;
		case 0x0d:
			dma[ch].mar &= 0x0000ffff;
			dma[ch].mar |= (data << 16);
			return;
		case 0x0e:
			dma[ch].mar &= 0x00ff00ff;
			dma[ch].mar |= (data << 8);
			return;
		case 0x0f:
			dma[ch].mar &= 0x00ffff00;
			dma[ch].mar |= data;
			return;

		// DAR
		case 0x14:
			return;
		case 0x15:
			dma[ch].dar &= 0x0000ffff;
			dma[ch].dar |= (data << 16);
			return;
		case 0x16:
			dma[ch].dar &= 0x00ff00ff;
			dma[ch].dar |= (data << 8);
			return;
		case 0x17:
			dma[ch].dar &= 0x00ffff00;
			dma[ch].dar |= data;
			return;

		// BTC
		case 0x1a:
			dma[ch].btc &= 0x00ff;
			dma[ch].btc |= (data << 8);
			return;
		case 0x1b:
			dma[ch].btc &= 0xff00;
			dma[ch].btc |= data;
			return;

		// BAR
		case 0x1c:
			return;
		case 0x1d:
			dma[ch].bar &= 0x0000ffff;
			dma[ch].bar |=(data << 16);
			return;
		case 0x1e:
			dma[ch].bar &= 0x00ff00ff;
			dma[ch].bar |= (data << 8);
			return;
		case 0x1f:
			dma[ch].bar &= 0x00ffff00;
			dma[ch].bar |= data;
			return;

		// NIV
		case 0x25:
			dma[ch].niv = data;
			return;

		// EIV
		case 0x27:
			dma[ch].eiv = data;
			return;

		// MFC
		case 0x29:
			dma[ch].mfc = data;
			return;

		// CPR
		case 0x2d:
			dma[ch].cp = data & 0x03;
			return;

		// DFC
		case 0x31:
			dma[ch].dfc = data;
			return;

		// BFC
		case 0x39:
			dma[ch].bfc = data;
			return;

		// GCR
		case 0x3f:
			if (ch == 3) {
				// �`���l��3�̂�
				SetGCR(data);
			}
			return;
	}
}

//---------------------------------------------------------------------------
//
//	DCR�Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::SetDCR(int ch, DWORD data)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(data < 0x100);

	// ACT���オ���Ă���΃^�C�~���O�G���[
	if (dma[ch].act) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "�`���l��%d �^�C�~���O�G���[(SetDCR)", ch);
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x02);
		return;
	}

	// XRM
	dma[ch].xrm = data >> 6;

	// DTYP
	dma[ch].dtyp = (data >> 4) & 0x03;

	// DPS
	if (data & 0x08) {
		dma[ch].dps = TRUE;
	}
	else {
		dma[ch].dps = FALSE;
	}

	// PCL
	dma[ch].pcl = (data & 0x03);

	// ���荞�݃`�F�b�N
	Interrupt();
}

//---------------------------------------------------------------------------
//
//	DCR�擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::GetDCR(int ch) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(dma[ch].xrm <= 3);
	ASSERT(dma[ch].dtyp <= 3);
	ASSERT(dma[ch].pcl <= 3);

	// �f�[�^�쐬
	data = dma[ch].xrm;
	data <<= 2;
	data |= dma[ch].dtyp;
	data <<= 1;
	if (dma[ch].dps) {
		data |= 0x01;
	}
	data <<= 3;
	data |= dma[ch].pcl;

	return data;
}

//---------------------------------------------------------------------------
//
//	OCR�Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::SetOCR(int ch, DWORD data)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(data < 0x100);

	// ACT���オ���Ă���΃^�C�~���O�G���[
	if (dma[ch].act) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "�`���l��%d �^�C�~���O�G���[(SetOCR)", ch);
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x02);
		return;
	}

	// DIR
	if (data & 0x80) {
		dma[ch].dir = TRUE;
	}
	else {
		dma[ch].dir = FALSE;
	}

	// BTD
	if (data & 0x40) {
		dma[ch].btd = TRUE;
	}
	else {
		dma[ch].btd = FALSE;
	}

	// SIZE
	dma[ch].size = (data >> 4) & 0x03;

	// CHAIN
	dma[ch].chain = (data >> 2) & 0x03;

	// REQG
	dma[ch].reqg = (data & 0x03);
}

//---------------------------------------------------------------------------
//
//	OCR�擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::GetOCR(int ch) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(dma[ch].size <= 3);
	ASSERT(dma[ch].chain <= 3);
	ASSERT(dma[ch].reqg <= 3);

	// �f�[�^�쐬
	data = 0;
	if (dma[ch].dir) {
		data |= 0x02;
	}
	if (dma[ch].btd) {
		data |= 0x01;
	}
	data <<= 2;
	data |= dma[ch].size;
	data <<= 2;
	data |= dma[ch].chain;
	data <<= 2;
	data |= dma[ch].reqg;

	return data;
}

//---------------------------------------------------------------------------
//
//	SCR�Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::SetSCR(int ch, DWORD data)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(data < 0x100);

	// ACT���オ���Ă���΃^�C�~���O�G���[
	if (dma[ch].act) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "�`���l��%d �^�C�~���O�G���[(SetSCR)", ch);
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x02);
		return;
	}

	dma[ch].mac = (data >> 2) & 0x03;
	dma[ch].dac = (data & 0x03);
}

//---------------------------------------------------------------------------
//
//	SCR�擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::GetSCR(int ch) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(dma[ch].mac <= 3);
	ASSERT(dma[ch].dac <= 3);

	// �f�[�^�쐬
	data = dma[ch].mac;
	data <<= 2;
	data |= dma[ch].dac;

	return data;
}

//---------------------------------------------------------------------------
//
//	CCR�Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::SetCCR(int ch, DWORD data)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(data < 0x100);

	// INT
	if (data & 0x08) {
		dma[ch].intr = TRUE;
	}
	else {
		dma[ch].intr = FALSE;
	}

	// HLT
	if (data & 0x20) {
		dma[ch].hlt = TRUE;
	}
	else {
		dma[ch].hlt = FALSE;
	}

	// STR
	if (data & 0x80) {
		dma[ch].str = TRUE;
		StartDMA(ch);
	}

	// CNT
	if (data & 0x40) {
		dma[ch].cnt = TRUE;
		ContDMA(ch);
	}
	else if (!legacy_cnt_mode) {
		dma[ch].cnt = FALSE;
	}

	// SAB
	if (data & 0x10) {
		dma[ch].sab = TRUE;
		AbortDMA(ch);
	}
}

//---------------------------------------------------------------------------
//
//	CCR�擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::GetCCR(int ch) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

	// INT,HLT,STR,CNT�����Ԃ�
	data = 0;
	if (dma[ch].intr) {
		data |= 0x08;
	}
	if (dma[ch].hlt) {
		data |= 0x20;
	}
	if (dma[ch].str) {
		data |= 0x80;
	}
	if (dma[ch].cnt) {
		data |= 0x40;
	}

	return data;
}

//---------------------------------------------------------------------------
//
//	CSR�Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::SetCSR(int ch, DWORD data)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(data < 0x100);

	// ACT,PCS�ȊO��1���������ނ��Ƃɂ��N���A�ł���
	if (data & 0x80) {
		dma[ch].coc = FALSE;
	}
	if (data & 0x40) {
		dma[ch].boc = FALSE;
	}
	if (data & 0x20) {
		dma[ch].ndt = FALSE;
	}
	if (data & 0x10) {
		dma[ch].err = FALSE;
	}
	if (data & 0x04) {
		dma[ch].dit = FALSE;
	}
	if (data & 0x02) {
		dma[ch].pct = FALSE;
	}

	// ���荞�ݏ���
	Interrupt();
}

//---------------------------------------------------------------------------
//
//	CSR�擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::GetCSR(int ch) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

	// �f�[�^�쐬
	data = 0;
	if (dma[ch].coc) {
		data |= 0x80;
	}
	if (dma[ch].boc) {
		data |= 0x40;
	}
	if (dma[ch].ndt) {
		data |= 0x20;
	}
	if (dma[ch].err) {
		data |= 0x10;
	}
	if (dma[ch].act) {
		data |= 0x08;
	}
	if (dma[ch].dit) {
		data |= 0x04;
	}
	if (dma[ch].pct) {
		data |= 0x02;
	}
	if (dma[ch].pcs) {
		data |= 0x01;
	}

	return data;
}

//---------------------------------------------------------------------------
//
//	GCR�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::SetGCR(DWORD data)
{
	int ch;
	DWORD bt;
	DWORD br;

	ASSERT(this);
	ASSERT(data < 0x100);

	// �f�[�^����
	bt = (data >> 2) & 0x03;
	br = data & 0x03;

	// �S�`���l���ɐݒ�
	for (ch=0; ch<4; ch++) {
		dma[ch].bt = bt;
		dma[ch].br = br;
	}
}

//---------------------------------------------------------------------------
//
//	DMA���Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::ResetDMA(int ch)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

	// GCR������
	dma[ch].bt = 0;
	dma[ch].br = 0;

	// DCR������
	dma[ch].xrm = 0;
	dma[ch].dtyp = 0;
	dma[ch].dps = FALSE;
	dma[ch].pcl = 0;

	// OCR������
	dma[ch].dir = FALSE;
	dma[ch].btd = FALSE;
	dma[ch].size = 0;
	dma[ch].chain = 0;
	dma[ch].reqg = 0;

	// SCR������
	dma[ch].mac = 0;
	dma[ch].dac = 0;

	// CCR������
	dma[ch].str = FALSE;
	dma[ch].cnt = FALSE;
	dma[ch].hlt = FALSE;
	dma[ch].sab = FALSE;
	dma[ch].intr = FALSE;

	// CSR������
	dma[ch].coc = FALSE;
	dma[ch].boc = FALSE;
	dma[ch].ndt = FALSE;
	dma[ch].err = FALSE;
	dma[ch].act = FALSE;
	dma[ch].dit = FALSE;
	dma[ch].pct = FALSE;
	if (ch == 0) {
		// FDC��'L'
		dma[ch].pcs = FALSE;
	}
	else {
		// ����ȊO��'H'
		dma[ch].pcs = TRUE;
	}

	// CPR������
	dma[ch].cp = 0;

	// CER������
	dma[ch].ecode = 0;

	// ���荞�݃x�N�^������
	dma[ch].niv = 0x0f;
	dma[ch].eiv = 0x0f;

	// �A�h���X�y�уJ�E���^�̓��Z�b�g���Ȃ�(�f�[�^�V�[�g�ɂ��)
	dma[ch].mar &= 0x00ffffff;
	dma[ch].dar &= 0x00ffffff;
	dma[ch].bar &= 0x00ffffff;
	dma[ch].mtc &= 0x0000ffff;
	dma[ch].btc &= 0x0000ffff;

	// �]���^�C�v�A�J�E���^������
	dma[ch].type = 0;
	dma[ch].startcnt = 0;
	dma[ch].errorcnt = 0;
}

//---------------------------------------------------------------------------
//
//	DMA�X�^�[�g
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::StartDMA(int ch)
{
	int c;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

#if defined(DMAC_LOG)
	LOG1(Log::Normal, "�`���l��%d �X�^�[�g", ch);
#endif	// DMAC_LOG

	// ACT,COC,BOC,NDT,ERR���オ���Ă���΃^�C�~���O�G���[
	if (dma[ch].act || dma[ch].coc || dma[ch].boc || dma[ch].ndt || dma[ch].err) {
#if defined(DMAC_LOG)
		if (dma[ch].act) {
			LOG1(Log::Normal, "�`���l��%d �^�C�~���O�G���[ (ACT)", ch);
		}
		if (dma[ch].coc) {
			LOG1(Log::Normal, "�`���l��%d �^�C�~���O�G���[ (COC)", ch);
		}
		if (dma[ch].boc) {
			LOG1(Log::Normal, "�`���l��%d �^�C�~���O�G���[ (BOC)", ch);
		}
		if (dma[ch].ndt) {
			LOG1(Log::Normal, "�`���l��%d �^�C�~���O�G���[ (NDT)", ch);
		}
		if (dma[ch].err) {
			LOG1(Log::Normal, "�`���l��%d �^�C�~���O�G���[ (ERR)", ch);
		}
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x02);
		return;
	}

	// �`�F�C���Ȃ��̏ꍇ�́AMTC=0�Ȃ烁�����J�E���g�G���[
	if (dma[ch].chain == 0) {
		if (dma[ch].mtc == 0) {
#if defined(DMAC_LOG)
			LOG1(Log::Normal, "�`���l��%d �������J�E���g�G���[", ch);
#endif	// DMAC_LOG
			ErrorDMA(ch, 0x0d);
			return;
		}
	}

	// �A���C�`�F�C���̏ꍇ�́ABTC=0�Ȃ�x�[�X�J�E���g�G���[
	if (dma[ch].chain == 0x02) {
		if (dma[ch].btc == 0) {
#if defined(DMAC_LOG)
			LOG1(Log::Normal, "�`���l��%d �x�[�X�J�E���g�G���[", ch);
#endif	// DMAC_LOG
			ErrorDMA(ch, 0x0f);
			return;
		}
	}

	// �R���t�B�M�����[�V�����G���[�`�F�b�N
	if ((dma[ch].xrm == 0x01) || (dma[ch].mac == 0x03) || (dma[ch].dac == 0x03)
			|| (dma[ch].chain == 0x01)) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "�`���l��%d �R���t�B�O�G���[", ch);
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x01);
		return;
	}

	// �]���^�C�v�쐬
	dma[ch].type = 0;
	if (dma[ch].dps) {
		dma[ch].type += 4;
	}
	dma[ch].type += dma[ch].size;

	// ���[�N������
	dma[ch].str = FALSE;
	dma[ch].act = TRUE;
	dma[ch].cnt = FALSE;
	dma[ch].sab = FALSE;

	// �J�E���g�A�b�v
	dma[ch].startcnt++;

	// �A���C�`�F�C���܂��̓����N�A���C�`�F�C���́A�ŏ��̃u���b�N�����[�h
	if (dma[ch].chain != 0) {
		LoadDMA(ch);
		// ���[�h���ɃA�h���X�G���[�܂��̓o�X�G���[���N������A�G���[�t���O���オ��
		if (dma[ch].err) {
			return;
		}
	}

	// CPU�T�C�N�����N���A���āA���[�h��
	dmactrl.cpu_cycle = 0;
	switch (dma[ch].reqg) {
		// �I�[�g���N�G�X�g����
		case 0:
		// �I�[�g���N�G�X�g�ő�
		case 1:
			// ���݂̎c�肾��DMA�𓮂����āACPU���~�߂�
			dmactrl.current_ch = ch;
			dmactrl.cpu_cycle = 0;
			dmactrl.exec = TRUE;
			scheduler->dma_active = TRUE;
			c = AutoDMA(cpu->GetIOCycle());
			if (c == 0) {
				cpu->Release();
			}
			break;

		// �O���v���]��
		case 2:
			break;

		// �I�[�g���N�G�X�g�{�O���v���]��
		case 3:
			// ���݂̎c�肾��DMA�𓮂����āACPU���~�߂�
			dmactrl.current_ch = ch;
			dmactrl.cpu_cycle = 0;
			dmactrl.exec = TRUE;
			scheduler->dma_active= TRUE;
			dma[ch].reqg = 1;
			c = AutoDMA(cpu->GetIOCycle());
			if (c == 0) {
				cpu->Release();
			}
			dma[ch].reqg = 3;
			break;

		default:
			ASSERT(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	DMA�R���e�B�j���[
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::ContDMA(int ch)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

#if defined(DMAC_LOG)
	LOG1(Log::Normal, "�`���l��%d �R���e�B�j���[", ch);
#endif	// DMAC_LOG

	// ACT���オ���Ă��Ȃ��Ɠ���^�C�~���O�G���[
	if (!dma[ch].act) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "�`���l��%d ����^�C�~���O�G���[(Cont)", ch);
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x02);
		return;
}
	// �`�F�C�����[�h�̏ꍇ�̓R���t�B�O�G���[
	if (dma[ch].chain != 0) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "�`���l��%d �R���t�B�O�G���[", ch);
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x01);
	}
}

//---------------------------------------------------------------------------
//
//	DMA�\�t�g�E�F�A�A�{�[�g
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::AbortDMA(int ch)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

	// ��A�N�e�B�u�Ȃ�G���[�������s��Ȃ�(Marianne.pan)
	if (!dma[ch].act) {
		// �����COC�𗎂Ƃ�(�o���f���[�N)
		dma[ch].coc = FALSE;
		return;
	}

#if defined(DMAC_LOG)
	LOG1(Log::Normal, "�`���l��%d �\�t�g�E�F�A�A�{�[�g", ch);
#endif	// DMAC_LOG

	// �]�������A��A�N�e�B�u
	dma[ch].coc = TRUE;
	dma[ch].act = FALSE;

	// �\�t�g�E�F�A�A�{�[�g�ŃG���[����
	ErrorDMA(ch, 0x11);
}

//---------------------------------------------------------------------------
//
//	DMA���u���b�N�̃��[�h
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::LoadDMA(int ch)
{
	DWORD base;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(dmactrl.load == 0);

	// ���[�h��(ReadWord�ł̃A�h���X�G���[�A�o�X�G���[�ɒ���)
	dmactrl.load = (ch + 1);

	if (dma[ch].bar & 1) {
		// BAR�A�h���X�G���[
		AddrErr(dma[ch].bar, TRUE);

		dmactrl.load = 0;
		return;
	}

	// MAR�ǂݍ���
	dma[ch].bar &= 0xfffffe;
	dma[ch].mar = (memory->ReadWord(dma[ch].bar) & 0x00ff);
	dma[ch].bar += 2;
	dma[ch].mar <<= 16;
	dma[ch].bar &= 0xfffffe;
	dma[ch].mar |= (memory->ReadWord(dma[ch].bar) & 0xffff);
	dma[ch].bar += 2;

	// MTC�ǂݍ���
	dma[ch].bar &= 0xfffffe;
	dma[ch].mtc = (memory->ReadWord(dma[ch].bar) & 0xffff);
	dma[ch].bar += 2;

	if (dma[ch].err) {
		// MAR,MTC�ǂݍ��݃G���[
		dmactrl.load = 0;
		return;
	}

	// �A���C�`�F�C���ł͂����܂�
	if (dma[ch].chain == 0x02) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "�`���l��%d �A���C�`�F�C�����u���b�N", ch);
#endif	// DMAC_LOG
		dma[ch].btc--;
		dmactrl.load = 0;
		return;
	}

	// �����N�A���C�`�F�C��(�ł͎��̃����N�A�h���X��BAR�փ��[�h
#if defined(DMAC_LOG)
	LOG1(Log::Normal, "�`���l��%d �����N�A���C�`�F�C�����u���b�N", ch);
#endif	// DMAC_LOG
	dma[ch].bar &= 0xfffffe;
	base = (memory->ReadWord(dma[ch].bar) & 0x00ff);
	dma[ch].bar += 2;
	base <<= 16;
	dma[ch].bar &= 0xfffffe;
	base |= (memory->ReadWord(dma[ch].bar) & 0xffff);
	dma[ch].bar = base;

	// ���[�h�I��
	dmactrl.load = 0;
}

//---------------------------------------------------------------------------
//
//	DMA�G���[
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::ErrorDMA(int ch, DWORD code)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT((code >= 0x01) && (code <= 17));

#if defined(DMAC_LOG)
	LOG2(Log::Normal, "�`���l��%d �G���[����$%02X", ch, code);
#endif	// DMAC_LOG

	// ACT���~�낷(�t�@�����N�X ADPCM)
	dma[ch].act = FALSE;

	// �G���[�R�[�h����������
	dma[ch].ecode = code;

	// �G���[�t���O�𗧂Ă�
	dma[ch].err = TRUE;

	// �J�E���g�A�b�v
	dma[ch].errorcnt++;

	// ���荞�ݏ���
	Interrupt();
}

//---------------------------------------------------------------------------
//
//	DMA���荞��
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::Interrupt()
{
	DWORD cp;
	int ch;

	ASSERT(this);

	// DMA�Ɠ����D��x�ŏ���(�f�[�^�V�[�g���)
	for (cp=0; cp<=3; cp++) {
		for (ch=0; ch<=3; ch++) {
			// CP�`�F�b�N
			if (cp != dma[ch].cp) {
				continue;
			}

			// �C���^���v�g�C�l�[�u�����`�F�b�N
			if (!dma[ch].intr) {
				continue;
			}

			// ERR�ɂ��EIV�ŏo��
			if (dma[ch].err) {
				if (dmactrl.vector != (int)dma[ch].eiv) {
					// �ʂ̊��荞�݂�v�����Ă���΁A��U�L�����Z��
					if (dmactrl.vector >= 0) {
						cpu->IntCancel(3);
					}
#if defined(DMAC_LOG)
					LOG1(Log::Normal, "�`���l��%d �G���[���荞��", ch);
#endif	// DMAC_LOG
					cpu->Interrupt(3, (BYTE)dma[ch].eiv);
					dmactrl.vector = (int)dma[ch].eiv;
				}
				return;
			}

			// COC,BOC,NDT,PCT(���荞�݃��C���̏ꍇ)��NIV�ŏo��
			if (dma[ch].coc || dma[ch].boc || dma[ch].ndt) {
				if (dmactrl.vector != (int)dma[ch].niv) {
					// �ʂ̊��荞�݂�v�����Ă���΁A��U�L�����Z��
					if (dmactrl.vector >= 0) {
						cpu->IntCancel(3);
					}
#if defined(DMAC_LOG)
					LOG1(Log::Normal, "�`���l��%d �ʏ튄�荞��", ch);
#endif	// DMAC_LOG
					cpu->Interrupt(3, (BYTE)dma[ch].niv);
					dmactrl.vector = (int)dma[ch].niv;
				}
				return;
			}

			if ((dma[ch].pcl == 0x01) && dma[ch].pct) {
				if (dmactrl.vector != (int)dma[ch].niv) {
					// �ʂ̊��荞�݂�v�����Ă���΁A��U�L�����Z��
					if (dmactrl.vector >= 0) {
						cpu->IntCancel(3);
					}
#if defined(DMAC_LOG)
					LOG1(Log::Normal, "�`���l��%d PCL���荞��", ch);
#endif	// DMAC_LOG
					cpu->Interrupt(3, (BYTE)dma[ch].niv);
					dmactrl.vector = (int)dma[ch].niv;
				}
				return;
			}
		}
	}

	// �v�����̊��荞�݂͂Ȃ�
	if (dmactrl.vector >= 0) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "���荞�݃L�����Z�� �x�N�^$%02X", dmactrl.vector);
#endif	// DMAC_LOG

		cpu->IntCancel(3);
		dmactrl.vector = -1;
	}
}

//---------------------------------------------------------------------------
//
//	���荞��ACK
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::IntAck()
{
	ASSERT(this);

	// ���Z�b�g����ɁACPU���犄�荞�݂��Ԉ���ē���ꍇ������
	if (dmactrl.vector < 0) {
		LOG0(Log::Warning, "�v�����Ă��Ȃ����荞��");
		return;
	}

#if defined(DMAC_LOG)
	LOG1(Log::Normal, "���荞�݉��� �x�N�^$%02X", dmactrl.vector);
#endif	// DMAC_LOG

	// �v�����x�N�^�Ȃ�
	dmactrl.vector = -1;
}

//---------------------------------------------------------------------------
//
//	DMA���擾
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::GetDMA(int ch, dma_t *buffer) const
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(buffer);

	// �`���l�����[�N���R�s�[
	*buffer = dma[ch];
}

//---------------------------------------------------------------------------
//
//	DMA������擾
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::GetDMACtrl(dmactrl_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);

	// ���䃏�[�N���R�s�[
	*buffer = dmactrl;
}

//---------------------------------------------------------------------------
//
//	DMA�]������
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::IsDMA() const
{
	ASSERT(this);

	// �]�����t���O(�`���l�����p)�ƁA���[�h�t���O������
	if ((dmactrl.transfer == 0) && (dmactrl.load == 0)) {
		return FALSE;
	}

	// �ǂ��炩�������Ă���
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	DMA�]���\��
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::IsAct(int ch) const
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

	// ACT�łȂ����AERR���AHLT�Ȃ�]���ł��Ȃ�
	if (!dma[ch].act || dma[ch].err || dma[ch].hlt) {
		return FALSE;
	}

	// �]���ł���
	return TRUE;
}

void FASTCALL DMAC::SetLegacyCntMode(BOOL enabled)
{
	ASSERT(this);
	legacy_cnt_mode = enabled ? TRUE : FALSE;
}

BOOL FASTCALL DMAC::IsLegacyCntMode() const
{
	ASSERT(this);
	return legacy_cnt_mode;
}

//---------------------------------------------------------------------------
//
//	DMA�o�X�G���[
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::BusErr(DWORD addr, BOOL read)
{
	ASSERT(this);
	ASSERT(addr <= 0xffffff);

	if (read) {
		LOG1(Log::Warning, "DMA�o�X�G���[(�ǂݍ���) $%06X", addr);
	}
	else {
		LOG1(Log::Warning, "DMA�o�X�G���[(��������) $%06X", addr);
	}

	// ���[�h���̃G���[��
	if (dmactrl.load != 0) {
		// �������E�f�o�C�X�E�x�[�X�̋�ʂ͍l�����Ă��Ȃ�
		ASSERT((dmactrl.load >= 1) && (dmactrl.load <= 4));
		ErrorDMA(dmactrl.load - 1, 0x08);
		return;
	}

	// �������E�f�o�C�X�E�x�[�X�̋�ʂ͍l�����Ă��Ȃ�
	ASSERT((dmactrl.transfer >= 1) && (dmactrl.transfer <= 4));
	ErrorDMA(dmactrl.transfer - 1, 0x08);
}

//---------------------------------------------------------------------------
//
//	DMA�A�h���X�G���[
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::AddrErr(DWORD addr, BOOL read)
{
	ASSERT(this);
	ASSERT(addr <= 0xffffff);

	if (read) {
		LOG1(Log::Warning, "DMA�A�h���X�G���[(�ǂݍ���) $%06X", addr);
	}
	else {
		LOG1(Log::Warning, "DMA�A�h���X�G���[(��������) $%06X", addr);
	}

	// ���[�h���̃G���[��
	if (dmactrl.load != 0) {
		// �������E�f�o�C�X�E�x�[�X�̋�ʂ͍l�����Ă��Ȃ�
		ASSERT((dmactrl.load >= 1) && (dmactrl.load <= 4));
		ErrorDMA(dmactrl.load - 1, 0x0c);
		return;
	}

	// �������E�f�o�C�X�E�x�[�X�̋�ʂ͍l�����Ă��Ȃ�
	ASSERT((dmactrl.transfer >= 1) && (dmactrl.transfer <= 4));
	ErrorDMA(dmactrl.transfer - 1, 0x0c);
}

//---------------------------------------------------------------------------
//
//	�x�N�^�擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::GetVector(int type) const
{
	ASSERT(this);
	ASSERT((type >= 0) && (type < 8));

	// �m�[�}���E�G���[�̃x�N�^�����݂ɏo��
	if (type & 1) {
		// �G���[
		return dma[type >> 1].eiv;
	}
	else {
		// �m�[�}��
		return dma[type >> 1].niv;
	}
}

//---------------------------------------------------------------------------
//
//	DMA�O�����N�G�X�g
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::ReqDMA(int ch)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

	// ACT�łȂ����AERR���AHLT�Ȃ牽�����Ȃ�
	if (!dma[ch].act || dma[ch].err || dma[ch].hlt) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "�`���l��%d �O�����N�G�X�g���s", ch);
#endif	// DMAC_LOG
		return FALSE;
	}

	// DMA�]��
	TransDMA(ch);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�I�[�g���N�G�X�g
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::AutoDMA(DWORD cycle)
{
	int i;
	int ch;
	int mul;
	int remain;
	int used;
	DWORD backup;
	BOOL flag;

	ASSERT(this);

	// �p�����[�^�L��
	remain = (int)cycle;

	// ���s�t���O���オ���Ă��Ȃ���΃I�[�g���N�G�X�g�͖���
	if (!dmactrl.exec) {
		return cycle;
	}

	// ���s�p���t���O�����Z�b�g
	flag = FALSE;

	// �ő呬�x�I�[�g���N�G�X�g�̃`���l�����ɏ���
	for (i=0; i<4; i++) {
		// ����ׂ��`���l��������
		ch = (dmactrl.current_ch + i) & 3;

		// ACT, ERR, HLT�̃`�F�b�N
		if (!dma[ch].act || dma[ch].err || dma[ch].hlt) {
			continue;
		}

		// �ő呬�x�I�[�g���N�G�X�g��
		if (dma[ch].reqg != 1) {
			continue;
		}

		// ���Z���āA�Œ�ł�10�T�C�N���͕K�v�B
		dmactrl.cpu_cycle += cycle;
		if (dmactrl.cpu_cycle < 10) {
			// CPU�͎��s�ł��Ȃ��BDMA�p��
			return 0;
		}

		// 2��ȏ���Z�����Ȃ��A�t���OUP
		cycle = 0;
		flag = TRUE;

		// �X�P�W���[����cycle(�I�[�o�[�T�C�N���v�Z)��ێ����A���Z�b�g
		backup = scheduler->GetCPUCycle();
		scheduler->SetCPUCycle(0);

		// cpu_cycle���}�C�i�X�ɂȂ�܂Ŏ��s�B����Ԃ̓X�P�W���[����蓾��
		while (scheduler->GetCPUCycle() < dmactrl.cpu_cycle) {
			// ACT, ERR, HLT�̃`�F�b�N
			if (!dma[ch].act || dma[ch].err || dma[ch].hlt) {
				break;
			}

			// scheulder->GetCPUCycle()���g���ADMAC����T�C�N�������擾����
			TransDMA(ch);
		}

		// ����T�C�N�������A����
		dmactrl.cpu_cycle -= scheduler->GetCPUCycle();
		remain -= scheduler->GetCPUCycle();
		scheduler->SetCPUCycle(backup);

		// �`���l��������(���E���h���r��)
		dmactrl.current_ch = (dmactrl.current_ch + 1) & 3;

		// ���ׂĎ��Ԃ��g���؂�����
		if (dmactrl.cpu_cycle <= 0) {
			// CPU�͎��s�ł��Ȃ�
			// �����őS�`���l�������������ꍇ�A����AudoDMA�Ńt���O���Ƃ�
			return 0;
		}
	}

	// �ő呬�x�I�[�g���N�G�X�g���Ȃ��������A�������Ď��Ԃ��]����
	// ���葬�x�I�[�g���N�G�X�g�̃`���l��������
	for (i=0; i<4; i++) {
		// ����ׂ��`���l��������
		ch = (dmactrl.current_ch + i) & 3;

		// ACT, ERR, HLT�̃`�F�b�N
		if (!dma[ch].act || dma[ch].err || dma[ch].hlt) {
			continue;
		}

		// �ő呬�x�I�[�g���N�G�X�g�͂��肦�Ȃ�(��̕����ŕK������)
		ASSERT(dma[ch].reqg != 1);

		// ���葬�x�I�[�g���N�G�X�g��
		if (dma[ch].reqg != 0) {
			continue;
		}

		// ���Z���āA�Œ�ł�10�T�C�N���͕K�v�B
		dmactrl.cpu_cycle += cycle;
		if (dmactrl.cpu_cycle < 10) {
			// CPU�͎��s�ł��Ȃ��BDMA�p��
			return 0;
		}

		// 2��ȏ���Z�����Ȃ��A�t���OUP
		cycle = 0;
		flag = TRUE;

		// �X�P�W���[����cycle(�I�[�o�[�T�C�N���v�Z)��ێ����A���Z�b�g
		backup = scheduler->GetCPUCycle();
		scheduler->SetCPUCycle(0);

		// �o�X��L���{�����v�Z(BT=0�Ȃ�2�{�Ȃ�)
		mul = (dma[ch].bt + 1);

		// cpu_cycle���o�X��L�����l�������l�𒴂��Ă��邩
		while ((scheduler->GetCPUCycle() << mul) < dmactrl.cpu_cycle) {
			// ACT, ERR, HLT�̃`�F�b�N
			if (!dma[ch].act || dma[ch].err || dma[ch].hlt) {
				break;
			}

			// �]��
			TransDMA(ch);
		}

		// �g�p�T�C�N�����L��(��Ŏg������)
		used = scheduler->GetCPUCycle();
		scheduler->SetCPUCycle(backup);

		// �`���l��������(���E���h���r��)
		dmactrl.current_ch = (dmactrl.current_ch + 1) & 3;

		// �����ŏI����
		if (dmactrl.cpu_cycle < (used << mul)) {
			// �\�肳��Ă����o�X��L�����g���؂����B�c���CPU�ɕԋp
			dmactrl.cpu_cycle -= used;
			if (used < remain) {
				// �\���]�肪����
				return (remain - used);
			}
			else {
				// �Ȃ����A�g���������BCPU��0
				return 0;
			}
		}

		// �܂��o�X�̎g�p���������̂ŁA���`���l�����܂��
		remain -= used;
	}

	if (!flag) {
		// DMA�͎g��Ȃ������Bdmactrl.exec���~�낷
		dmactrl.exec = FALSE;
		scheduler->dma_active = FALSE;
	}

	return cycle;
}

//---------------------------------------------------------------------------
//
//	DMA1��]��
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::TransDMA(int ch)
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(dmactrl.transfer == 0);

	// �]���t���OON
	dmactrl.transfer = ch + 1;

	// �^�C�v�A�f�B���N�V�����ɉ����ē]��
	switch (dma[ch].type) {
		// 8bit, Pack�o�C�g, 8bit�|�[�g
		case 0:
		// 8bit, Unpack�o�C�g, 8bit�|�[�g
		case 3:
		// 8bit, Unpack�o�C�g, 16bit�|�[�g
		case 7:
			// SCSI�f�B�X�N �x���`�}�[�N(dskbench.x)���
			if (dma[ch].dir) {
				memory->WriteByte(dma[ch].mar, (BYTE)(memory->ReadByte(dma[ch].dar)));
				scheduler->Wait(11);
			}
			else {
				memory->WriteByte(dma[ch].dar, (BYTE)(memory->ReadByte(dma[ch].mar)));
				scheduler->Wait(11);
			}
			break;

		// 8bit, Pack�o�C�g, 16bit�|�[�g(Unpack��葬������:�p���f�B�E�X��!)
		// Wait12:�p���f�B�E�X��!�AWait?:Moon Fighter
		case 4:
			if (dma[ch].dir) {
				memory->WriteByte(dma[ch].mar, (BYTE)(memory->ReadByte(dma[ch].dar)));
				scheduler->Wait(10);
			}
			else {
				memory->WriteByte(dma[ch].dar, (BYTE)(memory->ReadByte(dma[ch].mar)));
				scheduler->Wait(10);
			}
			break;

		// 8bit, ���[�h
		case 1:
			if (dma[ch].dir) {
				data = (BYTE)(memory->ReadByte(dma[ch].dar));
				data <<= 8;
				data |= (BYTE)(memory->ReadByte(dma[ch].dar + 2));
				memory->WriteWord(dma[ch].mar, data);
				scheduler->Wait(19);
			}
			else {
				data = memory->ReadWord(dma[ch].mar);
				memory->WriteByte(dma[ch].dar, (BYTE)(data >> 8));
				memory->WriteByte(dma[ch].dar + 2, (BYTE)data);
				scheduler->Wait(19);
			}
			break;

		// 8bit, �����O���[�h
		case 2:
			if (dma[ch].dir) {
				data = (BYTE)(memory->ReadByte(dma[ch].dar));
				data <<= 8;
				data |= (BYTE)(memory->ReadByte(dma[ch].dar + 2));
				data <<= 8;
				data |= (BYTE)(memory->ReadByte(dma[ch].dar + 4));
				data <<= 8;
				data |= (BYTE)(memory->ReadByte(dma[ch].dar + 6));
				memory->WriteWord(dma[ch].mar, (WORD)(data >> 16));
				memory->WriteWord(dma[ch].mar + 2, (WORD)data);
				scheduler->Wait(38);
			}
			else {
				data = memory->ReadWord(dma[ch].mar);
				data <<= 16;
				data |= (WORD)(memory->ReadWord(dma[ch].mar + 2));
				memory->WriteByte(dma[ch].dar, (BYTE)(data >> 24));
				memory->WriteByte(dma[ch].dar + 2, (BYTE)(data >> 16));
				memory->WriteByte(dma[ch].dar + 4, (BYTE)(data >> 8));
				memory->WriteByte(dma[ch].dar + 6, (BYTE)data);
				scheduler->Wait(38);
			}
			break;

		// 16bit, ���[�h
		case 5:
			// ���܂�x�������FM�������荞�݂��Ђ�������(�O���f�B�E�XII)
			if (dma[ch].dir) {
				data = memory->ReadWord(dma[ch].dar);
				memory->WriteWord(dma[ch].mar, (WORD)data);
				scheduler->Wait(10);
			}
			else {
				data = memory->ReadWord(dma[ch].mar);
				memory->WriteWord(dma[ch].dar, (WORD)data);
				scheduler->Wait(10);
			}
			break;

		// 16bit, �����O���[�h
		case 6:
			if (dma[ch].dir) {
				data = memory->ReadWord(dma[ch].dar);
				data <<= 16;
				data |= (WORD)(memory->ReadWord(dma[ch].dar + 2));
				memory->WriteWord(dma[ch].mar, (WORD)(data >> 16));
				memory->WriteWord(dma[ch].mar + 2, (WORD)data);
				scheduler->Wait(20);
			}
			else {
				data = memory->ReadWord(dma[ch].mar);
				data <<= 16;
				data |= (WORD)memory->ReadWord(dma[ch].mar + 2);
				memory->WriteWord(dma[ch].dar, (WORD)(data >> 16));
				memory->WriteWord(dma[ch].dar + 2, (WORD)data);
				scheduler->Wait(20);
			}
			break;

		// ����ȊO
		default:
			ASSERT(FALSE);
	}

	// �]���t���OOFF
	dmactrl.transfer = 0;

	// �]���G���[�̃`�F�b�N(�o�X�G���[�y�уA�h���X�G���[)
	if (dma[ch].err) {
		// �A�h���X�X�V�O�ɔ�����(�f�[�^�V�[�g�ɂ��)
		return FALSE;
	}

	// �A�h���X�X�V(12bit�ɐ���:Racing Champ)
	dma[ch].mar += MemDiffTable[ dma[ch].type ][ dma[ch].mac ];
	dma[ch].mar &= 0xffffff;
	dma[ch].dar += DevDiffTable[ dma[ch].type ][ dma[ch].dac ];
	dma[ch].dar &= 0xffffff;

	// �������J�E���g���f�N�������g
	dma[ch].mtc--;
	if (dma[ch].mtc > 0) {
		// �I��钼�O��DONE���A�T�[�g��FDC�̂�TC��ݒ�(DCII)
		if ((ch == 0) && (dma[ch].mtc == 1)) {
			fdc->SetTC();
		}
		return TRUE;
	}

	// �R���e�B�j���[�̏���
	if (dma[ch].cnt) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "�`���l��%d �R���e�B�j���[���u���b�N", ch);
#endif	// DMAC_LOG

		// BOC���グ��
		dma[ch].boc = TRUE;
		Interrupt();

		if (legacy_cnt_mode) {
			// Legacy XM6 behavior: keep CNT latched and reload BAR/BTC every block.
			dma[ch].mar = dma[ch].bar;
			dma[ch].mfc = dma[ch].bfc;
			dma[ch].mtc = dma[ch].btc;
			return TRUE;
		}

		// HD63450/PX68k behavior: CNT is one-shot continuous transfer.
		if ((dma[ch].bar != 0) && (dma[ch].btc != 0)) {
			dma[ch].mar = dma[ch].bar;
			dma[ch].mfc = dma[ch].bfc;
			dma[ch].mtc = dma[ch].btc;
			dma[ch].bar = 0;
			dma[ch].btc = 0;
			dma[ch].cnt = FALSE;
			return TRUE;
		}

		dma[ch].cnt = FALSE;
	}

	// �A���C�`�F�C���̏���
	if (dma[ch].chain == 0x02) {
		if (dma[ch].btc > 0) {
			// ���̃u���b�N������
			LoadDMA(ch);
			return TRUE;
		}
	}

	// �����N�A���C�`�F�C���̏���
	if (dma[ch].chain == 0x03) {
		if (dma[ch].bar != 0) {
			// ���̃u���b�N������
			LoadDMA(ch);
			return TRUE;
		}
	}

	// DMA����
#if defined(DMAC_LOG)
	LOG1(Log::Normal, "�`���l��%d DMA����", ch);
#endif	// DMAC_LOG

	// �t���O�ݒ�A���荞��
	dma[ch].act = FALSE;
	dma[ch].coc = TRUE;
	Interrupt();

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	�������A�h���X�X�V�e�[�u��
//
//---------------------------------------------------------------------------
const int DMAC::MemDiffTable[8][4] = {
	{ 0, 1, -1, 0},		// 8bit, �o�C�g
	{ 0, 2, -2, 0},		// 8bit, ���[�h
	{ 0, 4, -4, 0},		// 8bit, �����O���[�h
	{ 0, 1, -1, 0},		// 8bit, �p�b�N�o�C�g
	{ 0, 1, -1, 0},		// 16bit, �o�C�g
	{ 0, 2, -2, 0},		// 16bit, ���[�h
	{ 0, 4, -4, 0},		// 16bit, �����O���[�h
	{ 0, 1, -1, 0}		// 16bit, �p�b�N�o�C�g
};

//---------------------------------------------------------------------------
//
//	�f�o�C�X�A�h���X�X�V�e�[�u��
//
//---------------------------------------------------------------------------
const int DMAC::DevDiffTable[8][4] = {
	{ 0, 2, -2, 0},		// 8bit, �o�C�g
	{ 0, 4, -4, 0},		// 8bit, ���[�h
	{ 0, 8, -8, 0},		// 8bit, �����O���[�h
	{ 0, 2, -2, 0},		// 8bit, �p�b�N�o�C�g
	{ 0, 1, -1, 0},		// 16bit, �o�C�g
	{ 0, 2, -2, 0},		// 16bit, ���[�h
	{ 0, 4, -4, 0},		// 16bit, �����O���[�h
	{ 0, 1, -1, 0}		// 16bit, �p�b�N�o�C�g
};


