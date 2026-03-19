//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ �f�B�X�N ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "fileio.h"
#include "vm.h"
#include "sasi.h"
#include "disk.h"

//===========================================================================
//
//	�f�B�X�N�g���b�N
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
DiskTrack::DiskTrack(int track, int size, int sectors, BOOL raw)
{
	ASSERT(track >= 0);
	ASSERT((size == 8) || (size == 9) || (size == 11));
	ASSERT((sectors > 0) && (sectors <= 0x100));

	// �p�����[�^��ݒ�
	dt.track = track;
	dt.size = size;
	dt.sectors = sectors;
	dt.raw = raw;

	// ����������Ă��Ȃ�(���[�h����K�v����)
	dt.init = FALSE;

	// �ύX����Ă��Ȃ�
	dt.changed = FALSE;

	// ���I���[�N�͑��݂��Ȃ�
	dt.buffer = NULL;
	dt.changemap = NULL;
}

//---------------------------------------------------------------------------
//
//	�f�X�g���N�^
//
//---------------------------------------------------------------------------
DiskTrack::~DiskTrack()
{
	// ����������͍s�����A�����Z�[�u�͂��Ȃ�
	if (dt.buffer) {
		delete[] dt.buffer;
		dt.buffer = NULL;
	}
	if (dt.changemap) {
		delete[] dt.changemap;
		dt.changemap = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	���[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskTrack::Load(const Filepath& path)
{
	Fileio fio;
	DWORD offset;
	int i;
	int length;

	ASSERT(this);

	// ���Ƀ��[�h����Ă���Εs�v
	if (dt.init) {
		ASSERT(dt.buffer);
		ASSERT(dt.changemap);
		return TRUE;
	}

	ASSERT(!dt.buffer);
	ASSERT(!dt.changemap);

	// �I�t�Z�b�g���v�Z(����ȑO�̃g���b�N��256�Z�N�^�ێ��Ƃ݂Ȃ�)
	offset = (dt.track << 8);
	if (dt.raw) {
		ASSERT(dt.size == 11);
		offset *= 0x930;
		offset += 0x10;
	}
	else {
		offset <<= dt.size;
	}

	// �����O�X���v�Z(���̃g���b�N�̃f�[�^�T�C�Y)
	length = dt.sectors << dt.size;

	// �o�b�t�@�̃��������m��
	ASSERT((dt.size == 8) || (dt.size == 9) || (dt.size == 11));
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));
	try {
		dt.buffer = new BYTE[ length ];
	}
	catch (...) {
		dt.buffer = NULL;
		return FALSE;
	}
	if (!dt.buffer) {
		return FALSE;
	}

	// �ύX�}�b�v�̃��������m��
	try {
		dt.changemap = new BOOL[dt.sectors];
	}
	catch (...) {
		dt.changemap = NULL;
		return FALSE;
	}
	if (!dt.changemap) {
		return FALSE;
	}

	// �ύX�}�b�v���N���A
	for (i=0; i<dt.sectors; i++) {
		dt.changemap[i] = FALSE;
	}

	// �t�@�C������ǂݍ���
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}
	if (dt.raw) {
		// �����ǂ�
		for (i=0; i<dt.sectors; i++) {
			// �V�[�N
			if (!fio.Seek(offset)) {
				fio.Close();
				return FALSE;
			}

			// �ǂݍ���
			if (!fio.Read(&dt.buffer[i << dt.size], 1 << dt.size)) {
				fio.Close();
				return FALSE;
			}

			// ���̃I�t�Z�b�g
			offset += 0x930;
		}
	}
	else {
		// �A���ǂ�
		if (!fio.Seek(offset)) {
			fio.Close();
			return FALSE;
		}
		if (!fio.Read(dt.buffer, length)) {
			fio.Close();
			return FALSE;
		}
	}
	fio.Close();

	// �t���O�𗧂āA����I��
	dt.init = TRUE;
	dt.changed = FALSE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�Z�[�u
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskTrack::Save(const Filepath& path)
{
	DWORD offset;
	int i;
	Fileio fio;
	int length;

	ASSERT(this);

	// ����������Ă��Ȃ���Εs�v
	if (!dt.init) {
		return TRUE;
	}

	// �ύX����Ă��Ȃ���Εs�v
	if (!dt.changed) {
		return TRUE;
	}

	// �������ޕK�v������
	ASSERT(dt.buffer);
	ASSERT(dt.changemap);
	ASSERT((dt.size == 8) || (dt.size == 9) || (dt.size == 11));
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));

	// RAW���[�h�ł͏������݂͂��肦�Ȃ�
	ASSERT(!dt.raw);

	// �I�t�Z�b�g���v�Z(����ȑO�̃g���b�N��256�Z�N�^�ێ��Ƃ݂Ȃ�)
	offset = (dt.track << 8);
	offset <<= dt.size;

	// �Z�N�^������̃����O�X���v�Z
	length = 1 << dt.size;

	// �t�@�C���I�[�v��
	if (!fio.Open(path, Fileio::ReadWrite)) {
		return FALSE;
	}

	// �������݃��[�v
	for (i=0; i<dt.sectors; i++) {
		// �ύX����Ă����
		if (dt.changemap[i]) {
			// �V�[�N�A��������
			if (!fio.Seek(offset + (i << dt.size))) {
				fio.Close();
				return FALSE;
			}
			if (!fio.Write(&dt.buffer[i << dt.size], length)) {
				fio.Close();
				return FALSE;
			}

			// �ύX�t���O�𗎂Ƃ�
			dt.changemap[i] = FALSE;
		}
	}

	// �N���[�Y
	fio.Close();

	// �ύX�t���O�𗎂Ƃ��A�I��
	dt.changed = FALSE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	���[�h�Z�N�^
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskTrack::Read(BYTE *buf, int sec) const
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT((sec >= 0) & (sec < 0x100));

	// ����������Ă��Ȃ���΃G���[
	if (!dt.init) {
		return FALSE;
	}

	// �Z�N�^���L�����𒴂��Ă���΃G���[
	if (sec >= dt.sectors) {
		return FALSE;
	}

	// �R�s�[
	ASSERT(dt.buffer);
	ASSERT((dt.size == 8) || (dt.size == 9) || (dt.size == 11));
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));
	memcpy(buf, &dt.buffer[sec << dt.size], 1 << dt.size);

	// ����
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	���C�g�Z�N�^
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskTrack::Write(const BYTE *buf, int sec)
{
	int offset;
	int length;

	ASSERT(this);
	ASSERT(buf);
	ASSERT((sec >= 0) & (sec < 0x100));
	ASSERT(!dt.raw);

	// ����������Ă��Ȃ���΃G���[
	if (!dt.init) {
		return FALSE;
	}

	// �Z�N�^���L�����𒴂��Ă���΃G���[
	if (sec >= dt.sectors) {
		return FALSE;
	}

	// �I�t�Z�b�g�A�����O�X���v�Z
	offset = sec << dt.size;
	length = 1 << dt.size;

	// ��r
	ASSERT(dt.buffer);
	ASSERT((dt.size == 8) || (dt.size == 9) || (dt.size == 11));
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));
	if (memcmp(buf, &dt.buffer[offset], length) == 0) {
		// �������̂������������Ƃ��Ă���̂ŁA����I��
		return TRUE;
	}

	// �R�s�[�A�ύX����
	memcpy(&dt.buffer[offset], buf, length);
	dt.changemap[sec] = TRUE;
	dt.changed = TRUE;

	// ����
	return TRUE;
}

//===========================================================================
//
//	�f�B�X�N�L���b�V��
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
DiskCache::DiskCache(const Filepath& path, int size, int blocks)
{
	int i;

	ASSERT((size == 8) || (size == 9) || (size == 11));
	ASSERT(blocks > 0);

	// �L���b�V�����[�N
	for (i=0; i<CacheMax; i++) {
		cache[i].disktrk = NULL;
		cache[i].serial = 0;
	}

	// ���̑�
	serial = 0;
	sec_path = path;
	sec_size = size;
	sec_blocks = blocks;
	cd_raw = FALSE;
}

//---------------------------------------------------------------------------
//
//	�f�X�g���N�^
//
//---------------------------------------------------------------------------
DiskCache::~DiskCache()
{
	// �g���b�N���N���A
	Clear();
}

//---------------------------------------------------------------------------
//
//	RAW���[�h�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL DiskCache::SetRawMode(BOOL raw)
{
	ASSERT(this);
	ASSERT(sec_size == 11);

	// �ݒ�
	cd_raw = raw;
}

