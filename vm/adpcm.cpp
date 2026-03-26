//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[ ADPCM (MSM6258V) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "dmac.h"
#include "fileio.h"
#include "config.h"
#include "adpcm.h"

//===========================================================================
//
//	ADPCM
//
//===========================================================================
//#define ADPCM_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
ADPCM::ADPCM(VM *p) : MemDevice(p)
{
	// Device ID
	dev.id = MAKEID('A', 'P', 'C', 'M');
	dev.desc = "ADPCM (MSM6258V)";

	// Memory map
	memdev.first = 0xe92000;
	memdev.last = 0xe93fff;

	// Reset any extra state the constructor does not cover
	memset(&adpcm, 0, sizeof(adpcm));
	adpcm.sync_rate = 882;
	adpcm.sync_step = 0x9c4000 / 882;
	adpcm.vol = 0;
	adpcm.enable = FALSE;
	adpcm.sound = TRUE;
	dmac = NULL;
	adpcmbuf = NULL;
	quirk_arianshuu_loop_fix = FALSE;
	quirk_stuck_l = 0;
	quirk_stuck_r = 0;
	memset(&diag, 0, sizeof(diag));
}

//	Initialization
//
//	������
//
//---------------------------------------------------------------------------
BOOL FASTCALL ADPCM::Init()
{
	ASSERT(this);

	// Base class initialization
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Locate the DMAC
	ASSERT(!dmac);
	dmac = (DMAC*)vm->SearchDevice(MAKEID('D', 'M', 'A', 'C'));
	ASSERT(dmac);

	// Allocate the buffer
	try {
		adpcmbuf = new DWORD[ BufMax * 2 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!adpcmbuf) {
		return FALSE;
	}
	memset(adpcmbuf, 0, sizeof(DWORD) * (BufMax * 2));

	// Create the sampling event
	event.SetDevice(this);
	event.SetDesc("Sampling");
	event.SetUser(0);
	event.SetTime(0);
	scheduler->AddEvent(&event);

	// Reset interpolation parameters
	adpcm.interp = FALSE;

	// Reset the default table and runtime state
	adpcm.ratio = 0;
	adpcm.speed = 0x400;
	adpcm.clock = 8;

	// Build the lookup table and apply the default settings
	MakeTable();
	SetVolume(52);

	// Initialize the audio buffer
	InitBuf(adpcm.sync_rate * 50);

	// Recalculate speed parameters
	CalcSpeed();

	return TRUE;
}

//	Cleanup
//
//	�N���[���A�b�v
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Cleanup()
{
	ASSERT(this);

	// Free the audio buffer
	if (adpcmbuf) {
		delete[] adpcmbuf;
		adpcmbuf = NULL;
	}

	// ��{�N���X��
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	���Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Reset()
{
	ASSERT(this);
	ASSERT_DIAG();

	LOG0(Log::Normal, "���Z�b�g");

	// �������[�N������
	adpcm.play = FALSE;
	adpcm.rec = FALSE;
	adpcm.active = FALSE;
	adpcm.started = FALSE;
	adpcm.panpot = 0;
	adpcm.ratio = 0;
	adpcm.speed = 0x400;
	adpcm.data = 0;
	adpcm.offset = 0;
	adpcm.out = 0;
	adpcm.sample = 0;
	adpcm.clock = 8;
	CalcSpeed();
	quirk_stuck_l = 0;
	quirk_stuck_r = 0;

	// �o�b�t�@������
	InitBuf(adpcm.sync_rate * 50);

	// �C�x���g���~�߂�
	event.SetUser(0);
	event.SetTime(0);
	event.SetDesc("Sampling");
}

//---------------------------------------------------------------------------
//
//	�Z�[�u
//
//---------------------------------------------------------------------------
BOOL FASTCALL ADPCM::Save(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	ASSERT_DIAG();

	LOG0(Log::Normal, "�Z�[�u");

	// �T�C�Y���Z�[�u
	sz = sizeof(adpcm_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// ���̂��Z�[�u
	if (!fio->Write(&adpcm, (int)sz)) {
		return FALSE;
	}

	// �C�x���g���Z�[�u
	if (!event.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	���[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL ADPCM::Load(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	ASSERT_DIAG();

	LOG0(Log::Normal, "���[�h");

	// �T�C�Y�����[�h�A�ƍ�
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(adpcm_t)) {
		return FALSE;
	}

	// ���̂����[�h
	if (!fio->Read(&adpcm, (int)sz)) {
		return FALSE;
	}

	// �C�x���g�����[�h
	if (!event.Load(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�ݒ�K�p
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	ASSERT_DIAG();

	LOG0(Log::Normal, "�ݒ�K�p");

	// ���`���
	adpcm.interp = config->adpcm_interp;
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	�f�f
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::AssertDiag() const
{
	// ��{�N���X
	MemDevice::AssertDiag();

	ASSERT(this);
	ASSERT(GetID() == MAKEID('A', 'P', 'C', 'M'));
	ASSERT(memdev.first == 0xe92000);
	ASSERT(memdev.last == 0xe93fff);
	ASSERT(dmac);
	ASSERT(dmac->GetID() == MAKEID('D', 'M', 'A', 'C'));
	ASSERT((adpcm.panpot >= 0) && (adpcm.panpot <= 3));
	ASSERT((adpcm.play == TRUE) || (adpcm.play == FALSE));
	ASSERT((adpcm.rec == TRUE) || (adpcm.rec == FALSE));
	ASSERT((adpcm.active == TRUE) || (adpcm.active == FALSE));
	ASSERT((adpcm.started == TRUE) || (adpcm.started == FALSE));
	ASSERT((adpcm.clock == 4) || (adpcm.clock == 8));
	ASSERT((adpcm.ratio == 0) || (adpcm.ratio == 1) || (adpcm.ratio == 2));
	ASSERT((adpcm.speed & 0x007f) == 0);
	ASSERT((adpcm.speed >= 0x0100) && (adpcm.speed <= 0x400));
	ASSERT(adpcm.data < 0x100);
	ASSERT((adpcm.offset >= 0) && (adpcm.offset <= 48));
	ASSERT((adpcm.enable == TRUE) || (adpcm.enable == FALSE));
	ASSERT((adpcm.sound == TRUE) || (adpcm.sound == FALSE));
	ASSERT(adpcm.readpoint < BufMax);
	ASSERT(adpcm.writepoint < BufMax);
	ASSERT(adpcm.number <= BufMax);
	ASSERT(adpcm.sync_cnt < 0x4000);
	ASSERT((adpcm.interp == TRUE)  || (adpcm.interp == FALSE));
	ASSERT(adpcmbuf);
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	�o�C�g�ǂݍ���
//
//---------------------------------------------------------------------------
DWORD FASTCALL ADPCM::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT_DIAG();

	// ��A�h���X�̂݃f�R�[�h����Ă���
	if ((addr & 1) != 0) {
		// 4�o�C�g�P�ʂŃ��[�v
		addr &= 3;

		// �E�F�C�g
		scheduler->Wait(1);

		// �A�h���X�U�蕪��
		if (addr == 3) {
			// �f�[�^���W�X�^
			if (adpcm.rec && adpcm.active) {
				// �^���f�[�^�Ƃ���0x80��Ԃ�
				return 0x80;
			}
			return adpcm.data;
		}

		// �X�e�[�^�X���W�X�^
		if (adpcm.play) {
			if (quirk_arianshuu_loop_fix) {
				return 0xc0;
			}
			// �Đ����A�܂��͍Đ�������
			return 0x7f;
		}
		else {
			if (quirk_arianshuu_loop_fix) {
				return 0x40;
			}
			// �^�����[�h�A�܂��͍Đ��w���Ȃ�
			return 0xff;
		}
	}

	// �����A�h���X�̓f�R�[�h����Ă��Ȃ�
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	���[�h�ǂݍ���
//
//---------------------------------------------------------------------------
DWORD FASTCALL ADPCM::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT_DIAG();

	return (0xff00 | ReadByte(addr + 1));
}

//---------------------------------------------------------------------------
//
//	�o�C�g��������
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);
	ASSERT_DIAG();

	// ��A�h���X�̂݃f�R�[�h����Ă���
	if ((addr & 1) != 0) {
		// 4�o�C�g�P�ʂŃ��[�v
		addr &= 3;

		// �E�F�C�g
		scheduler->Wait(1);

		// �A�h���X�U�蕪��
		if (addr == 3) {
			// �f�[�^���W�X�^
			adpcm.data = data;
			return;
		}

#if defined(ADPCM_LOG)
		LOG1(Log::Normal, "ADPCM�R�}���h $%02X", data);
#endif	// ADPCM_LOG

		// �R�}���h���W�X�^
		if (quirk_arianshuu_loop_fix) {
			// PCM8A/MCDRV互換: STOPビット優先（0x03 を STOP として扱う）
			if (data & 1) {
				Stop();
				return;
			}
			if (data & 2) {
				// PCM8A互換: STARTは再生中でも境界リセットを行う
				adpcm.offset = 0;
				adpcm.sample = 0;
				adpcm.out = 0;
				adpcm.sync_cnt = 0;
				adpcm.number = 0;
				adpcm.readpoint = adpcm.writepoint;
				quirk_stuck_l = 0;
				quirk_stuck_r = 0;
				if (adpcmbuf) {
					const DWORD right = (adpcm.readpoint + 1) & (BufMax - 1);
					adpcmbuf[adpcm.readpoint] = 0;
					adpcmbuf[right] = 0;
				}

				if (!adpcm.play || !adpcm.active) {
					adpcm.play = TRUE;
					Start(0);
				}
				return;
			}
			if (data & 4) {
				adpcm.number = 0;
				adpcm.readpoint = adpcm.writepoint;
				if (!adpcm.rec || !adpcm.active) {
					adpcm.rec = TRUE;
					Start(1);
				}
				return;
			}
			return;
		}

		if (data & 1) {
			// �����~
			Stop();
		}
		if (data & 2) {
			// �Đ��X�^�[�g
			adpcm.play = TRUE;
			Start(0);
			return;
		}
		if (data & 4) {
			// �^���X�^�[�g
			adpcm.rec = TRUE;
			Start(1);
			return;
		}
		return;
	}

	// �����A�h���X�̓f�R�[�h����Ă��Ȃ�
}

//---------------------------------------------------------------------------
//
//	���[�h��������
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);
	ASSERT_DIAG();

	WriteByte(addr + 1, (BYTE)data);
}

//---------------------------------------------------------------------------
//
//	�ǂݍ��݂̂�
//
//---------------------------------------------------------------------------
DWORD FASTCALL ADPCM::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT_DIAG();

	// ��A�h���X�̂݃f�R�[�h����Ă���
	if (addr & 1) {
		// 4�o�C�g�P�ʂŃ��[�v
		addr &= 3;

		// �A�h���X�U�蕪��
		if (addr == 3) {
			// �f�[�^���W�X�^
			if (adpcm.rec && adpcm.active) {
				return 0x80;
			}
			return adpcm.data;
		}

		// �X�e�[�^�X���W�X�^
		if (adpcm.play) {
			if (quirk_arianshuu_loop_fix) {
				return 0xc0;
			}
			// �Đ����A�܂��͍Đ�������
			return 0x7f;
		}
		else {
			if (quirk_arianshuu_loop_fix) {
				return 0x40;
			}
			// �^�����[�h�A�܂��͍Đ��w���Ȃ�
			return 0xff;
		}
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	�����f�[�^�擾
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::GetADPCM(adpcm_t *buffer)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT_DIAG();

	// �����f�[�^���R�s�[
	*buffer = adpcm;
}

//---------------------------------------------------------------------------
//
//	�C�x���g�R�[���o�b�N
//
//---------------------------------------------------------------------------
BOOL FASTCALL ADPCM::Callback(Event *ev)
{
	BOOL valid;
	DWORD num;
	char string[64];

	ASSERT(this);
	ASSERT(ev);
	ASSERT_DIAG();

	// �E�F�C�g������΍��������A���̎��Ԃ܂ň������΂�
	if (adpcm.wait <= 0) {
		while (adpcm.wait <= 0) {
			// �C���A�N�e�B�u�܂���ReqDMA���s�̏ꍇ�A80�Ƃ���
			adpcm.data = 0x80;
			valid = FALSE;
			diag.req_total++;

			// 1��̃C�x���g��1�o�C�g(2�T���v��)�̏������s��
			if (adpcm.active) {
				if (dmac->ReqDMA(3)) {
					// DMA�]������
					valid = TRUE;
					diag.req_ok++;
				}
				else {
					diag.req_fail++;
				}
			}
			else {
				diag.req_fail++;
			}
			diag.last_data = adpcm.data;

			// �Đ���
			if (ev->GetUser() == 0) {
				// 0x88,0x80,0x00�ȊO�̓X�^�[�g�t���OON
				if ((adpcm.data != 0x88) && (adpcm.data != 0x80) && (adpcm.data != 0x00)) {
#if defined(ADPCM_LOG)
					if (!adpcm.started) {
						LOG0(Log::Normal, "����L���f�[�^���o");
					}
#endif	// ADPCM_LOG
					adpcm.started = TRUE;
				}

				// ADPCM��PCM�ϊ��A�o�b�t�@�փX�g�A
				num = adpcm.speed;
				num >>= 7;
				ASSERT((num >= 2) && (num <= 16));

				Decode((int)adpcm.data, num, valid);
				Decode((int)(adpcm.data >> 4), num, valid);
			}
			adpcm.wait++;
		}

		// �E�F�C�g�����Z�b�g
		adpcm.wait = 0;

		// ���x�ύX�ɑΉ�
		if (ev->GetTime() == adpcm.speed) {
			return TRUE;
		}
		sprintf(string, "Sampling %dHz", (2 * 1000 * 1000) / adpcm.speed);
		ev->SetDesc(string);
		ev->SetTime(adpcm.speed);
		return TRUE;
	}

	// �E�F�C�g�����炷
	adpcm.wait--;

	// ���x�ύX�ɑΉ�
	if (ev->GetTime() == adpcm.speed) {
		return TRUE;
	}
	sprintf(string, "Sampling %dHz", (2 * 1000 * 1000) / adpcm.speed);
	ev->SetDesc(string);
	ev->SetTime(adpcm.speed);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	��N���b�N�w��
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::SetClock(DWORD clk)
{
	ASSERT(this);
	ASSERT((clk == 4) || (clk == 8));
	ASSERT_DIAG();

#if defined(ADPCM_LOG)
	LOG1(Log::Normal, "�N���b�N %dMHz", clk);
#endif	// ADPCM_LOG

	// ���x���Čv�Z
	adpcm.clock = clk;
	CalcSpeed();
}

//---------------------------------------------------------------------------
//
//	�N���b�N�䗦�w��
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::SetRatio(DWORD ratio)
{
	ASSERT(this);
	ASSERT((ratio >= 0) || (ratio <= 3));
	ASSERT_DIAG();

#if defined(ADPCM_LOG)
	LOG1(Log::Normal, "���x�䗦 %d", ratio);
#endif	// ADPCM_LOG

	// ratio=3��2�Ƃ݂Ȃ�
	if (ratio == 3) {
		LOG0(Log::Warning, "����`���[�g�w�� $03");
		ratio = 2;
	}

	// ���x���Čv�Z
	if (adpcm.ratio != ratio) {
		adpcm.ratio = ratio;
		CalcSpeed();
	}
}

//---------------------------------------------------------------------------
//
//	�p���|�b�g�w��
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::SetPanpot(DWORD panpot)
{
	ASSERT(this);
	ASSERT((panpot >= 0) || (panpot <= 3));
	ASSERT_DIAG();

#if defined(ADPCM_LOG)
	LOG1(Log::Normal, "�p���|�b�g�w�� %d", panpot);
#endif	// ADPCM_LOG

	adpcm.panpot = panpot;
}

//---------------------------------------------------------------------------
//
//	���x�Čv�Z
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::CalcSpeed()
{
	ASSERT(this);

	// ���������珇�ɁA2, 3, 4������
	adpcm.speed = 2 - adpcm.ratio;
	ASSERT(adpcm.speed <= 2);
	adpcm.speed += 2;

	// �N���b�N4MHz�Ȃ�256�A8MHz�Ȃ�128���悸��
	adpcm.speed <<= 7;
	if (adpcm.clock == 4) {
		adpcm.speed <<= 1;
	}
}

//---------------------------------------------------------------------------
//
//	�^���E�Đ��J�n
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Start(int type)
{
	char string[64];

	ASSERT(this);
	ASSERT((type == 0) || (type == 1));
	ASSERT_DIAG();
	diag.start_events++;

#if defined(ADPCM_LOG)
	LOG1(Log::Normal, "�Đ��J�n %d", type);
#endif	// ADPCM_LOG

	// �A�N�e�B�u�t���O���グ��
	adpcm.active = TRUE;

	// �f�[�^��������
	adpcm.offset = 0;

	// �C�x���g��ݒ�
	event.SetUser(type);

	// �����ŕK�����Ԑݒ�(���@�Ƃ͈قȂ�\�������邪�AFM�����Ƃ̓�����D��)
	sprintf(string, "Sampling %dHz", (2 * 1000 * 1000) / adpcm.speed);
	event.SetDesc(string);
	event.SetTime(adpcm.speed);

	// ����̃C�x���g�������Ŏ��s(FM�����Ƃ̓������Ƃ���ʑ[�u)
	Callback(&event);
}

//---------------------------------------------------------------------------
//
//	�^���E�Đ���~
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Stop()
{
	ASSERT(this);
	ASSERT_DIAG();
	diag.stop_events++;

#if defined(ADPCM_LOG)
	LOG0(Log::Normal, "��~");
#endif	// ADPCM_LOG

	// �t���O���~�낷
	adpcm.active = FALSE;
	adpcm.play = FALSE;
	adpcm.rec = FALSE;

	if (quirk_arianshuu_loop_fix) {
		adpcm.number = 0;
		adpcm.readpoint = adpcm.writepoint;
		quirk_stuck_l = 0;
		quirk_stuck_r = 0;
		if (adpcmbuf) {
			const DWORD right = (adpcm.readpoint + 1) & (BufMax - 1);
			adpcmbuf[adpcm.readpoint] = 0;
			adpcmbuf[right] = 0;
		}
	}
}

//---------------------------------------------------------------------------
//
//	�ψʃe�[�u��
//
//---------------------------------------------------------------------------
const int ADPCM::NextTable[] = {
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8
};

//---------------------------------------------------------------------------
//
//	�I�t�Z�b�g�e�[�u��
//
//---------------------------------------------------------------------------
const int ADPCM::OffsetTable[] = {
	 0,
	 0,  1,  2,  3,  4,  5,  6,  7,
	 8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47,
	48, 48, 48, 48, 48, 48, 48, 48,
	48
};

//---------------------------------------------------------------------------
//
//	�f�R�[�h
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Decode(int data, int num, BOOL valid)
{
	int i;
	int store;
	int sample;
	int diff;
	int base;

	ASSERT(this);
	ASSERT((data >= 0) && (data < 0x100));
	ASSERT(num >= 2);
	ASSERT_DIAG();
	diag.decode_calls++;

	// �f�B�Z�[�u���Ȃ牽�����Ȃ�
	if (!adpcm.enable) {
		return;
	}

	// �f�[�^���}�X�N
	data &= 0x0f;

	// �����e�[�u�����瓾��
	i = adpcm.offset << 4;
	i |= data;
	sample = DiffTable[i];

	// ���̃I�t�Z�b�g�����߂Ă���
	adpcm.offset += NextTable[data];
	adpcm.offset = OffsetTable[adpcm.offset + 1];

	// �X�g�A�f�[�^�����Z
	store = (sample << 8) + (adpcm.sample * 245);
	store >>= 8;

	// ���ʏ����{�l�L��
	base = adpcm.sample;
	base *= adpcm.vol;
	base >>= 9;
	adpcm.sample = store;
	store *= adpcm.vol;
	store >>= 9;
	adpcm.out = store;

	// �L���ȃf�[�^�����Ă��Ȃ��Ȃ�A�T���v�����グ��
	if (!valid) {
		if (adpcm.sample < 0) {
			adpcm.sample++;
		}
		if (adpcm.sample > 0) {
			adpcm.sample--;
		}
	}

	// �u�s�[�v���̏���
	if ((adpcm.sample == 0) || (adpcm.sample == -1) || (adpcm.sample == 1)) {
		store = 0;
	}

	// �����𓾂�
	diff = store - base;

	// ���ʂ��l�����āAnum�����T���v�����X�g�A
	switch (adpcm.panpot) {
		// ���E�Ƃ��o��
		case 0:
			for (i=0; i<num; i++) {
				store = base + (diff * (i + 1)) / num;
				adpcmbuf[adpcm.writepoint++] = store;
				adpcmbuf[adpcm.writepoint++] = store;
				adpcm.writepoint &= (BufMax - 1);
			}
			break;

		// ���̂ݏo��
		case 1:
			for (i=0; i<num; i++) {
				store = base + (diff * (i + 1)) / num;
				adpcmbuf[adpcm.writepoint++] = store;
				adpcmbuf[adpcm.writepoint++] = 0;
				adpcm.writepoint &= (BufMax - 1);
			}
			break;

		// �E�̂ݏo��
		case 2:
			for (i=0; i<num; i++) {
				store = base + (diff * (i + 1)) / num;
				adpcmbuf[adpcm.writepoint++] = 0;
				adpcmbuf[adpcm.writepoint++] = store;
				adpcm.writepoint &= (BufMax - 1);
			}
			break;

		// �����I�t
		case 3:
			for (i=0; i<num; i++) {
				adpcmbuf[adpcm.writepoint++] = 0;
				adpcmbuf[adpcm.writepoint++] = 0;
				adpcm.writepoint &= (BufMax - 1);
			}
			break;

		// �ʏ�A���蓾�Ȃ�
		default:
			ASSERT(FALSE);
	}

	// �����X�V
	adpcm.number += (num << 1);
	if (adpcm.number >= BufMax) {
#if defined(ADPCM_LOG)
		LOG0(Log::Warning, "ADPCM�o�b�t�@ �I�[�o�[����");
#endif	// ADPCM_LOG
		adpcm.number = BufMax;
		adpcm.readpoint = adpcm.writepoint;
	}

	if (adpcm.number > diag.max_buffer_samples) {
		diag.max_buffer_samples = adpcm.number;
	}
}

//---------------------------------------------------------------------------
//
//	�����C�l�[�u��
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Enable(BOOL enable)
{
	ASSERT(this);
	ASSERT_DIAG();

	adpcm.enable = enable;
}

//---------------------------------------------------------------------------
//
//	�o�b�t�@������
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::InitBuf(DWORD rate)
{
	ASSERT(this);
	ASSERT(rate > 0);
	ASSERT((rate % 100) == 0);

#if defined(ADPCM_LOG)
	LOG0(Log::Normal, "�o�b�t�@������");
#endif	// ADPCM_LOG

	// �J�E���^�A�|�C���^
	adpcm.number = 0;
	adpcm.readpoint = 0;
	adpcm.writepoint = 0;
	adpcm.wait = 0;
	adpcm.sync_cnt = 0;
	adpcm.sync_rate = rate / 50;
	adpcm.sync_step = 0x9c4000 / adpcm.sync_rate;
	quirk_stuck_l = 0;
	quirk_stuck_r = 0;

	// �ŏ��̃f�[�^��0���Z�b�g
	if (adpcmbuf) {
		adpcmbuf[0] = 0;
		adpcmbuf[1] = 0;
	}
}

void FASTCALL ADPCM::SetArianshuuLoopFix(BOOL enabled)
{
	quirk_arianshuu_loop_fix = enabled ? TRUE : FALSE;
	quirk_stuck_l = 0;
	quirk_stuck_r = 0;
}

void FASTCALL ADPCM::GetDiag(adpcm_diag_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);

	if (!buffer) {
		return;
	}

	*buffer = diag;
}

//---------------------------------------------------------------------------
//
//	�o�b�t�@����f�[�^�擾
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::GetBuf(DWORD *buffer, int samples)
{
	int i;
	int j;
	int l;
	int r;
	int *ptr;
	DWORD point;

	ASSERT(this);
	ASSERT(buffer);
	ASSERT(samples >= 0);
	ASSERT_DIAG();

	// �����A�܂��̓`���l���J�b�g�̏ꍇ�́A�o�b�t�@�N���A
	if (!adpcm.enable || !adpcm.sound) {
		ASSERT(adpcm.sync_rate != 0);
		InitBuf(adpcm.sync_rate * 50);
		return;
	}

	// ������
	ptr = (int*)buffer;

	// �o�b�t�@�ɉ����Ȃ��Ƃ��́A�Ō�̃f�[�^�𑱂��ē����
	if (adpcm.number <= 2) {
		diag.underrun_head_events++;
		l = adpcmbuf[adpcm.readpoint];
		r = adpcmbuf[adpcm.readpoint + 1];
		if ((l > 8) || (l < -8) || (r > 8) || (r < -8)) {
			diag.stale_nonzero_events++;
		}

		if (quirk_arianshuu_loop_fix) {
			diag.silence_fill_events++;
			for (i=samples; i>0; i--) {
				*ptr += 0;
				ptr++;
				*ptr += 0;
				ptr++;
			}
			quirk_stuck_l = 0;
			quirk_stuck_r = 0;
			adpcmbuf[adpcm.readpoint] = 0;
			adpcmbuf[adpcm.readpoint + 1] = 0;
			return;
		}

		quirk_stuck_l = l;
		quirk_stuck_r = r;
		for (i=samples; i>0; i--) {
			*ptr += l;
			ptr++;
			*ptr += r;
			ptr++;
		}
		return;
	}

	// ���`��Ԃ̗L���ŕ�����
	if (adpcm.interp) {

		// ��Ԃ���
		for (i=samples; i>0; i--) {
			// �X�e�b�vUp
			adpcm.sync_cnt += adpcm.sync_step;
			if (adpcm.sync_cnt >= 0x4000) {
				// ��������
				adpcm.sync_cnt &= 0x3fff;

				// ����
				if (adpcm.number >= 4) {
					adpcm.readpoint += 2;
					adpcm.readpoint &= (BufMax - 1);
					adpcm.number -= 2;
				}
			}

			// ���̃f�[�^�����邩
			if (adpcm.number < 4 ) {
				diag.underrun_interp_events++;
				if (quirk_arianshuu_loop_fix) {
					diag.silence_fill_events++;
					*ptr += 0;
					ptr++;
					*ptr += 0;
					ptr++;
					quirk_stuck_l = 0;
					quirk_stuck_r = 0;
					continue;
				}

				// ���̃f�[�^���Ȃ��̂ŁA���̃f�[�^�����̂܂ܓ����
				*ptr += adpcmbuf[adpcm.readpoint];
				ptr++;
				*ptr += adpcmbuf[adpcm.readpoint + 1];
				ptr++;
			}
			else {
				// ���̃|�C���g�����炤
				point = adpcm.readpoint + 2;
				point &= (BufMax - 1);

				// ���f�[�^�Ǝ��̃f�[�^�ŕ��[L]
				l = adpcmbuf[point] * (int)adpcm.sync_cnt;
				r = adpcmbuf[adpcm.readpoint + 0] * (int)(0x4000 - adpcm.sync_cnt);
				*ptr += ((l + r) >> 14);
				ptr++;

				// ���f�[�^�Ǝ��̃f�[�^�ŕ��[R]
				l = adpcmbuf[point + 1] * (int)adpcm.sync_cnt;
				r = adpcmbuf[adpcm.readpoint + 1] * (int)(0x4000 - adpcm.sync_cnt);
				*ptr += ((l + r) >> 14);
				ptr++;
			}
		}
	}
	else {
		// ��ԂȂ�
		for (i=samples; i>0; i--) {
			// ���݂̈ʒu����f�[�^���i�[
			*buffer += adpcmbuf[adpcm.readpoint];
			buffer++;
			*buffer += adpcmbuf[adpcm.readpoint + 1];
			buffer++;

			// sync_step�����Z
			adpcm.sync_cnt += adpcm.sync_step;

			// 0x4000�Ɠ�������
			if (adpcm.sync_cnt < 0x4000) {
				continue;
			}
			adpcm.sync_cnt &= 0x3fff;

			// ����ADPCM�T���v���ֈړ�
			if (adpcm.number <= 2) {
				diag.underrun_linear_events++;
				if (quirk_arianshuu_loop_fix) {
					diag.silence_fill_events++;
					for (j=i-1; j>0; j--) {
						*buffer += 0;
						buffer++;
						*buffer += 0;
						buffer++;
						adpcm.sync_cnt += adpcm.sync_step;
					}
					adpcm.sync_cnt &= 0x3fff;
					quirk_stuck_l = 0;
					quirk_stuck_r = 0;
					adpcmbuf[adpcm.readpoint] = 0;
					adpcmbuf[adpcm.readpoint + 1] = 0;
					return;
				}

				// �Ō�̃f�[�^�𑱂��ē����
				l = adpcmbuf[adpcm.readpoint];
				r = adpcmbuf[adpcm.readpoint + 1];
				for (j=i-1; j>0; j--) {
					*buffer += l;
					buffer++;
					*buffer += r;
					buffer++;
					adpcm.sync_cnt += adpcm.sync_step;
				}
				adpcm.sync_cnt &= 0x3fff;
				return;
			}
			adpcm.readpoint += 2;
			adpcm.readpoint &= (BufMax - 1);
			adpcm.number -= 2;
		}
	}
}

//---------------------------------------------------------------------------
//
//	�E�F�C�g��������
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Wait(int num)
{
	int i;

	ASSERT(this);
	ASSERT_DIAG();

	// �C�x���g�J�n���ĂȂ����0
	if (event.GetTime() == 0) {
		adpcm.wait = 0;
		return;
	}

	// ���Ȃ����OPM�̕��������Ă���
	if ((int)adpcm.number <= num) {
		// ����������B������1/4���E�F�C�g�Ƃ���
		num -= (adpcm.number);
		num >>= 2;

		// �v�Z�́��Ɠ��l�B�������]
		i = adpcm.speed >> 6;
		i *= adpcm.sync_rate;
		adpcm.wait = -((625 * num) / i);

#if defined(ADPCM_LOG)
		if (adpcm.wait != 0) {
			LOG1(Log::Normal, "�E�F�C�g�ݒ� %d", adpcm.wait);
		}
#endif	// ADPCM_LOG
		return;
	}

	// ����������B������1/4���E�F�C�g�Ƃ���
	num = adpcm.number - num;
	num >>= 2;

	// 44.1k,48k etc.�ł̗]��T���v������x�Ƃ����
	// �E�F�C�g�񐔂� (625 * x) / (adpcm.sync_rate * (adpcm.speed >> 6))
	i = adpcm.speed >> 6;
	i *= adpcm.sync_rate;
	adpcm.wait = (625 * num) / i;

#if defined(ADPCM_LOG)
	if (adpcm.wait != 0) {
		LOG1(Log::Normal, "�E�F�C�g�ݒ� %d", adpcm.wait);
	}
#endif	// ADPCM_LOG
}

//---------------------------------------------------------------------------
//
//	�e�[�u���쐬
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::MakeTable()
{
	int i;
	int j;
	int base;
	int diff;
	int *p;

	ASSERT(this);

	// �e�[�u���쐬(floor�Ŋۂ߂�����panic���ŗǂ����ʂ�������)
	p = DiffTable;
	for (i=0; i<49; i++) {
		base = (int)floor(16.0 * pow(1.1, i));

		// ���Z�����ׂ�int�ōs��
		for (j=0; j<16; j++) {
			diff = 0;
			if (j & 4) {
				diff += base;
			}
			if (j & 2) {
				diff += (base >> 1);
			}
			if (j & 1) {
				diff += (base >> 2);
			}
			diff += (base >> 3);
			if (j & 8) {
				diff = -diff;
			}

			*p++ = diff;
		}
	}
}

//---------------------------------------------------------------------------
//
//	���ʐݒ�
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::SetVolume(int volume)
{
	double offset;
	double vol;

	ASSERT(this);
	ASSERT((volume >= 0) && (volume < 100));

	// 16384 * 10^((volume-140) / 200)���Z�o�A�Z�b�g
	offset = (double)(volume - 140);
	offset /= (double)200.0;
	vol = pow((double)10.0, offset);
	vol *= (double)16384.0;
	adpcm.vol = int(vol);
}
