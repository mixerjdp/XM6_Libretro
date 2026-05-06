//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ �r�f�I�R���g���[��(CATHY & VIPS) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "fileio.h"
#include "render.h"
#include "renderin.h"
#include "vc.h"

//===========================================================================
//
//	�r�f�I�R���g���[��
//
//===========================================================================
//#define VC_LOG

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
VC::VC(VM *p) : MemDevice(p)
{
	// �f�o�C�XID��������
	dev.id = MAKEID('V', 'C', ' ', ' ');
	dev.desc = "VC (CATHY & VIPS)";

	// �J�n�A�h���X�A�I���A�h���X
	memdev.first = 0xe82000;
	memdev.last = 0xe83fff;

	// ���̑�
	render = NULL;
}

//---------------------------------------------------------------------------
//
//	������
//
//---------------------------------------------------------------------------
BOOL FASTCALL VC::Init()
{
	ASSERT(this);

	// ��{�N���X
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// �����_���擾
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// �p���b�g���[�N���N���A
	memset(palette, 0, sizeof(palette));

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�N���[���A�b�v
//
//---------------------------------------------------------------------------
void FASTCALL VC::Cleanup()
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
void FASTCALL VC::Reset()
{
	ASSERT(this);
	LOG0(Log::Normal, "���Z�b�g");

	// �r�f�I���[�N���N���A
	memset(&vc, 0, sizeof(vc));

	// �L���p���[�N���m���ɔ��]������
	vc.vr1h = 0xff;
	vc.vr1l = 0xff;
	vc.vr2h = 0xff;
	vc.vr2l = 0xff;
}

//---------------------------------------------------------------------------
//
//	�Z�[�u
//
//---------------------------------------------------------------------------
BOOL FASTCALL VC::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "�Z�[�u");

	// �T�C�Y���Z�[�u
	sz = sizeof(vc_t);
	if (!fio->Write(&sz, (int)sizeof(sz))) {
		return FALSE;
	}

	// ���̂��Z�[�u
	if (!fio->Write(&vc, (int)sz)) {
		return FALSE;
	}

	// �p���b�g���Z�[�u
	if (!fio->Write(palette, sizeof(palette))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	���[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL VC::Load(Fileio *fio, int /*ver*/)
{
	size_t sz;
	DWORD addr;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "���[�h");

	// �T�C�Y�����[�h�A�ƍ�
	if (!fio->Read(&sz, (int)sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(vc_t)) {
		return FALSE;
	}

	// ���̂����[�h
	if (!fio->Read(&vc, (int)sz)) {
		return FALSE;
	}

	// �p���b�g�����[�h
	if (!fio->Read(palette, sizeof(palette))) {
		return FALSE;
	}

	// �����_���֒ʒm
	render->SetVC();
	for (addr=0; addr<0x200; addr++) {
		render->SetPalette(addr);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�ݒ�K�p
//
//---------------------------------------------------------------------------
void FASTCALL VC::ApplyCfg(const Config *config)
{
	ASSERT(config);
	printf("%p", (const void*)config);
	LOG0(Log::Normal, "�ݒ�K�p");
}

//---------------------------------------------------------------------------
//
//	�o�C�g�ǂݍ���
//
//---------------------------------------------------------------------------
DWORD FASTCALL VC::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// $1000�P�ʂŃ��[�v
	addr &= 0xfff;

	if (render && (render->GetCompositorMode() == Render::compositor_fast)) {
		if (addr < 0x400) {
			scheduler->Wait(1);
		}
		return render->VCtrlRead(memdev.first + addr);
	}

	// �f�R�[�h
	if (addr < 0x400) {
		// �p���b�g�G���A
		scheduler->Wait(1);
		addr ^= 1;
		return palette[addr];
	}

	// �r�f�I�R���g���[�����W�X�^
	if (addr < 0x500) {
		if (addr & 1) {
			return (BYTE)GetVR0();
		}
		else {
			return (GetVR0() >> 8);
		}
	}
	if (addr < 0x600) {
		if (addr & 1) {
			return (BYTE)GetVR1();
		}
		else {
			return (GetVR1() >> 8);
		}
	}
	if (addr < 0x700) {
		if (addr & 1) {
			return (BYTE)GetVR2();
		}
		else {
			return (GetVR2() >> 8);
		}
	}

	// �f�R�[�h����Ă��Ȃ��G���A��0
	return 0;
}

//---------------------------------------------------------------------------
//
//	���[�h�ǂݍ���
//
//---------------------------------------------------------------------------
DWORD FASTCALL VC::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);

	// $1000�P�ʂŃ��[�v
	addr &= 0xfff;

	// �f�R�[�h
	if (addr < 0x400) {
		// �p���b�g
		scheduler->Wait(1);
		return *(WORD *)(&palette[addr]);
	}

	// �r�f�I�R���g���[�����W�X�^
	if (addr < 0x500) {
		return GetVR0();
	}
	if (addr < 0x600) {
		return GetVR1();
	}
	if (addr < 0x700) {
		return GetVR2();
	}

	// �f�R�[�h����Ă��Ȃ��G���A��0
	return 0;
}

//---------------------------------------------------------------------------
//
//	�o�C�g��������
//
//---------------------------------------------------------------------------
void FASTCALL VC::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);

#if defined(VC_LOG)
	if ((addr & 0xfff) >= 0x400) {
		LOG2(Log::Normal, "VC�������� %08X <- %02X", addr, data);
	}
#endif	// VC_LOG

	// $1000�P�ʂŃ��[�v
	addr &= 0xfff;

	// �f�R�[�h
	if (addr < 0x400) {
		// �p���b�g�G���A
		scheduler->Wait(1);
		addr ^= 1;

		// ��r
		if (palette[addr] != data) {
			palette[addr] = (BYTE)data;

			// �����_���֒ʒm
			render->SetPalette(addr >> 1);
		}
		if (render && (render->GetCompositorMode() == Render::compositor_fast)) {
			render->VCtrlWrite(memdev.first + addr, (BYTE)data);
		}
		return;
	}

	// �r�f�I�R���g���[�����W�X�^
	if (addr < 0x500) {
		if (addr & 1) {
			SetVR0L(data);
		}
		if (render && (render->GetCompositorMode() == Render::compositor_fast)) {
			render->VCtrlWrite(memdev.first + addr, (BYTE)data);
		}
		return;
	}
	if (addr < 0x600) {
		if (addr & 1) {
			SetVR1L(data);
		}
		else {
			SetVR1H(data);
		}
		if (render && (render->GetCompositorMode() == Render::compositor_fast)) {
			render->VCtrlWrite(memdev.first + addr, (BYTE)data);
		}
		return;
	}
	if (addr < 0x700) {
		if (addr & 1) {
			SetVR2L(data);
		}
		else {
			SetVR2H(data);
		}
		if (render && (render->GetCompositorMode() == Render::compositor_fast)) {
			render->VCtrlWrite(memdev.first + addr, (BYTE)data);
		}
		return;
	}

	// ����ȊO�̓f�R�[�h����Ă��Ȃ�
}

