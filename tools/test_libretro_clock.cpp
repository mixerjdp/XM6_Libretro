// Tests the shipped core's option callback and actual scheduler state.
// Deliberately stops at a missing test disk, before any guest CPU executes.
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include "../libretro/libretro.h"

static const char *selected=nullptr;
static unsigned optionVersion=2;
static std::string runtime,logs;
static bool registeredClock=false;
static void check(bool value,const char *message) { if(!value)throw std::runtime_error(message); }
static void logger(enum retro_log_level,const char *format,...) {
    char buffer[8192];va_list args;va_start(args,format);vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);logs+=buffer;
}
static bool environment(unsigned cmd,void *data) {
    switch(cmd) {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:static_cast<retro_log_callback*>(data)->log=logger;return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:*static_cast<unsigned*>(data)=optionVersion;return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:*static_cast<const char**>(data)=runtime.c_str();return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        auto var=static_cast<retro_variable*>(data);
        if(!strcmp(var->key,"xm6_cpu_clock") && selected) { var->value=selected;return true; }
        return false;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2: {
        auto options=static_cast<retro_core_options_v2*>(data);
        for(auto def=options->definitions;def->key;++def)
            if(!strcmp(def->key,"xm6_cpu_clock"))registeredClock=true;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_VARIABLES:
        for(auto var=static_cast<retro_variable*>(data);var->key;++var)
            if(!strcmp(var->key,"xm6_cpu_clock"))registeredClock=true;
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:return true;
    }
    return false;
}
template<class T> static T proc(HMODULE dll,const char *name) {
    auto p=reinterpret_cast<T>(GetProcAddress(dll,name));check(p!=nullptr,name);return p;
}
int main(int argc,char **argv) {
    try {
        check(argc==3,"usage: test_libretro_clock core.dll private-runtime-directory");
        runtime=argv[2];check(SetCurrentDirectoryA(runtime.c_str())!=0,"private runtime directory");
        HMODULE dll=LoadLibraryA(argv[1]);check(dll!=nullptr,"load Libretro DLL");
        auto setenv=proc<void(*)(retro_environment_t)>(dll,"retro_set_environment");
        auto init=proc<void(*)()>(dll,"retro_init");
        auto deinit=proc<void(*)()>(dll,"retro_deinit");
        auto load=proc<bool(*)(const retro_game_info*)>(dll,"retro_load_game");
        auto stateSize=proc<size_t(*)()>(dll,"retro_serialize_size");
        auto save=proc<bool(*)(void*,size_t)>(dll,"retro_serialize");
        struct Case { const char *option;int index;uint32_t speed;const char *label; };
        const Case cases[]={{"10mhz",0,979,"10mhz"},{"12mhz",1,1171,"12mhz"},
            {"16mhz",3,1556,"16mhz"},{"22mhz",5,2200,"22mhz"},
            {"25mhz",6,2500,"25mhz"},{"40mhz",7,4000,"40mhz"},
            {"invalid",6,2500,"25mhz"},{nullptr,6,2500,"25mhz"}};
        unsigned count=0;
        for(unsigned version: {2u,0u})for(const auto &test:cases) {
            selected=test.option;optionVersion=version;logs.clear();registeredClock=false;
            setenv(environment);init();
            const std::string missing=runtime+"/clock-test-deliberately-missing.xdf";
            check(GetFileAttributesA(missing.c_str())==INVALID_FILE_ATTRIBUTES,"fixture must not exist");
            retro_game_info info={};info.path=missing.c_str();
            check(!load(&info),"probe must stop before booting content");
            size_t n=stateSize();check(n>0,"VM initialized through Libretro option application");
            std::vector<unsigned char> b(n);check(save(b.data(),n),"serialize scheduler before boot");
            const unsigned char tag[]={'E','H','C','S'};
            auto at=std::search(b.begin(),b.end(),tag,tag+4);check(at!=b.end(),"scheduler tag");
            size_t pos=size_t(at-b.begin())+4,breakBytes,schBytes;
            memcpy(&breakBytes,&b[pos],sizeof(size_t));pos+=sizeof(size_t)+breakBytes;
            check(pos+sizeof(size_t)<=b.size(),"scheduler size offset");
            memcpy(&schBytes,&b[pos],sizeof(size_t));pos+=sizeof(size_t);
            check(schBytes>=28 && pos+schBytes+16384<=b.size(),"scheduler payload extent");
            int clock;uint32_t speed;memcpy(&clock,&b[pos+12],4);memcpy(&speed,&b[pos+16],4);
            if(clock!=test.index || speed!=test.speed) {
                fprintf(stderr,"FAIL %s: actual index=%d speed=%u; expected index=%d speed=%u\n",selected?selected:"missing",clock,speed,test.index,test.speed);
                deinit();return 1;
            }
            check(registeredClock,"clock option registered for this frontend API");
            check(logs.find(std::string("clock=")+test.label+" ")!=std::string::npos,"diagnostic clock label");
            for(unsigned i=0;i<4096;++i) {
                uint32_t cycle;memcpy(&cycle,&b[pos+schBytes+i*4],4);
                check(cycle==i*test.speed/200,"actual scheduler cycle table");
            }
            deinit();++count;
        }
        FreeLibrary(dll);
        printf("{\"status\":\"PASS\",\"bits\":%u,\"option_cases\":%u,\"cycle_table_entries_per_case\":4096,\"frontend_apis\":[2,0],\"guest_cpu_executed\":false}\n",unsigned(sizeof(void*)*8),count);
        return 0;
    }catch(const std::exception &e) { fprintf(stderr,"FAIL: %s\n",e.what());return 1; }
}
