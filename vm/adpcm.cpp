//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
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
#include <new>

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
	adpcm.sync_rate = 441;
	adpcm.sync_step = 0x9c4000 / 441;
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

//---------------------------------------------------------------------------
//
//	Initialization
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
	adpcm.interp = adpcm.analog = adpcm.lpf = TRUE;

	// Reset the default table and runtime state
	adpcm.ratio = 0;
	adpcm.speed = adpcm.period = 0x400;
	adpcm.clock = 8;

	// Build the lookup table and apply the default settings
	MakeTable();
	SetVolume(50);

	// Initialize the audio buffer
	InitBuf(adpcm.sync_rate * 100);

	// Recalculate speed parameters
	CalcSpeed();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
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

	// Cleanup base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Reset()
{
    adpcm.play = adpcm.rec = adpcm.active = adpcm.started = FALSE;
    adpcm.panpot = adpcm.ratio = adpcm.data = 0;
    adpcm.offset = adpcm.sample = adpcm.out = 0;
    adpcm.clock = 8; adpcm.speed = 0x400;
    CalcSpeed(); adpcm.period = adpcm.speed; adpcm.stop_count = 0;
    InitBuf(adpcm.sync_rate * 100);
    event.SetUser(0); event.SetTime(0); event.SetDesc("Sampling");
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL ADPCM::Save(Fileio *fio, int ver)
{
    // Versioned payload uses fixed-width integers, including the entire ring.
    const DWORD magic = MAKEID('T','G','A','P'), version = 1, size = sizeof(adpcm);
    return fio->Write((void*)&magic, 4) && fio->Write((void*)&version, 4) &&
        fio->Write((void*)&size, 4) && fio->Write(&adpcm, sizeof(adpcm)) &&
        fio->Write(adpcmbuf, BufMax * 2 * sizeof(DWORD)) && event.Save(fio, ver);
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL ADPCM::Load(Fileio *fio, int ver)
{
    DWORD tag;
    if (!fio->Read(&tag, 4)) return FALSE;
    TypeGAdpcm::State loaded = {};
    DWORD *ring = new(std::nothrow) DWORD[BufMax * 2]();
    if (!ring) return FALSE;
    BOOL ok = FALSE;
    if (tag == MAKEID('T','G','A','P')) {
        DWORD version, bytes;
        ok = fio->Read(&version, 4) && version == 1 && fio->Read(&bytes, 4) &&
            bytes == sizeof(loaded) && fio->Read(&loaded, sizeof(loaded)) &&
            fio->Read(ring, BufMax * 2 * sizeof(DWORD));
    } else {
        // Previous builds wrote size_t then the unchanged 92-byte adpcm_t.
        adpcm_t old = {};
        DWORD upper = 0;
        ok = tag == sizeof(old);
        if (ok && sizeof(size_t) == 8) ok = fio->Read(&upper, 4) && upper == 0;
        if (ok) ok = fio->Read(&old, sizeof(old));
        if (ok) {
            memcpy(&loaded, &old, sizeof(old));
            loaded.sync_rate = old.sync_rate / 2;
            loaded.interp = old.interp != 0;
            loaded.analog = loaded.lpf = TRUE;
            loaded.period = loaded.speed;
            // Older files did not save a ring or analog history. Start empty,
            // retaining device registers, predictor and scheduled event.
            TypeGAdpcm::ResetBuffer(loaded, (int32_t*)ring, loaded.sync_rate * 100);
        }
    }
    ok = ok && loaded.offset >= 0 && loaded.offset <= 48 &&
        loaded.panpot >= 0 && loaded.panpot <= 3 &&
        (loaded.clock == 4 || loaded.clock == 8 || loaded.clock == 16) && loaded.ratio <= 2 &&
        loaded.period >= 64 && loaded.period <= 1024 &&
        loaded.sync_rate > 0 && loaded.sync_rate <= 10000 &&
        loaded.readpoint < BufMax && loaded.writepoint < BufMax && loaded.number <= BufMax &&
        loaded.history_index >= 0 && loaded.history_index < 6 &&
        loaded.trace_index >= 0 && loaded.trace_index < 512;
    if (ok) ok = event.Load(fio, ver);
    if (ok) { adpcm = loaded; memcpy(adpcmbuf, ring, BufMax * 2 * sizeof(DWORD)); }
    delete[] ring;
    return ok;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::ApplyCfg(const Config *config)
{
    adpcm.interp = config->adpcm_interp != 0;
    adpcm.analog = config->adpcm_analog != 0;
    adpcm.lpf = config->adpcm_lpf != 0;
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Assertion
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::AssertDiag() const
{
	// Base class
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
	ASSERT((adpcm.clock == 4) || (adpcm.clock == 8) || (adpcm.clock == 16));
	ASSERT((adpcm.ratio == 0) || (adpcm.ratio == 1) || (adpcm.ratio == 2));
	ASSERT((adpcm.speed & 0x003f) == 0);
	ASSERT((adpcm.speed >= 0x0080) && (adpcm.speed <= 0x400));
	ASSERT(adpcm.data < 0x100);
	ASSERT((adpcm.offset >= 0) && (adpcm.offset <= 48));
	ASSERT((adpcm.enable == TRUE) || (adpcm.enable == FALSE));
	ASSERT((adpcm.sound == TRUE) || (adpcm.sound == FALSE));
	ASSERT(adpcm.readpoint < BufMax);
	ASSERT(adpcm.writepoint < BufMax);
	ASSERT(adpcm.number <= BufMax);
	ASSERT(adpcm.sync_cnt < (adpcm.sync_step > 0x4000 ? adpcm.sync_step : 0x4000));
	ASSERT((adpcm.interp == TRUE)  || (adpcm.interp == FALSE));
	ASSERT(adpcmbuf);
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL ADPCM::ReadByte(DWORD addr)
{
    if (!(addr & 1)) return 0xff;
    scheduler->Wait(1);
    return (addr & 3) == 3 ? adpcm.data : adpcm.play ? 0x40 : 0xc0;
}

//---------------------------------------------------------------------------
//
//	Word read
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
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::WriteByte(DWORD addr, DWORD data)
{
    if (!(addr & 1)) return;
    scheduler->Wait(1);
    if ((addr & 3) == 3) { adpcm.data = data & 255; return; }
    if ((data & 7) == 2) {
        if (!adpcm.play) { adpcm.play = TRUE; adpcm.rec = FALSE; Start(0); }
    } else if ((data & 7) == 4) {
        if (!adpcm.rec) { adpcm.rec = TRUE; adpcm.play = FALSE; Start(1); }
    } else if (adpcm.play || adpcm.rec) Stop();
}

//---------------------------------------------------------------------------
//
//	Word write
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
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL ADPCM::ReadOnly(DWORD addr) const
{
    if (!(addr & 1)) return 0xff;
    return (addr & 3) == 3 ? adpcm.data : adpcm.play ? 0x40 : 0xc0;
}

//---------------------------------------------------------------------------
//
//	ADPCM data get
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::GetADPCM(adpcm_t *buffer)
{
    // Public diagnostic prefix remains compatible with previous clients.
    memcpy(buffer, &adpcm, sizeof(*buffer));
}

//---------------------------------------------------------------------------
//
//	Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL ADPCM::Callback(Event *ev)
{
    if (adpcm.wait <= 0) {
        do {
            DMAC::dma_t channel;
            dmac->GetDMA(3, &channel);
            if (ev->GetUser() == 0) {
                if (adpcm.active && channel.act && !channel.err) {
                    if (channel.hlt) return TRUE;
                    ++diag.req_total;
                    if (dmac->ReqDMA(3)) ++diag.req_ok; else ++diag.req_fail;
                }
                diag.last_data = adpcm.data;
                if (adpcm.data & 0x77) adpcm.started = TRUE;
                const DWORD frames = adpcm.period >> 6;
                Decode(adpcm.data, frames, TRUE);
                Decode(adpcm.data >> 4, frames, TRUE);
            } else {
                const DWORD frames = adpcm.period >> 6;
                adpcm.data = TypeGAdpcm::Encode(adpcm, codec_tables, (int32_t*)adpcmbuf, 8, frames) << 4;
                adpcm.data |= TypeGAdpcm::Encode(adpcm, codec_tables, (int32_t*)adpcmbuf, 0, frames);
                if (adpcm.active && channel.act && !channel.err && !channel.hlt) dmac->ReqDMA(3);
            }
            ++adpcm.wait;
        } while (adpcm.wait <= 0);
        adpcm.wait = 0;
    } else --adpcm.wait;
    if (adpcm.stop_count > 0 && --adpcm.stop_count == 0) return FALSE;
    CalcSpeed();
    if (ev->GetTime() != adpcm.period) ev->SetTime(adpcm.period);
    return TRUE;
}

//---------------------------------------------------------------------------
//
//	Master clock setting
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::SetClock(DWORD clk)
{
    adpcm.clock = (adpcm.clock_select && clk == 4) ? 16 : clk;
    CalcSpeed();
}

//---------------------------------------------------------------------------
//
//	Clock ratio setting
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::SetRatio(DWORD ratio)
{
	ASSERT(this);
	ASSERT((ratio >= 0) || (ratio <= 3));
	ASSERT_DIAG();

#if defined(ADPCM_LOG)
	LOG1(Log::Normal, "Speed ratio %d", ratio);
#endif	// ADPCM_LOG

	// ratio=3 is treated as 2
	if (ratio == 3) {
		LOG0(Log::Warning, "Illegal ratio setting $03");
		ratio = 2;
	}

	// Recalculate speed
	if (adpcm.ratio != ratio) {
		adpcm.ratio = ratio;
		CalcSpeed();
	}
}

//---------------------------------------------------------------------------
//
//	Panpot setting
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::SetPanpot(DWORD panpot)
{
	ASSERT(this);
	ASSERT((panpot >= 0) || (panpot <= 3));
	ASSERT_DIAG();

#if defined(ADPCM_LOG)
	LOG1(Log::Normal, "Panpot setting %d", panpot);
#endif	// ADPCM_LOG

	adpcm.panpot = panpot;
}

//---------------------------------------------------------------------------
//
//	Speed recalculation
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::CalcSpeed()
{
    TypeGAdpcm::CalcSpeed(adpcm);
}

//---------------------------------------------------------------------------
//
//	Record/playback start
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Start(int type)
{
    ++diag.start_events;
    adpcm.active = TRUE;
    adpcm.offset = adpcm.sample = 0;
    CalcSpeed();
    event.SetUser(type); event.SetTime(adpcm.speed);
    adpcm.stop_count = 0;
    Callback(&event);
}

//---------------------------------------------------------------------------
//
//	Record/playback stop
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Stop()
{
    ++diag.stop_events;
    adpcm.rec = adpcm.active = adpcm.play = FALSE;
    adpcm.stop_count = 4000000 / adpcm.speed;
}

//---------------------------------------------------------------------------
//
//	Next table
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Decode(int data, int num, BOOL valid)
{
    (void)valid;
    ++diag.decode_calls;
    TypeGAdpcm::Decode(adpcm, codec_tables, (int32_t*)adpcmbuf, data, num);
    if (adpcm.number > diag.max_buffer_samples) diag.max_buffer_samples = adpcm.number;
}

//---------------------------------------------------------------------------
//
//	Internal enable
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
//	Buffer initialization
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::InitBuf(DWORD rate)
{
    TypeGAdpcm::ResetBuffer(adpcm, (int32_t*)adpcmbuf, rate);
}

void FASTCALL ADPCM::SetArianshuuLoopFix(BOOL enabled)
{
    // Compatibility API only: TypeG commands/status/decay replace this old quirk.
    (void)enabled;
    quirk_arianshuu_loop_fix = FALSE;
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
//	Buffer audio data get
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::GetBuf(DWORD *buffer, int samples)
{
    if (adpcm.active && adpcm.rec) return;
    if (adpcm.number <= 1) ++diag.underrun_head_events;
    TypeGAdpcm::Resample(adpcm, (int32_t*)adpcmbuf, (uint32_t*)buffer, samples);
}

//---------------------------------------------------------------------------
//
//	Wait processing
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Wait(int num)
{
    // TypeG's host skips both output resampling and playback synchronization
    // while recording; the input ring must not control the playback wait.
    if (adpcm.active && adpcm.rec) return;
    if (!event.GetTime()) { adpcm.wait = 0; return; }
    const int divisor = (adpcm.speed >> 7) * adpcm.sync_rate;
    if (divisor <= 0) { adpcm.wait = 0; return; }
    const int count = (int)adpcm.number;
    adpcm.wait = count <= num ? -(((num - count) >> 8) * 625) / divisor
                             : (((count - num) >> 8) * 625) / divisor;
}

//---------------------------------------------------------------------------
//
//	Table creation
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::MakeTable()
{
    TypeGAdpcm::MakeTables(codec_tables);
}

//---------------------------------------------------------------------------
//
//	Volume setting
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::SetVolume(int volume)
{
    volume = volume < 0 ? 0 : volume > 100 ? 100 : volume;
    // TypeG sliders scale the level by TWO; zero is minimum gain, not mute.
    adpcm.vol = (int)(pow(10.0, (double)(volume * 2 - 140) * 0.005) * 16384.0);
}

// Host input is a 62.5 kHz stereo stream, as consumed by TypeG Encode.
void FASTCALL ADPCM::PutInput(const short *stereo, unsigned frames)
{
    if (!adpcm.active || !adpcm.rec || !adpcm.enable || !adpcm.sound || !stereo) return;
    for (unsigned i = 0; i < frames; ++i) {
        adpcmbuf[adpcm.writepoint * 2] = stereo[i * 2];
        adpcmbuf[adpcm.writepoint * 2 + 1] = stereo[i * 2 + 1];
        adpcm.writepoint = (adpcm.writepoint + 1) & 65535;
    }
    adpcm.number += frames;
    if (adpcm.number > BufMax) { adpcm.number = BufMax; adpcm.readpoint = adpcm.writepoint; }
}

void FASTCALL ADPCM::PutHostInput(const short *stereo, unsigned frames)
{
    if (stereo) TypeGAdpcm::Capture(adpcm, (int32_t*)adpcmbuf, stereo, frames);
}