//---------------------------------------------------------------------------
//
//	���[�h��������
//
//---------------------------------------------------------------------------
void FASTCALL VC::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

#if defined(VC_LOG)
	if ((addr & 0xfff) >= 0x400) {
		LOG2(Log::Normal, "VC�������� %08X <- %04X", addr, data);
	}
#endif	// VC_LOG

	// $1000�P�ʂŃ��[�v
	addr &= 0xfff;

	// �f�R�[�h
	if (addr < 0x400) {
		// �p���b�g�G���A
		scheduler->Wait(1);

		// ��r
		if (data != *(WORD*)(&palette[addr])) {
			*(WORD *)(&palette[addr]) = (WORD)data;

			// �����_���֒ʒm
			render->SetPalette(addr >> 1);
		}
		return;
	}

	// �r�f�I�R���g���[�����W�X�^
	if (addr < 0x500) {
		SetVR0L((BYTE)data);
		return;
	}
	if (addr < 0x600) {
		SetVR1L((BYTE)data);
		SetVR1H(data >> 8);
		return;
	}
	if (addr < 0x700) {
		SetVR2L((BYTE)data);
		SetVR2H(data >> 8);
		return;
	}

	// ����ȊO�̓f�R�[�h����Ă��Ȃ�
}

//---------------------------------------------------------------------------
//
//	�ǂݍ��݂̂�
//
//---------------------------------------------------------------------------
DWORD FASTCALL VC::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// $1000�P�ʂŃ��[�v
	addr &= 0xfff;

	// �f�R�[�h
	if (addr < 0x400) {
		// �p���b�g�G���A
		addr ^= 1;
		return palette[addr];
	}

	// �r�f�I�R���g���[�����W�X�^
	if (addr < 0x500) {
		if (addr & 1) {
			return (BYTE)GetVR0();
		}
		else {
			return (GetVR0() >> 8);
		}
	}
	if (addr < 0x600) {
		if (addr & 1) {
			return (BYTE)GetVR1();
		}
		else {
			return (GetVR1() >> 8);
		}
	}
	if (addr < 0x700) {
		if (addr & 1) {
			return (BYTE)GetVR2();
		}
		else {
			return (GetVR2() >> 8);
		}
	}

	// �f�R�[�h����Ă��Ȃ��G���A��0
	return 0;
}

