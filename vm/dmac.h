//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ DMAC(HD63450) ]
//
//---------------------------------------------------------------------------

#if !defined(dmac_h)
#define dmac_h

#include "device.h"

//===========================================================================
//
//	DMAC
//
//===========================================================================
class DMAC : public MemDevice
{
public:
	// �����f�[�^��`(�`���l����)
	typedef struct {
		// ��{�p�����[�^
		DWORD xrm;						// ���N�G�X�g���[�h
		DWORD dtyp;						// �f�o�C�X�^�C�v
		BOOL dps;						// �|�[�g�T�C�Y (TRUE��16bit)
		DWORD pcl;						// PCL�Z���N�^
		BOOL dir;						// ���� (TRUE��DAR��������)
		BOOL btd;						// DONE�Ŏ��u���b�N��
		DWORD size;						// �I�y�����h�T�C�Y
		DWORD chain;					// �`�F�C������
		DWORD reqg;						// REQ�������[�h
		DWORD mac;						// �������A�h���X�X�V���[�h
		DWORD dac;						// �f�o�C�X�A�h���X�X�V���[�h

		// ����t���O
		BOOL str;						// �X�^�[�g�t���O
		BOOL cnt;						// �R���e�B�j���[�t���O
		BOOL hlt;						// HALT�t���O
		BOOL sab;						// �\�t�g�E�F�A�A�{�[�g�t���O
		BOOL intr;						// ���荞�݉\�t���O
		BOOL coc;						// �`�����l�����슮���t���O
		BOOL boc;						// �u���b�N���슮���t���O
		BOOL ndt;						// ����I���t���O
		BOOL err;						// �G���[�t���O
		BOOL act;						// �A�N�e�B�u�t���O
		BOOL dit;						// DONE���̓t���O
		BOOL pct;						// PCL negedge���o�t���O
		BOOL pcs;						// PCL�̏�� (TRUE��H���x��)
		DWORD ecode;					// �G���[�R�[�h

		// �A�h���X�A�����O�X
		DWORD mar;						// �������A�h���X�J�E���^
		DWORD dar;						// �f�o�C�X�A�h���X���W�X�^
		DWORD bar;						// �x�[�X�A�h���X���W�X�^
		DWORD mtc;						// �������g�����X�t�@�J�E���^
		DWORD btc;						// �x�[�X�g�����X�t�@�J�E���^
		DWORD mfc;						// �������t�@���N�V�����R�[�h
		DWORD dfc;						// �f�o�C�X�t�@���N�V�����R�[�h
		DWORD bfc;						// �x�[�X�t�@���N�V�����R�[�h
		DWORD niv;						// �m�[�}���C���^���v�g�x�N�^
		DWORD eiv;						// �G���[�C���^���v�g�x�N�^

		// �o�[�X�g�]��
		DWORD cp;						// �v���C�I���e�B
		DWORD bt;						// �o�[�X�g�]���^�C��
		DWORD br;						// �o���h��
		int type;						// �]���^�C�v

		// ����J�E���^(�f�o�b�O����)
		DWORD startcnt;					// �X�^�[�g�J�E���^
		DWORD errorcnt;					// �G���[�J�E���^
	} dma_t;

	// �����f�[�^��`(�O���[�o��)
	typedef struct {
		int transfer;					// �]�����t���O(�`���l�����p)
		int load;						// �`�F�C�����[�h�t���O(�`���l�����p)
		BOOL exec;						// �I�[�g���N�G�X�g�L���t���O
		int current_ch;					// �I�[�g���N�G�X�g�����`���l��
		int cpu_cycle;					// CPU�T�C�N���J�E���^
		int vector;						// ���荞�ݗv�����x�N�^
	} dmactrl_t;

public:
	// ��{�t�@���N�V����
	DMAC(VM *p);
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

	// �������f�o�C�X
	DWORD FASTCALL ReadByte(DWORD addr);
										// �o�C�g�ǂݍ���
	DWORD FASTCALL ReadWord(DWORD addr);
										// ���[�h�ǂݍ���
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// �o�C�g��������
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// ���[�h�ǂݍ���
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// �ǂݍ��݂̂�

	// �O��API
	void FASTCALL GetDMA(int ch, dma_t *buffer) const;
										// DMA���擾
	void FASTCALL GetDMACtrl(dmactrl_t *buffer) const;
										// DMA������擾
	BOOL FASTCALL ReqDMA(int ch);
										// DMA�]���v��
	DWORD FASTCALL AutoDMA(DWORD cycle);
										// DMA�I�[�g���N�G�X�g
	BOOL FASTCALL IsDMA() const;
										// DMA�]�������₢���킹
	void FASTCALL BusErr(DWORD addr, BOOL read);
										// �o�X�G���[
	void FASTCALL AddrErr(DWORD addr, BOOL read);
										// �A�h���X�G���[
	DWORD FASTCALL GetVector(int type) const;
										// �x�N�^�擾
	void FASTCALL IntAck();
										// ���荞��ACK
	BOOL FASTCALL IsAct(int ch) const;
										// DMA�]���\���₢���킹
	void FASTCALL SetLegacyCntMode(BOOL enabled);
	BOOL FASTCALL IsLegacyCntMode() const;

private:
	// �`���l���������A�N�Z�X
	DWORD FASTCALL ReadDMA(int ch, DWORD addr) const;
										// DMA�ǂݍ���
	void FASTCALL WriteDMA(int ch, DWORD addr, DWORD data);
										// DMA��������
	void FASTCALL SetDCR(int ch, DWORD data);
										// DCR�Z�b�g
	DWORD FASTCALL GetDCR(int ch) const;
										// DCR�擾
	void FASTCALL SetOCR(int ch, DWORD data);
										// OCR�Z�b�g
	DWORD FASTCALL GetOCR(int ch) const;
										// OCR�擾
	void FASTCALL SetSCR(int ch, DWORD data);
										// SCR�Z�b�g
	DWORD FASTCALL GetSCR(int ch) const;
										// SCR�擾
	void FASTCALL SetCCR(int ch, DWORD data);
										// CCR�Z�b�g
	DWORD FASTCALL GetCCR(int ch) const;
										// CCR�擾
	void FASTCALL SetCSR(int ch, DWORD data);
										// CSR�Z�b�g
	DWORD FASTCALL GetCSR(int ch) const;
										// CSR�擾
	void FASTCALL SetGCR(DWORD data);
										// GCR�Z�b�g

	// �`���l���I�y���[�V����
	void FASTCALL ResetDMA(int ch);
										// DMA���Z�b�g
	void FASTCALL StartDMA(int ch);
										// DMA�X�^�[�g
	void FASTCALL ContDMA(int ch);
										// DMA�R���e�B�j���[
	void FASTCALL AbortDMA(int ch);
										// DMA�\�t�g�E�F�A�A�{�[�g
	void FASTCALL LoadDMA(int ch);
										// DMA�u���b�N���[�h
	void FASTCALL ErrorDMA(int ch, DWORD code);
										// �G���[
	void FASTCALL Interrupt();
										// ���荞��
	BOOL FASTCALL TransDMA(int ch);
										// DMA1��]��

	// �e�[�u���A�������[�N
	static const int MemDiffTable[8][4];
										// �������X�V�e�[�u��
	static const int DevDiffTable[8][4];
										// �f�o�C�X�X�V�e�[�u��
	Memory *memory;
										// ������
	FDC *fdc;
										// FDC
	dma_t dma[4];
										// �������[�N(�`���l��)
	dmactrl_t dmactrl;
										// �������[�N(�O���[�o��)
	BOOL legacy_cnt_mode;
};

#endif	// dmac_h