//---------------------------------------------------------------------------
//
//	�Z�[�u
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::Save()
{
	int i;

	ASSERT(this);

	// �g���b�N��ۑ�
	for (i=0; i<CacheMax; i++) {
		// �L���ȃg���b�N��
		if (cache[i].disktrk) {
			// �ۑ�
			if (!cache[i].disktrk->Save(sec_path)) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�f�B�X�N�L���b�V�����擾
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::GetCache(int index, int& track, DWORD& serial) const
{
	ASSERT(this);
	ASSERT((index >= 0) && (index < CacheMax));

	// ���g�p�Ȃ�FALSE
	if (!cache[index].disktrk) {
		return FALSE;
	}

	// �g���b�N�ƃV���A����ݒ�
	track = cache[index].disktrk->GetTrack();
	serial = cache[index].serial;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�N���A
//
//---------------------------------------------------------------------------
void FASTCALL DiskCache::Clear()
{
	int i;

	ASSERT(this);

	// �L���b�V�����[�N�����
	for (i=0; i<CacheMax; i++) {
		if (cache[i].disktrk) {
			delete cache[i].disktrk;
			cache[i].disktrk = NULL;
		}
	}
}

//---------------------------------------------------------------------------
//
//	�Z�N�^���[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::Read(BYTE *buf, int block)
{
	int track;
	DiskTrack *disktrk;

	ASSERT(this);
	ASSERT(sec_size != 0);

	// ��ɍX�V
	Update();

	// �g���b�N���Z�o(256�Z�N�^/�g���b�N�ɌŒ�)
	track = block >> 8;

	// ���̃g���b�N�f�[�^�𓾂�
	disktrk = Assign(track);
	if (!disktrk) {
		return FALSE;
	}

	// �g���b�N�ɔC����
	return disktrk->Read(buf, (BYTE)block);
}

//---------------------------------------------------------------------------
//
//	�Z�N�^���C�g
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::Write(const BYTE *buf, int block)
{
	int track;
	DiskTrack *disktrk;

	ASSERT(this);
	ASSERT(sec_size != 0);

	// ��ɍX�V
	Update();

	// �g���b�N���Z�o(256�Z�N�^/�g���b�N�ɌŒ�)
	track = block >> 8;

	// ���̃g���b�N�f�[�^�𓾂�
	disktrk = Assign(track);
	if (!disktrk) {
		return FALSE;
	}

	// �g���b�N�ɔC����
	return disktrk->Write(buf, (BYTE)block);
}

//---------------------------------------------------------------------------
//
//	�g���b�N�̊��蓖��
//
//---------------------------------------------------------------------------
DiskTrack* FASTCALL DiskCache::Assign(int track)
{
	int i;
	int c;
	DWORD s;

	ASSERT(this);
	ASSERT(sec_size != 0);
	ASSERT(track >= 0);

	// �܂��A���Ɋ��蓖�Ă���Ă��Ȃ������ׂ�
	for (i=0; i<CacheMax; i++) {
		if (cache[i].disktrk) {
			if (cache[i].disktrk->GetTrack() == track) {
				// �g���b�N����v
				cache[i].serial = serial;
				return cache[i].disktrk;
			}
		}
	}

	// ���ɁA�󂢂Ă�����̂��Ȃ������ׂ�
	for (i=0; i<CacheMax; i++) {
		if (!cache[i].disktrk) {
			// ���[�h�����݂�
			if (Load(i, track)) {
				// ���[�h����
				cache[i].serial = serial;
				return cache[i].disktrk;
			}

			// ���[�h���s
			return NULL;
		}
	}

	// �Ō�ɁA�V���A���ԍ��̈�ԎႢ���̂�T���A�폜����

	// �C���f�b�N�X0�����c�Ƃ���
	s = cache[0].serial;
	c = 0;

	// ���ƃV���A�����r���A��菬�������̂֍X�V����
	for (i=0; i<CacheMax; i++) {
		ASSERT(cache[i].disktrk);

		// ���ɑ��݂���V���A���Ɣ�r�A�X�V
		if (cache[i].serial < s) {
			s = cache[i].serial;
			c = i;
		}
	}

	// ���̃g���b�N��ۑ�
	if (!cache[c].disktrk->Save(sec_path)) {
		return NULL;
	}

	// ���̃g���b�N���폜
	delete cache[c].disktrk;
	cache[c].disktrk = NULL;

	// ���[�h
	if (Load(c, track)) {
		// ���[�h����
		cache[c].serial = serial;
		return cache[c].disktrk;
	}

	// ���[�h���s
	return NULL;
}

//---------------------------------------------------------------------------
//
//	�g���b�N�̃��[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::Load(int index, int track)
{
	int sectors;
	DiskTrack *disktrk;

	ASSERT(this);
	ASSERT((index >= 0) && (index < CacheMax));
	ASSERT(track >= 0);
	ASSERT(!cache[index].disktrk);

	// ���̃g���b�N�̃Z�N�^�����擾
	sectors = sec_blocks - (track << 8);
	ASSERT(sectors > 0);
	if (sectors > 0x100) {
		sectors = 0x100;
	}

	// �f�B�X�N�g���b�N���쐬
	disktrk = new DiskTrack(track, sec_size, sectors, cd_raw);

	// ���[�h�����݂�
	if (!disktrk->Load(sec_path)) {
		// ���s
		delete disktrk;
		return FALSE;
	}

	// ���蓖�Đ����A���[�N��ݒ�
	cache[index].disktrk = disktrk;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�V���A���ԍ��̍X�V
//
//---------------------------------------------------------------------------
void FASTCALL DiskCache::Update()
{
	int i;

	ASSERT(this);

	// �X�V���āA0�ȊO�͉������Ȃ�
	serial++;
	if (serial != 0) {
		return;
	}

	// �S�L���b�V���̃V���A�����N���A(32bit���[�v���Ă���)
	for (i=0; i<CacheMax; i++) {
		cache[i].serial = 0;
	}
}

//===========================================================================
//
//	�f�B�X�N
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
Disk::Disk(Device *dev)
{
	// �R���g���[���ƂȂ�f�o�C�X���L��
	ctrl = dev;

	// ���[�N������
	disk.id = MAKEID('N', 'U', 'L', 'L');
	disk.ready = FALSE;
	disk.writep = FALSE;
	disk.readonly = FALSE;
	disk.removable = FALSE;
	disk.lock = FALSE;
	disk.attn = FALSE;
	disk.reset = FALSE;
	disk.size = 0;
	disk.blocks = 0;
	disk.lun = 0;
	disk.code = 0;
	disk.dcache = NULL;
}

//---------------------------------------------------------------------------
//
//	�f�X�g���N�^
//
//---------------------------------------------------------------------------
Disk::~Disk()
{
	// �f�B�X�N�L���b�V���̕ۑ�
	if (disk.ready) {
		// ���f�B�̏ꍇ�̂�
		ASSERT(disk.dcache);
		disk.dcache->Save();
	}

	// �f�B�X�N�L���b�V���̍폜
	if (disk.dcache) {
		delete disk.dcache;
		disk.dcache = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	���Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL Disk::Reset()
{
	ASSERT(this);

	// ���b�N�Ȃ��A�A�e���V�����Ȃ��A���Z�b�g����
	disk.lock = FALSE;
	disk.attn = FALSE;
	disk.reset = TRUE;
}

//---------------------------------------------------------------------------
//
//	�Z�[�u
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Save(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);

	// �T�C�Y���Z�[�u
	sz = sizeof(disk_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// ���̂��Z�[�u
	if (!fio->Write(&disk, (int)sz)) {
		return FALSE;
	}

	// �p�X���Z�[�u
	if (!diskpath.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	���[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Load(Fileio *fio, int ver)
{
	size_t sz;
	disk_t buf;
	Filepath path;

	ASSERT(this);
	ASSERT(fio);

	// version2.03���O�́A�f�B�X�N�̓Z�[�u���Ă��Ȃ�
	if (ver <= 0x0202) {
		return TRUE;
	}

	// ���݂̃f�B�X�N�L���b�V�����폜
	if (disk.dcache) {
		disk.dcache->Save();
		delete disk.dcache;
		disk.dcache = NULL;
	}

	// �T�C�Y�����[�h�A�ƍ�
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(disk_t)) {
		return FALSE;
	}

	// �o�b�t�@�փ��[�h
	if (!fio->Read(&buf, (int)sz)) {
		return FALSE;
	}

	// �p�X�����[�h
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	// ID����v�����ꍇ�̂݁A�ړ�
	if (disk.id == buf.id) {
		// NULL�Ȃ牽�����Ȃ�
		if (IsNULL()) {
			return TRUE;
		}

		// �Z�[�u�������Ɠ�����ނ̃f�o�C�X
		disk.ready = FALSE;
		if (Open(path)) {
			// Open���Ńf�B�X�N�L���b�V���͍쐬����Ă���
			// �v���p�e�B�݈̂ړ�
			if (!disk.readonly) {
				disk.writep = buf.writep;
			}
			disk.lock = buf.lock;
			disk.attn = buf.attn;
			disk.reset = buf.reset;
			disk.lun = buf.lun;
			disk.code = buf.code;

			// ����Ƀ��[�h�ł���
			return TRUE;
		}
	}

	// �f�B�X�N�L���b�V���č쐬
	if (!IsReady()) {
		disk.dcache = NULL;
	}
	else {
		disk.dcache = new DiskCache(diskpath, disk.size, disk.blocks);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	NULL�`�F�b�N
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::IsNULL() const
{
	ASSERT(this);

	if (disk.id == MAKEID('N', 'U', 'L', 'L')) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	SASI�`�F�b�N
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::IsSASI() const
{
	ASSERT(this);

	if (disk.id == MAKEID('S', 'A', 'H', 'D')) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	�I�[�v��
//	���h���N���X�ŁA�I�[�v��������̌㏈���Ƃ��ČĂяo������
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Open(const Filepath& path)
{
	Fileio fio;

	ASSERT(this);
	ASSERT((disk.size == 8) || (disk.size == 9) || (disk.size == 11));
	ASSERT(disk.blocks > 0);

	// ���f�B
	disk.ready = TRUE;

	// �L���b�V��������
	ASSERT(!disk.dcache);
	disk.dcache = new DiskCache(path, disk.size, disk.blocks);

	// �ǂݏ����I�[�v���\��
	if (fio.Open(path, Fileio::ReadWrite)) {
		// �������݋��A���[�h�I�����[�łȂ�
		disk.writep = FALSE;
		disk.readonly = FALSE;
		fio.Close();
	}
	else {
		// �������݋֎~�A���[�h�I�����[
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// ���b�N����Ă��Ȃ�
	disk.lock = FALSE;

	// �p�X�ۑ�
	diskpath = path;

	// ����
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�C�W�F�N�g
//
//---------------------------------------------------------------------------
void FASTCALL Disk::Eject(BOOL force)
{
	ASSERT(this);

	// �����[�o�u���łȂ���΃C�W�F�N�g�ł��Ȃ�
	if (!disk.removable) {
		return;
	}

	// ���f�B�łȂ���΃C�W�F�N�g�K�v�Ȃ�
	if (!disk.ready) {
		return;
	}

	// �����t���O���Ȃ��ꍇ�A���b�N����Ă��Ȃ����Ƃ��K�v
	if (!force) {
		if (disk.lock) {
			return;
		}
	}

	// �f�B�X�N�L���b�V�����폜
	disk.dcache->Save();
	delete disk.dcache;
	disk.dcache = NULL;

	// �m�b�g���f�B�A�A�e���V�����Ȃ�
	disk.ready = FALSE;
	disk.writep = FALSE;
	disk.readonly = FALSE;
	disk.attn = FALSE;
}

//---------------------------------------------------------------------------
//
//	�������݋֎~
//
//---------------------------------------------------------------------------
void FASTCALL Disk::WriteP(BOOL writep)
{
	ASSERT(this);

	// ���f�B�ł��邱��
	if (!disk.ready) {
		return;
	}

	// Read Only�̏ꍇ�́A�v���e�N�g��Ԃ̂�
	if (disk.readonly) {
		ASSERT(disk.writep);
		return;
	}

	// �t���O�ݒ�
	disk.writep = writep;
}

//---------------------------------------------------------------------------
//
//	�������[�N�擾
//
//---------------------------------------------------------------------------
void FASTCALL Disk::GetDisk(disk_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);

	// �������[�N���R�s�[
	*buffer = disk;
}

//---------------------------------------------------------------------------
//
//	�p�X�擾
//
//---------------------------------------------------------------------------
void FASTCALL Disk::GetPath(Filepath& path) const
{
	path = diskpath;
}

//---------------------------------------------------------------------------
//
//	�t���b�V��
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Flush()
{
	ASSERT(this);

	// �L���b�V�����Ȃ���Ή������Ȃ�
	if (!disk.dcache) {
		return TRUE;
	}

	// �L���b�V����ۑ�
	return disk.dcache->Save();
}

//---------------------------------------------------------------------------
//
//	���f�B�`�F�b�N
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::CheckReady()
{
	ASSERT(this);

	// ���Z�b�g�Ȃ�A�X�e�[�^�X��Ԃ�
	if (disk.reset) {
		disk.code = DISK_DEVRESET;
		disk.reset = FALSE;
		return FALSE;
	}

	// �A�e���V�����Ȃ�A�X�e�[�^�X��Ԃ�
	if (disk.attn) {
		disk.code = DISK_ATTENTION;
		disk.attn = FALSE;
		return FALSE;
	}

	// �m�b�g���f�B�Ȃ�A�X�e�[�^�X��Ԃ�
	if (!disk.ready) {
		disk.code = DISK_NOTREADY;
		return FALSE;
	}

	// �G���[�Ȃ��ɏ�����
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//	���펞��������K�v������
//
//---------------------------------------------------------------------------
int FASTCALL Disk::Inquiry(const DWORD* /*cdb*/, BYTE* /*buf*/)
{
	ASSERT(this);

	// �f�t�H���g��INQUIRY���s
	disk.code = DISK_INVALIDCMD;
	return 0;
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//	��SASI�͕ʏ���
//
//---------------------------------------------------------------------------
int FASTCALL Disk::RequestSense(const DWORD *cdb, BYTE *buf)
{
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);

	// �G���[���Ȃ��ꍇ�Ɍ���A�m�b�g���f�B���`�F�b�N
	if (disk.code == DISK_NOERROR) {
		if (!disk.ready) {
			disk.code = DISK_NOTREADY;
		}
	}

	// �T�C�Y����(�A���P�[�V���������O�X�ɏ]��)
	size = (int)cdb[4];
	ASSERT((size >= 0) && (size < 0x100));

	// SCSI-1�ł́A�T�C�Y0�̂Ƃ���4�o�C�g�]������(SCSI-2�ł͂��̎d�l�͍폜)
	if (size == 0) {
		size = 4;
	}

	// �o�b�t�@���N���A
	memset(buf, 0, size);

	// �g���Z���X�f�[�^���܂߂��A18�o�C�g��ݒ�
	buf[0] = 0x70;
	buf[2] = (BYTE)(disk.code >> 16);
	buf[7] = 10;
	buf[12] = (BYTE)(disk.code >> 8);
	buf[13] = (BYTE)disk.code;

	// �R�[�h���N���A
	disk.code = 0x00;

	return size;
}

//---------------------------------------------------------------------------
//
//	MODE SELECT�`�F�b�N
//	��disk.code�̉e�����󂯂Ȃ�
//
//---------------------------------------------------------------------------
int FASTCALL Disk::SelectCheck(const DWORD *cdb)
{
	int length;

	ASSERT(this);
	ASSERT(cdb);

	// �Z�[�u�p�����[�^���ݒ肳��Ă���΃G���[
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return 0;
	}

	// �p�����[�^�����O�X�Ŏw�肳�ꂽ�f�[�^���󂯎��
	length = (int)cdb[4];
	return length;
}

//---------------------------------------------------------------------------
//
//	MODE SELECT
//	��disk.code�̉e�����󂯂Ȃ�
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::ModeSelect(const BYTE *buf, int size)
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT(size >= 0);

	// �ݒ�ł��Ȃ�
	disk.code = DISK_INVALIDPRM;

	printf("%p %d", (const void*)buf, size);

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	MODE SENSE
//	��disk.code�̉e�����󂯂Ȃ�
//
//---------------------------------------------------------------------------
int FASTCALL Disk::ModeSense(const DWORD *cdb, BYTE *buf)
{
	int page;
	int length;
	int size;
	BOOL valid;
	BOOL change;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x1a);

	// �����O�X�擾�A�o�b�t�@�N���A
	length = (int)cdb[4];
	ASSERT((length >= 0) && (length < 0x100));
	memset(buf, 0, length);

	// �ύX�\�t���O�擾
	if ((cdb[2] & 0xc0) == 0x40) {
		change = TRUE;
	}
	else {
		change = FALSE;
	}

	// �y�[�W�R�[�h�擾(0x00�͍ŏ�����valid)
	page = cdb[2] & 0x3f;
	if (page == 0x00) {
		valid = TRUE;
	}
	else {
		valid = FALSE;
	}

	// ��{���
	size = 4;
	if (disk.writep) {
		buf[2] = 0x80;
	}

	// DBD��0�Ȃ�A�u���b�N�f�B�X�N���v�^��ǉ�
	if ((cdb[1] & 0x08) == 0) {
		// ���[�h�p�����[�^�w�b�_
		buf[ 3] = 0x08;

		// ���f�B�̏ꍇ�Ɍ���
		if (disk.ready) {
			// �u���b�N�f�B�X�N���v�^(�u���b�N��)
			buf[ 5] = (BYTE)(disk.blocks >> 16);
			buf[ 6] = (BYTE)(disk.blocks >> 8);
			buf[ 7] = (BYTE)disk.blocks;

			// �u���b�N�f�B�X�N���v�^(�u���b�N�����O�X)
			size = 1 << disk.size;
			buf[ 9] = (BYTE)(size >> 16);
			buf[10] = (BYTE)(size >> 8);
			buf[11] = (BYTE)size;
		}

		// �T�C�Y�Đݒ�
		size = 12;
	}

	// �y�[�W�R�[�h1(read-write error recovery)
	if ((page == 0x01) || (page == 0x3f)) {
		size += AddError(change, &buf[size]);
		valid = TRUE;
	}

	// �y�[�W�R�[�h3(format device)
	if ((page == 0x03) || (page == 0x3f)) {
		size += AddFormat(change, &buf[size]);
		valid = TRUE;
	}

	// �y�[�W�R�[�h6(optical)
	if (disk.id == MAKEID('S', 'C', 'M', 'O')) {
		if ((page == 0x06) || (page == 0x3f)) {
			size += AddOpt(change, &buf[size]);
			valid = TRUE;
		}
	}

	// �y�[�W�R�[�h8(caching)
	if ((page == 0x08) || (page == 0x3f)) {
		size += AddCache(change, &buf[size]);
		valid = TRUE;
	}

	// �y�[�W�R�[�h13(CD-ROM)
	if (disk.id == MAKEID('S', 'C', 'C', 'D')) {
		if ((page == 0x0d) || (page == 0x3f)) {
			size += AddCDROM(change, &buf[size]);
			valid = TRUE;
		}
	}

	// �y�[�W�R�[�h14(CD-DA)
	if (disk.id == MAKEID('S', 'C', 'C', 'D')) {
		if ((page == 0x0e) || (page == 0x3f)) {
			size += AddCDDA(change, &buf[size]);
			valid = TRUE;
		}
	}

	// ���[�h�f�[�^�����O�X���ŏI�ݒ�
	buf[0] = (BYTE)(size - 1);

	// �T�|�[�g���Ă��Ȃ��y�[�W��
	if (!valid) {
		disk.code = DISK_INVALIDCDB;
		return 0;
	}

	// Saved values�̓T�|�[�g���Ă��Ȃ�
	if ((cdb[2] & 0xc0) == 0xc0) {
		disk.code = DISK_PARAMSAVE;
		return 0;
	}

	// MODE SENSE����
	disk.code = DISK_NOERROR;
	return length;
}

//---------------------------------------------------------------------------
//
//	�G���[�y�[�W�ǉ�
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddError(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// �R�[�h�E�����O�X��ݒ�
	buf[0] = 0x01;
	buf[1] = 0x0a;

	// �ύX�\�̈�͂Ȃ�
	if (change) {
		return 12;
	}

	// ���g���C�J�E���g��0�A���~�b�g�^�C���͑��u�����̃f�t�H���g�l���g�p
	return 12;
}

//---------------------------------------------------------------------------
//
//	�t�H�[�}�b�g�y�[�W�ǉ�
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddFormat(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// �R�[�h�E�����O�X��ݒ�
	buf[0] = 0x03;
	buf[1] = 0x16;

	// �ύX�\�̈�͂Ȃ�
	if (change) {
		return 24;
	}

	// �����[�o�u��������ݒ�
	if (disk.removable) {
		buf[20] = 0x20;
	}

	return 24;
}

//---------------------------------------------------------------------------
//
//	�I�v�e�B�J���y�[�W�ǉ�
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddOpt(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// �R�[�h�E�����O�X��ݒ�
	buf[0] = 0x06;
	buf[1] = 0x02;

	// �ύX�\�̈�͂Ȃ�
	if (change) {
		return 4;
	}

	// �X�V�u���b�N�̃��|�[�g�͍s��Ȃ�
	return 4;
}

//---------------------------------------------------------------------------
//
//	�L���b�V���y�[�W�ǉ�
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddCache(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// �R�[�h�E�����O�X��ݒ�
	buf[0] = 0x08;
	buf[1] = 0x0a;

	// �ύX�\�̈�͂Ȃ�
	if (change) {
		return 12;
	}

	// �ǂݍ��݃L���b�V���̂ݗL���A�v���t�F�b�`�͍s��Ȃ�
	return 12;
}

//---------------------------------------------------------------------------
//
//	CD-ROM�y�[�W�ǉ�
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddCDROM(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// �R�[�h�E�����O�X��ݒ�
	buf[0] = 0x0d;
	buf[1] = 0x06;

	// �ύX�\�̈�͂Ȃ�
	if (change) {
		return 8;
	}

	// �C���A�N�e�B�u�^�C�}��2sec
	buf[3] = 0x05;

	// MSF�{���͂��ꂼ��60, 75
	buf[5] = 60;
	buf[7] = 75;

	return 8;
}

//---------------------------------------------------------------------------
//
//	CD-DA�y�[�W�ǉ�
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddCDDA(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// �R�[�h�E�����O�X��ݒ�
	buf[0] = 0x0e;
	buf[1] = 0x0e;

	// �ύX�\�̈�͂Ȃ�
	if (change) {
		return 16;
	}

	// �I�[�f�B�I�͑��슮����҂��A �����g���b�N�ɂ܂�����PLAY��������
	return 16;
}

//---------------------------------------------------------------------------
//
//	TEST UNIT READY
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::TestUnitReady(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// ��ԃ`�F�b�N
	if (!CheckReady()) {
		return FALSE;
	}

	// TEST UNIT READY����
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	REZERO UNIT
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Rezero(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// ��ԃ`�F�b�N
	if (!CheckReady()) {
		return FALSE;
	}

	// REZERO����
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	FORMAT UNIT
//	��SASI�̓I�y�R�[�h$06�ASCSI�̓I�y�R�[�h$04
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Format(const DWORD *cdb)
{
	ASSERT(this);

	// ��ԃ`�F�b�N
	if (!CheckReady()) {
		return FALSE;
	}

	// FMTDATA=1�̓T�|�[�g���Ȃ�
	if (cdb[1] & 0x10) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// FORMAT����
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	REASSIGN BLOCKS
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Reassign(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// ��ԃ`�F�b�N
	if (!CheckReady()) {
		return FALSE;
	}

	// REASSIGN BLOCKS����
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	READ
//
//---------------------------------------------------------------------------
int FASTCALL Disk::Read(BYTE *buf, int block)
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT(block >= 0);

	// ��ԃ`�F�b�N
	if (!CheckReady()) {
		return 0;
	}

	// �g�[�^���u���b�N���𒴂��Ă���΃G���[
	if (block >= disk.blocks) {
		disk.code = DISK_INVALIDLBA;
		return 0;
	}

	// �L���b�V���ɔC����
	if (!disk.dcache->Read(buf, block)) {
		disk.code = DISK_READFAULT;
		return 0;
	}

	// ����
	return (1 << disk.size);
}

//---------------------------------------------------------------------------
//
//	WRITE�`�F�b�N
//
//---------------------------------------------------------------------------
int FASTCALL Disk::WriteCheck(int block)
{
	ASSERT(this);
	ASSERT(block >= 0);

	// ��ԃ`�F�b�N
	if (!CheckReady()) {
		return 0;
	}

	// �g�[�^���u���b�N���𒴂��Ă���΃G���[
	if (block >= disk.blocks) {
		return 0;
	}

	// �������݋֎~�Ȃ�G���[
	if (disk.writep) {
		disk.code = DISK_WRITEPROTECT;
		return 0;
	}

	// ����
	return (1 << disk.size);
}

//---------------------------------------------------------------------------
//
//	WRITE
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Write(const BYTE *buf, int block)
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT(block >= 0);

	// ���f�B�łȂ���΃G���[
	if (!disk.ready) {
		disk.code = DISK_NOTREADY;
		return FALSE;
	}

	// �g�[�^���u���b�N���𒴂��Ă���΃G���[
	if (block >= disk.blocks) {
		disk.code = DISK_INVALIDLBA;
		return FALSE;
	}

	// �������݋֎~�Ȃ�G���[
	if (disk.writep) {
		disk.code = DISK_WRITEPROTECT;
		return FALSE;
	}

	// �L���b�V���ɔC����
	if (!disk.dcache->Write(buf, block)) {
		disk.code = DISK_WRITEFAULT;
		return FALSE;
	}

	// ����
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	SEEK
//	��LBA�̃`�F�b�N�͍s��Ȃ�(SASI IOCS)
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Seek(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// ��ԃ`�F�b�N
	if (!CheckReady()) {
		return FALSE;
	}

	// SEEK����
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	START STOP UNIT
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::StartStop(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x1b);

	// �C�W�F�N�g�r�b�g�����āA�K�v�Ȃ�C�W�F�N�g
	if (cdb[4] & 0x02) {
		if (disk.lock) {
			// ���b�N����Ă���̂ŁA�C�W�F�N�g�ł��Ȃ�
			disk.code = DISK_PREVENT;
			return FALSE;
		}

		// �C�W�F�N�g
		Eject(FALSE);
	}

	// OK
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	SEND DIAGNOSTIC
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::SendDiag(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x1d);

	// PF�r�b�g�̓T�|�[�g���Ȃ�
	if (cdb[1] & 0x10) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// �p�����[�^���X�g�̓T�|�[�g���Ȃ�
	if ((cdb[3] != 0) || (cdb[4] != 0)) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// ��ɐ���
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	PREVENT/ALLOW MEDIUM REMOVAL
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Removal(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x1e);

	// ��ԃ`�F�b�N
	if (!CheckReady()) {
		return FALSE;
	}

	// ���b�N�t���O��ݒ�
	if (cdb[4] & 0x01) {
		disk.lock = TRUE;
	}
	else {
		disk.lock = FALSE;
	}

	// REMOVAL����
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	READ CAPACITY
//
//---------------------------------------------------------------------------
int FASTCALL Disk::ReadCapacity(const DWORD* /*cdb*/, BYTE *buf)
{
	DWORD blocks;
	DWORD length;

	ASSERT(this);
	ASSERT(buf);

	// �o�b�t�@�N���A
	memset(buf, 0, 8);

	// ��ԃ`�F�b�N
	if (!CheckReady()) {
		return 0;
	}

	// �_���u���b�N�A�h���X�̏I�[(disk.blocks - 1)���쐬
	ASSERT(disk.blocks > 0);
	blocks = disk.blocks - 1;
	buf[0] = (BYTE)(blocks >> 24);
	buf[1] = (BYTE)(blocks >> 16);
	buf[2] = (BYTE)(blocks >>  8);
	buf[3] = (BYTE)blocks;

	// �u���b�N�����O�X(1 << disk.size)���쐬
	length = 1 << disk.size;
	buf[4] = (BYTE)(length >> 24);
	buf[5] = (BYTE)(length >> 16);
	buf[6] = (BYTE)(length >> 8);
	buf[7] = (BYTE)length;

	// �ԑ��T�C�Y��Ԃ�
	return 8;
}

//---------------------------------------------------------------------------
//
//	VERIFY
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Verify(const DWORD *cdb)
{
	int record;
    int blocks;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x2f);

	// �p�����[�^�擾
	record = cdb[2];
	record <<= 8;
	record |= cdb[3];
	record <<= 8;
	record |= cdb[4];
	record <<= 8;
	record |= cdb[5];
	blocks = cdb[7];
	blocks <<= 8;
	blocks |= cdb[8];

	// ��ԃ`�F�b�N
	if (!CheckReady()) {
		return 0;
	}

	// �p�����[�^�`�F�b�N
	if (disk.blocks < (record + blocks)) {
		disk.code = DISK_INVALIDLBA;
		return FALSE;
	}

	// ����
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	READ TOC
//
//---------------------------------------------------------------------------
int FASTCALL Disk::ReadToc(const DWORD *cdb, BYTE *buf)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x43);
	ASSERT(buf);

	printf("%p %p", (const void*)buf, (const void*)cdb);

	// ���̃R�}���h�̓T�|�[�g���Ȃ�
	disk.code = DISK_INVALIDCMD;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::PlayAudio(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x45);

	printf("%p", (const void*)cdb);

	// ���̃R�}���h�̓T�|�[�g���Ȃ�
	disk.code = DISK_INVALIDCMD;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO MSF
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::PlayAudioMSF(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x47);

	printf("%p", (const void*)cdb);
	// ���̃R�}���h�̓T�|�[�g���Ȃ�
	disk.code = DISK_INVALIDCMD;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO TRACK
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::PlayAudioTrack(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x48);
	printf("%p", (const void*)cdb);
	// ���̃R�}���h�̓T�|�[�g���Ȃ�
	disk.code = DISK_INVALIDCMD;
	return FALSE;
}

//===========================================================================
//
//	SASI �n�[�h�f�B�X�N
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
SASIHD::SASIHD(Device *dev) : Disk(dev)
{
	// SASI �n�[�h�f�B�X�N
	disk.id = MAKEID('S', 'A', 'H', 'D');
}

//---------------------------------------------------------------------------
//
//	�I�[�v��
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIHD::Open(const Filepath& path)
{
	Fileio fio;
	DWORD size;

	ASSERT(this);
	ASSERT(!disk.ready);

	// �ǂݍ��݃I�[�v�����K�v
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// �t�@�C���T�C�Y�̎擾
	size = fio.GetFileSize();
	fio.Close();

	// 10MB, 20MB, 40MB�̂�
	switch (size) {
		// 10MB
		case 0x9f5400:
			break;

		// 20MB
		case 0x13c9800:
			break;

		// 40MB
		case 0x2793000:
			break;

		// ���̑�(�T�|�[�g���Ȃ�)
		default:
			return FALSE;
	}

	// �Z�N�^�T�C�Y�ƃu���b�N��
	disk.size = 8;
	disk.blocks = size >> 8;

	// ��{�N���X
	return Disk::Open(path);
}

//---------------------------------------------------------------------------
//
//	�f�o�C�X���Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL SASIHD::Reset()
{
	ASSERT(this);

	// ���b�N��ԉ����A�A�e���V��������
	disk.lock = FALSE;
	disk.attn = FALSE;

	// ���Z�b�g�Ȃ��A�R�[�h���N���A
	disk.reset = TRUE;
	disk.code = 0x00;
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//
//---------------------------------------------------------------------------
int FASTCALL SASIHD::RequestSense(const DWORD *cdb, BYTE *buf)
{
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);

	// �T�C�Y����
	size = (int)cdb[4];
	ASSERT((size >= 0) && (size < 0x100));

	// SASI�͔�g���t�H�[�}�b�g�ɌŒ�
	memset(buf, 0, size);
	buf[0] = (BYTE)(disk.code >> 16);
	buf[1] = (BYTE)(disk.lun << 5);

	// �R�[�h���N���A
	disk.code = 0x00;

	return size;
}

//===========================================================================
//
//	SCSI �n�[�h�f�B�X�N
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
SCSIHD::SCSIHD(Device *dev) : Disk(dev)
{
	// SCSI �n�[�h�f�B�X�N
	disk.id = MAKEID('S', 'C', 'H', 'D');
}

//---------------------------------------------------------------------------
//
//	�I�[�v��
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIHD::Open(const Filepath& path)
{
	Fileio fio;
	DWORD size;

	ASSERT(this);
	ASSERT(!disk.ready);

	// �ǂݍ��݃I�[�v�����K�v
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// �t�@�C���T�C�Y�̎擾
	size = fio.GetFileSize();
	fio.Close();

	// 512�o�C�g�P�ʂł��邱��
	if (size & 0x1ff) {
		return FALSE;
	}

	// 10MB�ȏ�4GB����
	if (size < 0x9f5400) {
		return FALSE;
	}
	if (size > 0xfff00000) {
		return FALSE;
	}

	// �Z�N�^�T�C�Y�ƃu���b�N��
	disk.size = 9;
	disk.blocks = size >> 9;

	// ��{�N���X
	return Disk::Open(path);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD::Inquiry(const DWORD *cdb, BYTE *buf)
{
	DWORD major;
	DWORD minor;
	char string[32];
	int size;
	int len;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD�`�F�b�N
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// ���f�B�`�F�b�N(�C���[�W�t�@�C�����Ȃ��ꍇ�A�G���[�Ƃ���)
	if (!disk.ready) {
		disk.code = DISK_NOTREADY;
		return FALSE;
	}

	// ��{�f�[�^
	// buf[0] ... Direct Access Device
	// buf[2] ... SCSI-2�����̃R�}���h�̌n
	// buf[3] ... SCSI-2������Inquiry���X�|���X
	// buf[4] ... Inquiry�ǉ��f�[�^
	memset(buf, 0, 8);
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 0x1f;

	// �x���_
	memset(&buf[8], 0x20, 28);
	memcpy(&buf[8], "XM6", 3);

	// ���i��
	size = disk.blocks >> 11;
	if (size < 300)
		sprintf(string, "PRODRIVE LPS%dS", size);
	else if (size < 600)
		sprintf(string, "MAVERICK%dS", size);
	else if (size < 800)
		sprintf(string, "LIGHTNING%dS", size);
	else if (size < 1000)
		sprintf(string, "TRAILBRAZER%dS", size);
	else if (size < 2000)
		sprintf(string, "FIREBALL%dS", size);
	else
		sprintf(string, "FBSE%d.%dS", size / 1000, (size % 1000) / 100);
	memcpy(&buf[16], string, strlen(string));

	// ���r�W����(XM6�̃o�[�W����No)
	ctrl->GetVM()->GetVersion(major, minor);
	sprintf(string, "0%01d%01d%01d",
				major, (minor >> 4), (minor & 0x0f));
	memcpy((char*)&buf[32], string, 4);

	// �T�C�Y36�o�C�g���A���P�[�V���������O�X�̂����A�Z�����œ]��
	size = 36;
	len = (int)cdb[4];
	if (len < size) {
		size = len;
	}

	// ����
	disk.code = DISK_NOERROR;
	return size;
}

//===========================================================================
//
//	SCSI �����C�f�B�X�N
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
SCSIMO::SCSIMO(Device *dev) : Disk(dev)
{
	// SCSI �����C�f�B�X�N
	disk.id = MAKEID('S', 'C', 'M', 'O');

	// �����[�o�u��
	disk.removable = TRUE;
}

//---------------------------------------------------------------------------
//
//	�I�[�v��
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIMO::Open(const Filepath& path, BOOL attn)
{
	Fileio fio;
	DWORD size;

	ASSERT(this);
	ASSERT(!disk.ready);

	// �ǂݍ��݃I�[�v�����K�v
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// �t�@�C���T�C�Y�̎擾
	size = fio.GetFileSize();
	fio.Close();

	switch (size) {
		// 128MB
		case 0x797f400:
			disk.size = 9;
			disk.blocks = 248826;
			break;

		// 230MB
		case 0xd9eea00:
			disk.size = 9;
			disk.blocks = 446325;
			break;

		// 540MB
		case 0x1fc8b800:
			disk.size = 9;
			disk.blocks = 1041500;
			break;

		// 640MB
		case 0x25e28000:
			disk.size = 11;
			disk.blocks = 310352;
			break;

		// ���̑�(�G���[)
		default:
			return FALSE;
	}

	// ��{�N���X
	Disk::Open(path);

	// ���f�B�Ȃ�A�e���V����
	if (disk.ready && attn) {
		disk.attn = TRUE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	���[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIMO::Load(Fileio *fio, int ver)
{
	size_t sz;
	disk_t buf;
	Filepath path;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x0200);

	// version2.03���O�́A�f�B�X�N�̓Z�[�u���Ă��Ȃ�
	if (ver <= 0x0202) {
		return TRUE;
	}

	// �T�C�Y�����[�h�A�ƍ�
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(disk_t)) {
		return FALSE;
	}

	// �o�b�t�@�փ��[�h
	if (!fio->Read(&buf, (int)sz)) {
		return FALSE;
	}

	// �p�X�����[�h
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	// �K���C�W�F�N�g
	Eject(TRUE);

	// ID����v�����ꍇ�̂݁A�ړ�
	if (disk.id != buf.id) {
		// �Z�[�u����MO�łȂ������B�C�W�F�N�g��Ԃ��ێ�
		return TRUE;
	}

	// �ăI�[�v�������݂�
	if (!Open(path, FALSE)) {
		// �ăI�[�v���ł��Ȃ��B�C�W�F�N�g��Ԃ��ێ�
		return TRUE;
	}

	// Open���Ńf�B�X�N�L���b�V���͍쐬����Ă���B�v���p�e�B�݈̂ړ�
	if (!disk.readonly) {
		disk.writep = buf.writep;
	}
	disk.lock = buf.lock;
	disk.attn = buf.attn;
	disk.reset = buf.reset;
	disk.lun = buf.lun;
	disk.code = buf.code;

	// ����Ƀ��[�h�ł���
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIMO::Inquiry(const DWORD *cdb, BYTE *buf)
{
	DWORD major;
	DWORD minor;
	char string[32];
	int size;
	int len;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD�`�F�b�N
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// ��{�f�[�^
	// buf[0] ... Optical Memory Device
	// buf[1] ... �����[�o�u��
	// buf[2] ... SCSI-2�����̃R�}���h�̌n
	// buf[3] ... SCSI-2������Inquiry���X�|���X
	// buf[4] ... Inquiry�ǉ��f�[�^
	memset(buf, 0, 8);
	buf[0] = 0x07;
	buf[1] = 0x80;
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 0x1f;

	// �x���_
	memset(&buf[8], 0x20, 28);
	memcpy(&buf[8], "XM6", 3);

	// ���i��
	memcpy(&buf[16], "M2513A", 6);

	// ���r�W����(XM6�̃o�[�W����No)
	ctrl->GetVM()->GetVersion(major, minor);
	sprintf(string, "0%01d%01d%01d",
				major, (minor >> 4), (minor & 0x0f));
	memcpy((char*)&buf[32], string, 4);

	// �T�C�Y36�o�C�g���A���P�[�V���������O�X�̂����A�Z�����œ]��
	size = 36;
	len = cdb[4];
	if (len < size) {
		size = len;
	}

	// ����
	disk.code = DISK_NOERROR;
	return size;
}

//===========================================================================
//
//	CD�g���b�N
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
CDTrack::CDTrack(SCSICD *scsicd)
{
	ASSERT(scsicd);

	// �e�ƂȂ�CD-ROM�f�o�C�X��ݒ�
	cdrom = scsicd;

	// �g���b�N����
	valid = FALSE;

	// ���̑��̃f�[�^��������
	track_no = -1;
	first_lba = 0;
	last_lba = 0;
	audio = FALSE;
	raw = FALSE;
}

//---------------------------------------------------------------------------
//
//	�f�X�g���N�^
//
//---------------------------------------------------------------------------
CDTrack::~CDTrack()
{
}

//---------------------------------------------------------------------------
//
//	������
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDTrack::Init(int track, DWORD first, DWORD last)
{
	ASSERT(this);
	ASSERT(!valid);
	ASSERT(track >= 1);
	ASSERT(first < last);

	// �g���b�N�ԍ���ݒ�A�L����
	track_no = track;
	valid = TRUE;

	// LBA���L��
	first_lba = first;
	last_lba = last;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�p�X�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL CDTrack::SetPath(BOOL cdda, const Filepath& path)
{
	ASSERT(this);
	ASSERT(valid);

	// CD-DA���A�f�[�^��
	audio = cdda;

	// �p�X�L��
	imgpath = path;
}

//---------------------------------------------------------------------------
//
//	�p�X�擾
//
//---------------------------------------------------------------------------
void FASTCALL CDTrack::GetPath(Filepath& path) const
{
	ASSERT(this);
	ASSERT(valid);

	// �p�X��Ԃ�
	path = imgpath;
}

//---------------------------------------------------------------------------
//
//	�C���f�b�N�X�ǉ�
//
//---------------------------------------------------------------------------
void FASTCALL CDTrack::AddIndex(int index, DWORD lba)
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(index > 0);
	ASSERT(first_lba <= lba);
	ASSERT(lba <= last_lba);

	printf("%d %d", lba, index);

	// ���݂̓C���f�b�N�X�̓T�|�[�g���Ȃ�
	ASSERT(FALSE);
}

//---------------------------------------------------------------------------
//
//	�J�nLBA�擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDTrack::GetFirst() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	return first_lba;
}

//---------------------------------------------------------------------------
//
//	�I�[LBA�擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDTrack::GetLast() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	return last_lba;
}

//---------------------------------------------------------------------------
//
//	�u���b�N���擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDTrack::GetBlocks() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	// �J�nLBA�ƍŏILBA����Z�o
	return (DWORD)(last_lba - first_lba + 1);
}

//---------------------------------------------------------------------------
//
//	�g���b�N�ԍ��擾
//
//---------------------------------------------------------------------------
int FASTCALL CDTrack::GetTrackNo() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(track_no >= 1);

	return track_no;
}

//---------------------------------------------------------------------------
//
//	�L���ȃu���b�N��
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDTrack::IsValid(DWORD lba) const
{
	ASSERT(this);

	// �g���b�N���̂������Ȃ�AFALSE
	if (!valid) {
		return FALSE;
	}

	// first���O�Ȃ�AFALSE
	if (lba < first_lba) {
		return FALSE;
	}

	// last����Ȃ�AFALSE
	if (last_lba < lba) {
		return FALSE;
	}

	// ���̃g���b�N
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�I�[�f�B�I�g���b�N��
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDTrack::IsAudio() const
{
	ASSERT(this);
	ASSERT(valid);

	return audio;
}

//===========================================================================
//
//	CD-DA�o�b�t�@
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
CDDABuf::CDDABuf()
{
}

//---------------------------------------------------------------------------
//
//	�f�X�g���N�^
//
//---------------------------------------------------------------------------
CDDABuf::~CDDABuf()
{
}

//===========================================================================
//
//	SCSI CD-ROM
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
SCSICD::SCSICD(Device *dev) : Disk(dev)
{
	int i;

	// SCSI CD-ROM
	disk.id = MAKEID('S', 'C', 'C', 'D');

	// �����[�o�u���A�����݋֎~
	disk.removable = TRUE;
	disk.writep = TRUE;

	// RAW�`���łȂ�
	rawfile = FALSE;

	// �t���[��������
	frame = 0;

	// �g���b�N������
	for (i=0; i<TrackMax; i++) {
		track[i] = NULL;
	}
	tracks = 0;
	dataindex = -1;
	audioindex = -1;
}

//---------------------------------------------------------------------------
//
//	�f�X�g���N�^
//
//---------------------------------------------------------------------------
SCSICD::~SCSICD()
{
	// �g���b�N�N���A
	ClearTrack();
}

//---------------------------------------------------------------------------
//
//	���[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::Load(Fileio *fio, int ver)
{
	size_t sz;
	disk_t buf;
	Filepath path;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x0200);

	// version2.03���O�́A�f�B�X�N�̓Z�[�u���Ă��Ȃ�
	if (ver <= 0x0202) {
		return TRUE;
	}

	// �T�C�Y�����[�h�A�ƍ�
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(disk_t)) {
		return FALSE;
	}

	// �o�b�t�@�փ��[�h
	if (!fio->Read(&buf, (int)sz)) {
		return FALSE;
	}

	// �p�X�����[�h
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	// �K���C�W�F�N�g
	Eject(TRUE);

	// ID����v�����ꍇ�̂݁A�ړ�
	if (disk.id != buf.id) {
		// �Z�[�u����CD-ROM�łȂ������B�C�W�F�N�g��Ԃ��ێ�
		return TRUE;
	}

	// �ăI�[�v�������݂�
	if (!Open(path, FALSE)) {
		// �ăI�[�v���ł��Ȃ��B�C�W�F�N�g��Ԃ��ێ�
		return TRUE;
	}

	// Open���Ńf�B�X�N�L���b�V���͍쐬����Ă���B�v���p�e�B�݈̂ړ�
	if (!disk.readonly) {
		disk.writep = buf.writep;
	}
	disk.lock = buf.lock;
	disk.attn = buf.attn;
	disk.reset = buf.reset;
	disk.lun = buf.lun;
	disk.code = buf.code;

	// �ēx�A�f�B�X�N�L���b�V����j��
	if (disk.dcache) {
		delete disk.dcache;
		disk.dcache = NULL;
	}
	disk.dcache = NULL;

	// �b��
	disk.blocks = track[0]->GetBlocks();
	if (disk.blocks > 0) {
		// �f�B�X�N�L���b�V������蒼��
		track[0]->GetPath(path);
		disk.dcache = new DiskCache(path, disk.size, disk.blocks);
		disk.dcache->SetRawMode(rawfile);

		// �f�[�^�C���f�b�N�X���Đݒ�
		dataindex = 0;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�I�[�v��
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::Open(const Filepath& path, BOOL attn)
{
	Fileio fio;
	DWORD size;
	char file[5];

	ASSERT(this);
	ASSERT(!disk.ready);

	// �������A�g���b�N�N���A
	disk.blocks = 0;
	rawfile = FALSE;
	ClearTrack();

	// �ǂݍ��݃I�[�v�����K�v
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// �T�C�Y�擾
	size = fio.GetFileSize();
	if (size <= 4) {
		fio.Close();
		return FALSE;
	}

	// CUE�V�[�g���AISO�t�@�C�����̔�����s��
	fio.Read(file, 4);
	file[4] = '\0';
	fio.Close();

	// FILE�Ŏn�܂��Ă���΁ACUE�V�[�g�Ƃ݂Ȃ�
	if (_strnicmp(file, "FILE", 4) == 0) {
		// CUE�Ƃ��ăI�[�v��
		if (!OpenCue(path)) {
			return FALSE;
		}
	}
	else {
		// ISO�Ƃ��ăI�[�v��
		if (!OpenIso(path)) {
			return FALSE;
		}
	}

	// �I�[�v������
	ASSERT(disk.blocks > 0);
	disk.size = 11;

	// ��{�N���X
	Disk::Open(path);

	// RAW�t���O��ݒ�
	ASSERT(disk.dcache);
	disk.dcache->SetRawMode(rawfile);

	// ROM���f�B�A�Ȃ̂ŁA�������݂͂ł��Ȃ�
	disk.writep = TRUE;

	// ���f�B�Ȃ�A�e���V����
	if (disk.ready && attn) {
		disk.attn = TRUE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�I�[�v��(CUE)
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::OpenCue(const Filepath& path)
{
	ASSERT(this);

	printf("%p", (const void*)&path);
	// ��Ɏ��s
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	�I�[�v��(ISO)
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::OpenIso(const Filepath& path)
{
	Fileio fio;
	DWORD size;
	BYTE header[12];
	BYTE sync[12];

	ASSERT(this);

	// �ǂݍ��݃I�[�v�����K�v
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// �T�C�Y�擾
	size = fio.GetFileSize();
	if (size < 0x800) {
		fio.Close();
		return FALSE;
	}

	// �ŏ���12�o�C�g��ǂݎ���āA�N���[�Y
	if (!fio.Read(header, sizeof(header))) {
		fio.Close();
		return FALSE;
	}

	// RAW�`�����`�F�b�N
	memset(sync, 0xff, sizeof(sync));
	sync[0] = 0x00;
	sync[11] = 0x00;
	rawfile = FALSE;
	if (memcmp(header, sync, sizeof(sync)) == 0) {
		// 00,FFx10,00�Ȃ̂ŁARAW�`���Ɛ��肳���
		if (!fio.Read(header, 4)) {
			fio.Close();
			return FALSE;
		}

		// MODE1/2048�܂���MODE1/2352�̂݃T�|�[�g
		if (header[3] != 0x01) {
			// ���[�h���Ⴄ
			fio.Close();
			return FALSE;
		}

		// RAW�t�@�C���ɐݒ�
		rawfile = TRUE;
	}
	fio.Close();

	if (rawfile) {
		// �T�C�Y��2536�̔{���ŁA700MB�ȉ��ł��邱��
		if (size % 0x930) {
			return FALSE;
		}
		if (size > 912579600) {
			return FALSE;
		}

		// �u���b�N����ݒ�
		disk.blocks = size / 0x930;
	}
	else {
		// �T�C�Y��2048�̔{���ŁA700MB�ȉ��ł��邱��
		if (size & 0x7ff) {
			return FALSE;
		}
		if (size > 0x2bed5000) {
			return FALSE;
		}

		// �u���b�N����ݒ�
		disk.blocks = size >> 11;
	}

	// �f�[�^�g���b�N1�̂ݍ쐬
	ASSERT(!track[0]);
	track[0] = new CDTrack(this);
	track[0]->Init(1, 0, disk.blocks - 1);
	track[0]->SetPath(FALSE, path);
	tracks = 1;
	dataindex = 0;

	// �I�[�v������
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::Inquiry(const DWORD *cdb, BYTE *buf)
{
	DWORD major;
	DWORD minor;
	char string[32];
	int size;
	int len;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD�`�F�b�N
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// ��{�f�[�^
	// buf[0] ... CD-ROM Device
	// buf[1] ... �����[�o�u��
	// buf[2] ... SCSI-2�����̃R�}���h�̌n
	// buf[3] ... SCSI-2������Inquiry���X�|���X
	// buf[4] ... Inquiry�ǉ��f�[�^
	memset(buf, 0, 8);
	buf[0] = 0x05;
	buf[1] = 0x80;
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 0x1f;

	// �x���_
	memset(&buf[8], 0x20, 28);
	memcpy(&buf[8], "XM6", 3);

	// ���i��
	memcpy(&buf[16], "CDU-55S", 7);

	// ���r�W����(XM6�̃o�[�W����No)
	ctrl->GetVM()->GetVersion(major, minor);
	sprintf(string, "0%01d%01d%01d",
				major, (minor >> 4), (minor & 0x0f));
	memcpy((char*)&buf[32], string, 4);

	// �T�C�Y36�o�C�g���A���P�[�V���������O�X�̂����A�Z�����œ]��
	size = 36;
	len = cdb[4];
	if (len < size) {
		size = len;
	}

	// ����
	disk.code = DISK_NOERROR;
	return size;
}

//---------------------------------------------------------------------------
//
//	READ
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::Read(BYTE *buf, int block)
{
	int index;
	Filepath path;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(block >= 0);

	// ��ԃ`�F�b�N
	if (!CheckReady()) {
		return 0;
	}

	// �g���b�N����
	index = SearchTrack(block);

	// �����Ȃ�A�͈͊O
	if (index < 0) {
		disk.code = DISK_INVALIDLBA;
		return 0;
	}
	ASSERT(track[index]);

	// ���݂̃f�[�^�g���b�N�ƈقȂ��Ă����
	if (dataindex != index) {
		// ���݂̃f�B�X�N�L���b�V�����폜(Save�̕K�v�͂Ȃ�)
		delete disk.dcache;
		disk.dcache = NULL;

		// �u���b�N�����Đݒ�
		disk.blocks = track[index]->GetBlocks();
		ASSERT(disk.blocks > 0);

		// �f�B�X�N�L���b�V������蒼��
		track[index]->GetPath(path);
		disk.dcache = new DiskCache(path, disk.size, disk.blocks);
		disk.dcache->SetRawMode(rawfile);

		// �f�[�^�C���f�b�N�X���Đݒ�
		dataindex = index;
	}

	// ��{�N���X
	ASSERT(dataindex >= 0);
	return Disk::Read(buf, block);
}

//---------------------------------------------------------------------------
//
//	READ TOC
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::ReadToc(const DWORD *cdb, BYTE *buf)
{
	int last;
	int index;
	int length;
	int loop;
	int i;
	BOOL msf;
	DWORD lba;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x43);
	ASSERT(buf);

	// ���f�B�`�F�b�N
	if (!CheckReady()) {
		return 0;
	}

	// ���f�B�ł���Ȃ�A�g���b�N���Œ�1�ȏ㑶�݂���
	ASSERT(tracks > 0);
	ASSERT(track[0]);

	// �A���P�[�V���������O�X�擾�A�o�b�t�@�N���A
	length = cdb[7] << 8;
	length |= cdb[8];
	memset(buf, 0, length);

	// MSF�t���O�擾
	if (cdb[1] & 0x02) {
		msf = TRUE;
	}
	else {
		msf = FALSE;
	}

	// �ŏI�g���b�N�ԍ����擾�A�`�F�b�N
	last = track[tracks - 1]->GetTrackNo();
	if ((int)cdb[6] > last) {
		// ������AA�͏��O
		if (cdb[6] != 0xaa) {
			disk.code = DISK_INVALIDCDB;
			return 0;
		}
	}

	// �J�n�C���f�b�N�X���`�F�b�N
	index = 0;
	if (cdb[6] != 0x00) {
		// �g���b�N�ԍ�����v����܂ŁA�g���b�N��i�߂�
		while (track[index]) {
			if ((int)cdb[6] == track[index]->GetTrackNo()) {
				break;
			}
			index++;
		}

		// ������Ȃ����AA���A�����G���[
		if (!track[index]) {
			if (cdb[6] == 0xaa) {
				// AA�Ȃ̂ŁA�ŏILBA+1��Ԃ�
				buf[0] = 0x00;
				buf[1] = 0x0a;
				buf[2] = (BYTE)track[0]->GetTrackNo();
				buf[3] = (BYTE)last;
				buf[6] = 0xaa;
				lba = track[tracks -1]->GetLast() + 1;
				if (msf) {
					LBAtoMSF(lba, &buf[8]);
				}
				else {
					buf[10] = (BYTE)(lba >> 8);
					buf[11] = (BYTE)lba;
				}
				return length;
			}

			// ����ȊO�̓G���[
			disk.code = DISK_INVALIDCDB;
			return 0;
		}
	}

	// ����Ԃ��g���b�N�f�B�X�N���v�^�̌�(���[�v��)
	loop = last - track[index]->GetTrackNo() + 1;
	ASSERT(loop >= 1);

	// �w�b�_�쐬
	buf[0] = (BYTE)(((loop << 3) + 2) >> 8);
	buf[1] = (BYTE)((loop << 3) + 2);
	buf[2] = (BYTE)track[0]->GetTrackNo();
	buf[3] = (BYTE)last;
	buf += 4;

	// ���[�v
	for (i=0; i<loop; i++) {
		// ADR��Control
		if (track[index]->IsAudio()) {
			// �I�[�f�B�I�g���b�N
			buf[1] = 0x10;
		}
		else {
			// �f�[�^�g���b�N
			buf[1] = 0x14;
		}

		// �g���b�N�ԍ�
		buf[2] = (BYTE)track[index]->GetTrackNo();

		// �g���b�N�A�h���X
		if (msf) {
			LBAtoMSF(track[index]->GetFirst(), &buf[4]);
		}
		else {
			buf[6] = (BYTE)(track[index]->GetFirst() >> 8);
			buf[7] = (BYTE)(track[index]->GetFirst());
		}

		// �o�b�t�@�ƃC���f�b�N�X��i�߂�
		buf += 8;
		index++;
	}

	// �A���P�[�V���������O�X�����K���Ԃ�
	return length;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::PlayAudio(const DWORD *cdb)
{
	ASSERT(this);
	printf("%p", (const void*)cdb);
	disk.code = DISK_INVALIDCDB;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO MSF
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::PlayAudioMSF(const DWORD *cdb)
{
	ASSERT(this);
	printf("%p", (const void*)cdb);
	disk.code = DISK_INVALIDCDB;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO TRACK
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::PlayAudioTrack(const DWORD *cdb)
{
	ASSERT(this);
	printf("%p", (const void*)cdb);
	disk.code = DISK_INVALIDCDB;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	LBA��MSF�ϊ�
//
//---------------------------------------------------------------------------
void FASTCALL SCSICD::LBAtoMSF(DWORD lba, BYTE *msf) const
{
	DWORD m;
	DWORD s;
	DWORD f;

	ASSERT(this);

	// 75�A75*60�ł��ꂼ��]����o��
	m = lba / (75 * 60);
	s = lba % (75 * 60);
	f = s % 75;
	s /= 75;

	// ��_��M=0,S=2,F=0
	s += 2;
	if (s >= 60) {
		s -= 60;
		m++;
	}

	// �i�[
	ASSERT(m < 0x100);
	ASSERT(s < 60);
	ASSERT(f < 75);
	msf[0] = 0x00;
	msf[1] = (BYTE)m;
	msf[2] = (BYTE)s;
	msf[3] = (BYTE)f;
}

//---------------------------------------------------------------------------
//
//	MSF��LBA�ϊ�
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCSICD::MSFtoLBA(const BYTE *msf) const
{
	DWORD lba;

	ASSERT(this);
	ASSERT(msf[2] < 60);
	ASSERT(msf[3] < 75);

	// 1, 75, 75*60�̔{���ō��Z
	lba = msf[1];
	lba *= 60;
	lba += msf[2];
	lba *= 75;
	lba += msf[3];

	// ��_��M=0,S=2,F=0�Ȃ̂ŁA150������
	lba -= 150;

	return lba;
}

//---------------------------------------------------------------------------
//
//	�g���b�N�N���A
//
//---------------------------------------------------------------------------
void FASTCALL SCSICD::ClearTrack()
{
	int i;

	ASSERT(this);

	// �g���b�N�I�u�W�F�N�g���폜
	for (i=0; i<TrackMax; i++) {
		if (track[i]) {
			delete track[i];
			track[i] = NULL;
		}
	}

	// �g���b�N��0
	tracks = 0;

	// �f�[�^�A�I�[�f�B�I�Ƃ��ݒ�Ȃ�
	dataindex = -1;
	audioindex = -1;
}

//---------------------------------------------------------------------------
//
//	�g���b�N����
//	��������Ȃ����-1��Ԃ�
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::SearchTrack(DWORD lba) const
{
	int i;

	ASSERT(this);

	// �g���b�N���[�v
	for (i=0; i<tracks; i++) {
		// �g���b�N�ɕ���
		ASSERT(track[i]);
		if (track[i]->IsValid(lba)) {
			return i;
		}
	}

	// ������Ȃ�����
	return -1;
}

//---------------------------------------------------------------------------
//
//	�t���[���ʒm
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::NextFrame()
{
	ASSERT(this);
	ASSERT((frame >= 0) && (frame < 75));

	// �t���[����0�`74�͈̔͂Őݒ�
	frame = (frame + 1) % 75;

	// 1��������FALSE
	if (frame != 0) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	CD-DA�o�b�t�@�擾
//
//---------------------------------------------------------------------------
void FASTCALL SCSICD::GetBuf(DWORD *buffer, int samples, DWORD rate)
{
	ASSERT(this);
	printf("%d", samples);
	printf("%p %lu", (const void*)buffer, (unsigned long)rate);
}
