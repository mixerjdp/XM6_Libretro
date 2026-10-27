// XM6 TypeG YMFM integration, recovered from the pinned reference executable.
// The Yamaha synthesis core remains BSD-licensed ymfm (Aaron Giles).
#ifndef XM6_TYPEG_YMFM_H
#define XM6_TYPEG_YMFM_H
#include "ymfm/ymfm_opm.h"
#include "typeg_adpcm.h" // defined x86 wrap/shift arithmetic
#include <cstring>
#include <cmath>

namespace TypeGYmfm {
struct State {
    int32_t gain, denominator, step, phase, hold[2], filter_enabled;
    int32_t input[2], smooth[2], previous_input[2], reserved[6];
    int32_t scaled[2], previous_scaled[2], recursive[2], trace_index;
    int32_t trace[8][512];
};
static_assert(sizeof(State) == 0x4068, "TypeG YMFM signal state layout");

class Bridge : public ymfm::ymfm_interface {};
class Chip : public ymfm::ym2151 {
public:
    explicit Chip(ymfm::ymfm_interface &intf) : ym2151(intf) {}
    fm_engine &Engine() { return m_fm; }
    void PrepareChannel(unsigned n) { m_fm.debug_channel(n)->prepare(); }
    void PrepareAll() { for (unsigned n=0;n<8;++n) PrepareChannel(n); }
};

class Engine {
public:
    Engine() : chip_(bridge_) {
        std::memset(&state_,0,sizeof(state_));state_.denominator=state_.step=625;
    }
    bool Init(uint32_t clock, uint32_t rate, bool filter) {
        if (clock != 4000000) return false;
        state_.filter_enabled=filter;
        switch(rate) {
        case 44100:state_.denominator=882;state_.step=1250;break;
        case 48000:state_.denominator=192;state_.step=250;break;
        case 62500:state_.denominator=625;state_.step=625;break;
        case 88200:state_.denominator=882;state_.step=625;break;
        case 96000:state_.denominator=192;state_.step=125;break;
        }
        return true;
    }
    void Reset() {
        chip_.reset();
        const int32_t trace_index=state_.trace_index;
        state_.phase=0;state_.hold[0]=state_.hold[1]=0;
        std::memset(state_.input,0,sizeof(State)-offsetof(State,input));
        state_.trace_index=trace_index;
    }
    void SetReg(uint32_t reg,uint32_t value) {
        chip_.write_address(uint8_t(reg));chip_.write_data(uint8_t(value));
        if(reg==8)chip_.PrepareChannel(value&7);
    }
    void TimerA() { chip_.Engine().engine_timer_expired(0);chip_.PrepareAll(); }
    void SetVolume(int level) {
        int db=int(double(level*2-140)*0.2);if(db>20)db=20;
        state_.gain=db>-192?int(std::pow(10.0,double(db)*0.025)*16384.0):0;
    }
    void Generate(int32_t *out,unsigned frames) {
        // TypeG uses YM3012 mantissa/exponent truncation, not the old raw mode.
        for(unsigned i=0;i<frames;++i) {
            ymfm::ym2151::output_data sample;chip_.generate(&sample);
            out[i*2]=sample.data[0];out[i*2+1]=sample.data[1];
        }
    }
    void Mix(int32_t *out,unsigned frames) {
        using namespace TypeGAdpcm;
        for(unsigned i=0;i<frames;++i) {
            if(state_.denominator==625) Generate(state_.hold,1);
            else {
                state_.phase-=state_.step;
                if(state_.phase<=0) {
                    state_.phase+=state_.denominator;
                    if(state_.phase>0)Generate(state_.hold,1);
                    else {
                        state_.phase+=state_.denominator;int32_t pair[4];Generate(pair,2);
                        state_.hold[0]=add(pair[0],pair[2])/2;
                        state_.hold[1]=add(pair[1],pair[3])/2;
                    }
                }
            }
            out[i*2]=state_.hold[0];out[i*2+1]=state_.hold[1];
            for(unsigned ch=0;ch<8;++ch)
                state_.trace[ch][state_.trace_index]=chip_.Engine().debug_channel(ch)->debug_typeg_output();
            state_.trace_index=(state_.trace_index+1)&511;
        }
        for(unsigned i=0;i<frames;++i) {
            for(unsigned side=0;side<2;++side) {
                const int32_t sample=clamp(out[i*2+side],-65536,65535);
                if(!state_.filter_enabled) out[i*2+side]=clamp(sar(mul(sample,state_.gain),14),-32768,32767);
                else {
                    const int32_t scaled=int32_t(uint32_t(mul(sample,state_.gain))&0xffffc000u);
                    state_.scaled[side]=scaled;
                    int32_t recursive=state_.recursive[side];
                    recursive=add(add(add(recursive,-sar(recursive,12)),-sar(recursive,10)),
                                  add(scaled,int32_t(0u-uint32_t(state_.previous_scaled[side]))));
                    state_.recursive[side]=recursive;state_.previous_scaled[side]=scaled;
                    state_.input[side]=mul(sar(recursive,9),29);
                    const int32_t sum=add(add(mul(state_.smooth[side],70),state_.input[side]),state_.previous_input[side]);
                    state_.smooth[side]=sar(sum,7);state_.previous_input[side]=state_.input[side];
                    out[i*2+side]=clamp(sar(state_.smooth[side],5),-32768,32767);
                }
            }
        }
    }
    State &SignalState() { return state_; }
    Chip &Synthesis() { return chip_; }
    void SaveCheckpoint(std::vector<uint8_t> &out) {
        std::vector<uint8_t> core;ymfm::ymfm_saved_state saved(core,true);
        chip_.save_restore(saved);chip_.Engine().typeg_checkpoint(saved);
        out.resize(8+sizeof(State)+core.size());
        const uint32_t header[2]={0x31464d59,1};
        std::memcpy(out.data(),header,8);std::memcpy(out.data()+8,&state_,sizeof(State));
        std::memcpy(out.data()+8+sizeof(State),core.data(),core.size());
    }
    bool LoadCheckpoint(const std::vector<uint8_t> &in) {
        std::vector<uint8_t> previous;SaveCheckpoint(previous);
        if(in.size()!=previous.size() || std::memcmp(in.data(),previous.data(),8))return false;
        State restored;std::memcpy(&restored,in.data()+8,sizeof(State));
        if(restored.trace_index<0 || restored.trace_index>511 ||
           (restored.filter_enabled!=0 && restored.filter_enabled!=1) ||
           (restored.denominator!=882 && restored.denominator!=192 && restored.denominator!=625) ||
           restored.phase<0 || restored.phase>1250)return false;
        std::vector<uint8_t> core(in.begin()+8+sizeof(State),in.end());
        ymfm::ymfm_saved_state saved(core,false);chip_.save_restore(saved);chip_.Engine().typeg_checkpoint(saved);
        state_=restored;return true;
    }
private:
    Bridge bridge_;Chip chip_;State state_;
};
}
#endif