//---------------------------------------------------------------------------
//
//	�����f�[�^�擾
//
//---------------------------------------------------------------------------
void FASTCALL VC::GetVC(vc_t *buffer)
{
	ASSERT(this);
	ASSERT(buffer);

	// �������[�N���R�s�[
	*buffer = vc;
}

//---------------------------------------------------------------------------
//
//	�r�f�I���W�X�^0(L)�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL VC::SetVR0L(DWORD data)
{
	BOOL siz;
	DWORD col;

	ASSERT(this);
	ASSERT(data < 0x100);

	// �L��
	siz = vc.siz;
	col = vc.col;

	// �ݒ�
	if (data & 4) {
		vc.siz = TRUE;
	}
	else {
		vc.siz = FALSE;
	}
	vc.col = (data & 3);

	// ��r
	if ((vc.siz != siz) || (vc.col != col)) {
		render->SetVC();
	}
}

//---------------------------------------------------------------------------
//
//	�r�f�I���W�X�^0�擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL VC::GetVR0() const
{
	DWORD data;

	ASSERT(this);

	data = 0;
	if (vc.siz) {
		data |= 0x04;
	}
	data |= vc.col;

	return data;
}

//---------------------------------------------------------------------------
//
//	�r�f�I���W�X�^1(H)�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL VC::SetVR1H(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	data &= 0x3f;

	// ��r
	if (vc.vr1h == data) {
		return;
	}
	vc.vr1h = data;

	vc.gr = (data & 3);
	data >>= 2;
	vc.tx = (data & 3);
	data >>= 2;
	vc.sp = data;

	// �ʒm
	render->SetVC();
}

//---------------------------------------------------------------------------
//
//	�r�f�I���W�X�^1(L)�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL VC::SetVR1L(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	// ��r
	if (vc.vr1l == data) {
		return;
	}
	vc.vr1l = data;

	vc.gp[0] = (data & 3);
	data >>= 2;
	vc.gp[1] = (data & 3);
	data >>= 2;
	vc.gp[2] = (data & 3);
	data >>= 2;
	vc.gp[3] = (data & 3);

	// �ʒm
	render->SetVC();
}

//---------------------------------------------------------------------------
//
//	�r�f�I���W�X�^1�擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL VC::GetVR1() const
{
	DWORD data;

	ASSERT(this);

	data = vc.sp;
	data <<= 2;
	data |= vc.tx;
	data <<= 2;
	data |= vc.gr;
	data <<= 2;
	data |= vc.gp[3];
	data <<= 2;
	data |= vc.gp[2];
	data <<= 2;
	data |= vc.gp[1];
	data <<= 2;
	data |= vc.gp[0];

	return data;
}

