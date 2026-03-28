//----------------------------------------------------------------------------
//
//	X68Sound bridge implementation
//
//----------------------------------------------------------------------------

#include "x68sound_bridge.h"

#if defined(XM6CORE_ENABLE_X68SOUND)

#include "vm.h"
#include "memory.h"
#include "dmac.h"

namespace {

static Memory *g_memory = nullptr;
static bool g_started = false;

static int CALLBACK x68sound_mem_read(unsigned char *adrs)
{
	if (!g_memory) {
		return 0;
	}

	const std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(adrs);
	const DWORD addr = static_cast<DWORD>(raw & 0x00ffffffu);
	return static_cast<int>(g_memory->ReadByte(addr) & 0xffu);
}

static inline unsigned char compose_dcr(const DMAC::dma_t &dma)
{
	unsigned char data = static_cast<unsigned char>((dma.xrm & 0x03u) << 6);
	data |= static_cast<unsigned char>((dma.dtyp & 0x03u) << 4);
	if (dma.dps) {
		data |= 0x08;
	}
	data |= static_cast<unsigned char>(dma.pcl & 0x03u);
	return data;
}

static inline unsigned char compose_ocr(const DMAC::dma_t &dma)
{
	unsigned char data = 0;
	if (dma.dir) {
		data |= 0x80;
	}
	if (dma.btd) {
		data |= 0x40;
	}
	data |= static_cast<unsigned char>((dma.size & 0x03u) << 4);
	data |= static_cast<unsigned char>((dma.chain & 0x03u) << 2);
	data |= static_cast<unsigned char>(dma.reqg & 0x03u);
	return data;
}

static inline unsigned char compose_scr(const DMAC::dma_t &dma)
{
	unsigned char data = static_cast<unsigned char>((dma.mac & 0x03u) << 2);
	data |= static_cast<unsigned char>(dma.dac & 0x03u);
	return data;
}

static inline unsigned char compose_ccr(const DMAC::dma_t &dma)
{
	unsigned char data = 0;
	if (dma.intr) {
		data |= 0x08;
	}
	if (dma.hlt) {
		data |= 0x20;
	}
	if (dma.str) {
		data |= 0x80;
	}
	if (dma.cnt) {
		data |= 0x40;
	}
	if (dma.sab) {
		data |= 0x10;
	}
	return data;
}

} // namespace

namespace Xm6X68Sound {

bool Available()
{
	return true;
}

void SetMemory(Memory *memory)
{
	g_memory = memory;
	X68Sound_MemReadFunc(memory ? x68sound_mem_read : nullptr);
}

void Start(unsigned int sample_rate)
{
	X68Sound_Free();
	X68Sound_Reset();
	X68Sound_OpmClock(4000000);
	X68Sound_TotalVolume(256);
	const int rc = X68Sound_StartPcm(static_cast<int>(sample_rate), 1, 1, 5);
	X68Sound_MemReadFunc(g_memory ? x68sound_mem_read : nullptr);
	g_started = (rc >= 0);
	if (!g_started) {
		X68Sound_MemReadFunc(nullptr);
		X68Sound_Free();
	}
}

void Shutdown()
{
	X68Sound_MemReadFunc(nullptr);
	if (!g_started) {
		g_memory = nullptr;
		return;
	}
	X68Sound_Free();
	g_started = false;
	g_memory = nullptr;
}

void Reset()
{
	if (g_started) {
		X68Sound_Reset();
		X68Sound_MemReadFunc(g_memory ? x68sound_mem_read : nullptr);
	}
}

int GetPcm(void *buffer, int frames)
{
	if (!g_started) {
		return X68SNDERR_NOTACTIVE;
	}
	return X68Sound_GetPcm(buffer, frames);
}

void WriteOpm(unsigned char reg, unsigned char data)
{
	if (!g_started) {
		return;
	}
	X68Sound_OpmReg(reg);
	X68Sound_OpmPoke(data);
}

void WriteAdpcm(unsigned char data)
{
	if (!g_started) {
		return;
	}
	X68Sound_AdpcmPoke(data);
}

void WritePpi(unsigned char data)
{
	if (!g_started) {
		return;
	}
	X68Sound_PpiPoke(data);
}

void WritePpiCtrl(unsigned char data)
{
	if (!g_started) {
		return;
	}
	X68Sound_PpiCtrl(data);
}

void WriteDma(unsigned char adrs, unsigned char data)
{
	if (!g_started) {
		return;
	}
	X68Sound_DmaPoke(adrs, data);
}

void TimerA()
{
	if (!g_started) {
		return;
	}
	X68Sound_TimerA();
}

} // namespace Xm6X68Sound

#else

namespace Xm6X68Sound {

bool Available()
{
	return false;
}

void SetMemory(Memory * /*memory*/)
{
}

void Start(unsigned int /*sample_rate*/)
{
}

void Shutdown()
{
}

void Reset()
{
}

int GetPcm(void * /*buffer*/, int /*frames*/)
{
	return -1;
}

void WriteOpm(unsigned char /*reg*/, unsigned char /*data*/)
{
}

void WriteAdpcm(unsigned char /*data*/)
{
}

void WritePpi(unsigned char /*data*/)
{
}

void WritePpiCtrl(unsigned char /*data*/)
{
}

void WriteDma(unsigned char /*adrs*/, unsigned char /*data*/)
{
}

void TimerA()
{
}

} // namespace Xm6X68Sound

#endif
