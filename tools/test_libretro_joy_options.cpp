// Production Libretro callbacks + synthetic guest PPI reads, no GUI/game.
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <stdexcept>
#include "../libretro/libretro.h"
using Bytes=std::vector<unsigned char>;
static std::map<std::string,std::string> options;
static std::string runtime;
static bool updated=false;
static unsigned pressed=0;
static void check(bool ok,const char *s) { if(!ok)throw std::runtime_error(s); }
static bool environment(unsigned cmd,void *p) {
    switch(cmd) {
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:*static_cast<unsigned*>(p)=2;return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:*static_cast<const char**>(p)=runtime.c_str();return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        auto v=static_cast<retro_variable*>(p);auto found=options.find(v->key);
        if(found==options.end())return false;v->value=found->second.c_str();return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:*static_cast<bool*>(p)=updated;updated=false;return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:return true;
    }
    return false;
}
static void poll() {}
static int16_t input(unsigned,unsigned device,unsigned,unsigned id) {
    return device==RETRO_DEVICE_JOYPAD && id<16 && (pressed&(1u<<id)) ? 1:0;
}
static void video(const void*,unsigned,unsigned,size_t) {}
static size_t audio(const int16_t*,size_t frames) { return frames; }
template<class T> static T proc(HMODULE dll,const char *name) {
    T p=reinterpret_cast<T>(GetProcAddress(dll,name));check(p!=nullptr,name);return p;
}
static size_t payload(const Bytes &b,const char *tag) {
    auto pos=std::search(b.begin(),b.end(),tag,tag+4);check(pos!=b.end(),"device tag missing");
    size_t a=size_t(pos-b.begin())+4,n;check(a+sizeof(size_t)<=b.size(),"payload extent");
    memcpy(&n,&b[a],sizeof(n));a+=sizeof(n);check(a+n<=b.size(),"payload bounds");return a;
}
int main(int argc,char **argv) {
 try {
    check(argc==3,"usage: test_libretro_joy_options core.dll private-runtime-directory");
    runtime=argv[2];check(SetCurrentDirectoryA(runtime.c_str())!=0,"private cwd");
    const std::string disk=runtime+"/joy-options-blank.xdf";
    check(GetFileAttributesA(disk.c_str())==INVALID_FILE_ATTRIBUTES,"blank fixture already exists");
    FILE *f=fopen(disk.c_str(),"wb");check(f!=nullptr,"blank disk creation");
    Bytes blank(1261568);check(fwrite(blank.data(),1,blank.size(),f)==blank.size(),"blank disk write");fclose(f);
    HMODULE dll=LoadLibraryA(argv[1]);check(dll!=nullptr,"load actual core");
    auto init=proc<void(*)()>(dll,"retro_init"),deinit=proc<void(*)()>(dll,"retro_deinit"),run=proc<void(*)()>(dll,"retro_run");
    auto unload=proc<void(*)()>(dll,"retro_unload_game");
    auto load=proc<bool(*)(const retro_game_info*)>(dll,"retro_load_game");
    auto size=proc<size_t(*)()>(dll,"retro_serialize_size");
    auto save=proc<bool(*)(void*,size_t)>(dll,"retro_serialize");
    auto restore=proc<bool(*)(const void*,size_t)>(dll,"retro_unserialize");
    auto memory=proc<void*(*)(unsigned)>(dll,"retro_get_memory_data");
    options={{"xm6_joy1_type","cpsf_sfc"},{"xm6_joy2_type","cpsf_sfc"},{"xm6_exec_mode","legacy_exec"},
             {"xm6_cpu_clock","10mhz"},{"xm6_pad_start_select","disabled"},{"xm6_render_mode","original"}};
    proc<void(*)(retro_environment_t)>(dll,"retro_set_environment")(environment);
    proc<void(*)(retro_input_poll_t)>(dll,"retro_set_input_poll")(poll);
    proc<void(*)(retro_input_state_t)>(dll,"retro_set_input_state")(input);
    proc<void(*)(retro_video_refresh_t)>(dll,"retro_set_video_refresh")(video);
    proc<void(*)(retro_audio_sample_batch_t)>(dll,"retro_set_audio_sample_batch")(audio);
    init();retro_game_info info={};info.path=disk.c_str();check(load(&info),"load blank disk");
    for(unsigned i=0;i<50;++i)run();
    auto snapshot=[&]() { Bytes b(size());check(!b.empty()&&save(b.data(),b.size()),"snapshot");return b; };
    Bytes b=snapshot();size_t cpu=payload(b," UPC");memset(&b[cpu],0,176);
    uint32_t sp=0x80000,pc=0x1000,sr=0x2700;
    memcpy(&b[cpu+15*4],&sp,4);memcpy(&b[cpu+17*4],&pc,4);memcpy(&b[cpu+26*4],&sr,4);
    check(restore(b.data(),b.size()),"install synthetic CPU context");
    auto ram=static_cast<unsigned char*>(memory(RETRO_MEMORY_SYSTEM_RAM));check(ram!=nullptr,"main RAM");
    // Read both ports with PC4/PC5 low and high, storing four bytes in RAM.
    const uint16_t code[]={0x13fc,0,0x00e9,0xa005,
        0x13f9,0x00e9,0xa001,0,0x2000,0x13f9,0x00e9,0xa003,0,0x2001,
        0x13fc,0x30,0x00e9,0xa005,
        0x13f9,0x00e9,0xa001,0,0x2002,0x13f9,0x00e9,0xa003,0,0x2003,0x60c6};
    memcpy(ram+0x1000,code,sizeof(code));
    for(unsigned i=0;i<180;++i)run();
    const unsigned ids[]={RETRO_DEVICE_ID_JOYPAD_X,RETRO_DEVICE_ID_JOYPAD_Y,RETRO_DEVICE_ID_JOYPAD_A,
        RETRO_DEVICE_ID_JOYPAD_B,RETRO_DEVICE_ID_JOYPAD_R,RETRO_DEVICE_ID_JOYPAD_L,
        RETRO_DEVICE_ID_JOYPAD_START,RETRO_DEVICE_ID_JOYPAD_SELECT};
    const unsigned banks[]={1,1,0,0,1,1,1,1},masks[]={2,4,0x40,0x20,0x20,1,0x40,8};
    const std::pair<const char*,const char*> changes[]={
        {"xm6_fm_volume","60"},{"xm6_fm_volume","50"},
        {"xm6_adpcm_volume","60"},{"xm6_cpu_clock","12mhz"},{"xm6_cpu_clock","10mhz"},
        {"xm6_fast_floppy","enabled"},{"xm6_fast_floppy","disabled"},
        {"xm6_alt_raster","disabled"},{"xm6_alt_raster","enabled"},
        {"xm6_audio_engine","YMFM"},{"xm6_fm_interp","disabled"},{"xm6_ymfm_rate","48000"},
        {"xm6_reverb","10"},{"xm6_reverb","0"},{"xm6_eq_bass","60"},{"xm6_surround","enabled"},
        {"xm6_bass_enhancer","10"},{"xm6_legacy_dmac_cnt","enabled"},
        {"xm6_mouse_speed","200"},{"xm6_mouse_swap","enabled"},
        {"xm6_midi_output","enabled"},{"xm6_adpcm_interp","disabled"},
        {"xm6_adpcm_analog","disabled"},{"xm6_adpcm_lpf","disabled"}};
    unsigned checks=0;
    for(const auto &change:changes) {
        options[change.first]=change.second;updated=true;
        for(unsigned button=0;button<8;++button) {
            pressed=1u<<ids[button];run();b=snapshot();size_t ppi=payload(b," IPP");
            uint32_t t[2];memcpy(t,&b[ppi+4],8);
            if(t[0]!=7 || t[1]!=7) {
                fprintf(stderr,"FAIL after %s=%s: PPI ports became %u/%u instead of CPSF-SFC 7/7\n",change.first,change.second,t[0],t[1]);
                unload();deinit();FreeLibrary(dll);DeleteFileA(disk.c_str());return 1;
            }
            for(unsigned port=0;port<2;++port)for(unsigned bank=0;bank<2;++bank) {
                unsigned want=bank==banks[button]?(0xffu&~masks[button]):0xffu;
                unsigned actual=ram[(0x2000+bank*2+port)^1];
                if(actual!=want) {
                    fprintf(stderr,"FAIL PPI bytes after %s: button=%u port=%u bank=%u actual=%02X want=%02X\n",change.first,button,port,bank,actual,want);
                    throw std::runtime_error("CPSF-SFC protocol output changed");
                }
            }
            ++checks;
        }
    }
    unload();deinit();FreeLibrary(dll);check(DeleteFileA(disk.c_str())!=0,"remove own blank fixture");
    printf("{\"status\":\"PASS\",\"bits\":%u,\"option_changes\":24,\"button_cases\":%u,\"ports\":2,\"checks\":\"Both actual PPI types and four guest-observed bank bytes per button\",\"game_executed\":false}\n",unsigned(sizeof(void*)*8),checks);
    return 0;
 }catch(const std::exception &e) { fprintf(stderr,"FAIL: %s\n",e.what());return 1; }
}