//---------------------------------------------------------------------------
//
//	�r�f�I���W�X�^2(H)�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL VC::SetVR2H(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	// �f�[�^��r
	if (vc.vr2h == data) {
		return;
	}
	vc.vr2h = data;

	// YS
	if (data & 0x80) {
		vc.ys = TRUE;
	}
	else {
		vc.ys = FALSE;
	}

	// AH
	if (data & 0x40) {
		vc.ah = TRUE;
	}
	else {
		vc.ah = FALSE;
	}

	// VHT
	if (data & 0x20) {
		vc.vht = TRUE;
	}
	else {
		vc.vht = FALSE;
	}

	// EXON
	if (data & 0x10) {
		vc.exon = TRUE;
	}
	else {
		vc.exon = FALSE;
	}

	// H/P
	if (data & 0x08) {
		vc.hp = TRUE;
	}
	else {
		vc.hp = FALSE;
	}

	// B/P
	if (data & 0x04) {
		vc.bp = TRUE;
	}
	else {
		vc.bp = FALSE;
	}

	// G/G
	if (data & 0x02) {
		vc.gg = TRUE;
	}
	else {
		vc.gg = FALSE;
	}

	// G/T
	if (data & 0x01) {
		vc.gt = TRUE;
	}
	else {
		vc.gt = FALSE;
	}

	// �ʒm
	render->SetVC();
}

//---------------------------------------------------------------------------
//
//	�r�f�I���W�X�^2(L)�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL VC::SetVR2L(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	// ��r
	if (vc.vr2l == data) {
		return;
	}
	vc.vr2l = data;

	// BCON
	if (data & 0x80) {
		vc.bcon = TRUE;
	}
	else {
		vc.bcon = FALSE;
	}

	// SON
	if (data & 0x40) {
		vc.son = TRUE;
	}
	else {
		vc.son = FALSE;
	}

	// TON
	if (data & 0x20) {
		vc.ton = TRUE;
	}
	else {
		vc.ton = FALSE;
	}

	// GON
	if (data & 0x10) {
		vc.gon = TRUE;
	}
	else {
		vc.gon = FALSE;
	}

	// GS[3]
	if (data & 0x08) {
		vc.gs[3] = TRUE;
	}
	else {
		vc.gs[3] = FALSE;
	}

	// GS[2]
	if (data & 0x04) {
		vc.gs[2] = TRUE;
	}
	else {
		vc.gs[2] = FALSE;
	}

	// GS[1]
	if (data & 0x02) {
		vc.gs[1] = TRUE;
	}
	else {
		vc.gs[1] = FALSE;
	}

	// GS[0]
	if (data & 0x01) {
		vc.gs[0] = TRUE;
	}
	else {
		vc.gs[0] = FALSE;
	}

	// �ʒm
	render->SetVC();
}

//---------------------------------------------------------------------------
//
//	�r�f�I���W�X�^2�擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL VC::GetVR2() const
{
	DWORD data;

	ASSERT(this);

	data = 0;
	if (vc.ys) {
		data |= 0x8000;
	}
	if (vc.ah) {
		data |= 0x4000;
	}
	if (vc.vht) {
		data |= 0x2000;
	}
	if (vc.exon) {
		data |= 0x1000;
	}
	if (vc.hp) {
		data |= 0x0800;
	}
	if (vc.bp) {
		data |= 0x0400;
	}
	if (vc.gg) {
		data |= 0x0200;
	}
	if (vc.gt) {
		data |= 0x0100;
	}
	if (vc.bcon) {
		data |= 0x0080;
	}
	if (vc.son) {
		data |= 0x0040;
	}
	if (vc.ton) {
		data |= 0x0020;
	}
	if (vc.gon) {
		data |= 0x0010;
	}
	if (vc.gs[3]) {
		data |= 0x0008;
	}
	if (vc.gs[2]) {
		data |= 0x0004;
	}
	if (vc.gs[1]) {
		data |= 0x0002;
	}
	if (vc.gs[0]) {
		data |= 0x0001;
	}

	return data;
}
