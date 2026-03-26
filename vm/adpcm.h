//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[ ADPCM (MSM6258V) ]
//
//---------------------------------------------------------------------------

#if !defined(adpcm_h)
#define adpcm_h

#include "device.h"
#include "event.h"

//===========================================================================
//
//	ADPCM
//
//===========================================================================
class ADPCM : public MemDevice
{
public:
	// Sample data definition
	typedef struct {
		DWORD panpot;					// �p���|�b�g
		BOOL play;						// �Đ����[�h
		BOOL rec;						// �^�����[�h
		BOOL active;					// �A�N�e�B�u�t���O
		BOOL started;					// �Đ���L�ׂȃf�[�^�����o
		DWORD clock;					// �����N���b�N(4 or 8)
		DWORD ratio;					// �N���b�N�䗦 (0 or 1 or 2)
		DWORD speed;					// �i�s���x(128,192,256,384,512)
		DWORD data;						// �T���v���f�[�^(4bit * 2sample)

		int offset;						// �����I�t�Z�b�g (0-48)
		int sample;						// �T���v���f�[�^
		int out;						// �o�̓f�[�^
		int vol;						// ����

		BOOL enable;					// �C�l�[�u���t���O
		BOOL sound;						// ADPCM�o�͗L���t���O
		DWORD readpoint;				// �o�b�t�@�ǂݍ��݃|�C���g
		DWORD writepoint;				// �o�b�t�@�������݃|�C���g
		DWORD number;					// �o�b�t�@�L���f�[�^��
		int wait;						// �����E�F�C�g
		DWORD sync_cnt;					// �����J�E���^
		DWORD sync_rate;				// �������[�g(882,960,etc...)
		DWORD sync_step;				// �����X�e�b�v(���`��ԑΉ�)
		BOOL interp;					// ��ԃt���O
	} adpcm_t;

	typedef struct {
		DWORD start_events;
		DWORD stop_events;
		DWORD req_total;
		DWORD req_ok;
		DWORD req_fail;
		DWORD decode_calls;
		DWORD underrun_head_events;
		DWORD underrun_interp_events;
		DWORD underrun_linear_events;
		DWORD silence_fill_events;
		DWORD stale_nonzero_events;
		DWORD max_buffer_samples;
		DWORD last_data;
	} adpcm_diag_t;

public:
	// ��{�t�@���N�V����
	ADPCM(VM *p);
										// �R���X�g���N�^
	BOOL FASTCALL Init();
										// ������
	void FASTCALL Cleanup();
										// �N���[���A�b�v
	void FASTCALL Reset();
										// ���Z�b�g
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// �Z�[�u
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// ���[�h
	void FASTCALL ApplyCfg(const Config *config);
										// �ݒ�K�p
#if !defined(NDEBUG)
	void FASTCALL AssertDiag() const;
										// �f�f
#endif	// NDEBUG

	// �������f�o�C�X
	DWORD FASTCALL ReadByte(DWORD addr);
										// �o�C�g�ǂݍ���
	DWORD FASTCALL ReadWord(DWORD addr);
										// ���[�h�ǂݍ���
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// �o�C�g��������
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// ���[�h��������
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// �ǂݍ��݂̂�

	// �O��API
	void FASTCALL GetADPCM(adpcm_t *buffer);
										// �����f�[�^�擾
	BOOL FASTCALL Callback(Event *ev);
										// �C�x���g�R�[���o�b�N
	void FASTCALL SetClock(DWORD clk);
										// ��N���b�N�w��
	void FASTCALL SetRatio(DWORD ratio);
										// �N���b�N�䗦�w��
	void FASTCALL SetPanpot(DWORD pan);
										// �p���|�b�g�w��
	void FASTCALL Enable(BOOL enable);
										// �����C�l�[�u��
	void FASTCALL InitBuf(DWORD rate);
										// �o�b�t�@������
	void FASTCALL GetBuf(DWORD *buffer, int samples);
										// �o�b�t�@�擾
	void FASTCALL Wait(int num);
										// �E�F�C�g�w��
	void FASTCALL EnableADPCM(BOOL flag) { adpcm.sound = flag; }
										// �Đ��L��
	void FASTCALL SetVolume(int volume);
										// ���ʐݒ�
	void FASTCALL SetArianshuuLoopFix(BOOL enabled);
										// Arianshuu: evita hold infinito al vaciar ADPCM
	void FASTCALL GetDiag(adpcm_diag_t *buffer) const;
										// Telemetria interna ADPCM
	BOOL FASTCALL IsArianshuuLoopFixEnabled() const { return quirk_arianshuu_loop_fix; }
										// State del quirk Arianshuu
	void FASTCALL ClrStarted()			{ adpcm.started = FALSE; }
										// �X�^�[�g�t���O�N���A
	BOOL FASTCALL IsStarted() const		{ return adpcm.started; }
										// �X�^�[�g�t���O�擾

private:
	enum {
		BufMax = 0x10000				// �o�b�t�@�T�C�Y
	};
	void FASTCALL MakeTable();
										// �e�[�u���쐬
	void FASTCALL CalcSpeed();
										// ���x�Čv�Z
	void FASTCALL Start(int type);
										// �^���E�Đ��X�^�[�g
	void FASTCALL Stop();
										// �^���E�Đ��X�g�b�v
	void FASTCALL Decode(int data, int num, BOOL valid);
										// 4bit�f�R�[�h
	Event event;
										// �^�C�}�[�C�x���g
	adpcm_t adpcm;
										// �����f�[�^
	DMAC *dmac;
										// DMAC
	DWORD *adpcmbuf;
										// �����o�b�t�@
	int DiffTable[49 * 16];
										// �����e�[�u��
	static const int NextTable[16];
										// �ψʃe�[�u��
	static const int OffsetTable[58];
										// �I�t�Z�b�g�e�[�u��
	BOOL quirk_arianshuu_loop_fix;
	int quirk_stuck_l;
	int quirk_stuck_r;
	adpcm_diag_t diag;
};

#endif	// adpcm_h
