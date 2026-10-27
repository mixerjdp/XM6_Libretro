#include "ymfm_opm_engine.h"
#if defined(XM6CORE_ENABLE_YMFM)
#include "typeg_ymfm.h"
#include <new>

namespace {
class TypeGYmfmOpmEngine : public FM::OPM {
public:
    bool Init(uint clock, uint rate, bool interpolation) override {
        return engine_.Init(clock, rate, interpolation);
    }
    bool SetRate(uint clock, uint rate, bool interpolation) override {
        return engine_.Init(clock, rate, interpolation);
    }
    bool UseRawModeReg() const override { return true; }
    void Reset() override { engine_.Reset(); }
    void SetReg(uint reg, uint value) override { engine_.SetReg(reg, value); }
    void Mix(FM::Sample *buffer, int frames) override {
        if(frames>0) engine_.Mix(buffer, unsigned(frames));
    }
    void SetVolume(int level) override { engine_.SetVolume(level); }
    void TimerA() override { engine_.TimerA(); }
    unsigned AudioStateKind() const override { return 0x594d5447; }
    void SaveAudioState(std::vector<unsigned char> &out) override { engine_.SaveCheckpoint(out); }
    bool LoadAudioState(const std::vector<unsigned char> &in) override { return engine_.LoadCheckpoint(in); }
private:
    TypeGYmfm::Engine engine_;
};
}

BOOL YmfmOpmEngineSupportsRate(uint clock, uint rate)
{
    return clock==4000000 && (rate==44100 || rate==48000 || rate==62500 ||
                             rate==88200 || rate==96000);
}

FM::OPM *CreateYmfmOpmEngine(BOOL /*legacy_direct_mode*/)
{
    // The historical direct/raw selector is an alias of faithful TypeG YMFM.
    return new(std::nothrow) TypeGYmfmOpmEngine;
}
#endif // XM6CORE_ENABLE_YMFM
