"""Headless CPU throughput probe: only a synthetic 68000 loop, no game/GUI."""
from pathlib import Path
import ctypes as C, struct, os, json, time

ROOT=Path(__file__).resolve().parents[1]
RUNTIME=ROOT/'tools/typeg_audio/build/runtime'
LIB=ROOT/'main/Release_DLL_x64/xm6core.dll'
def main():
 os.chdir(RUNTIME)
 lib=C.CDLL(str(LIB));lib.xm6_create.restype=C.c_void_p
 lib.xm6_set_system_dir.argtypes=[C.c_char_p]
 for n,t in [('xm6_destroy',[C.c_void_p]),('xm6_reset',[C.c_void_p]),('xm6_set_system_clock',[C.c_void_p,C.c_int]),('xm6_exec',[C.c_void_p,C.c_uint]),('xm6_set_render_mode',[C.c_void_p,C.c_int]),('xm6_get_main_ram',[C.c_void_p,C.POINTER(C.c_uint)]),('xm6_state_size',[C.c_void_p,C.POINTER(C.c_uint)]),('xm6_save_state_mem',[C.c_void_p,C.c_void_p,C.c_uint]),('xm6_load_state_mem',[C.c_void_p,C.c_void_p,C.c_uint])]:getattr(lib,n).argtypes=t
 lib.xm6_get_main_ram.restype=C.c_void_p
 assert lib.xm6_set_system_dir((str(RUNTIME)+'\\').encode())==0
 h=lib.xm6_create();assert h
 results=[]
 def save():
  n=C.c_uint();assert lib.xm6_state_size(h,C.byref(n))==0
  b=C.create_string_buffer(n.value);assert lib.xm6_save_state_mem(h,b,n.value)==0;return b.raw
 def load(b):assert lib.xm6_load_state_mem(h,C.create_string_buffer(bytes(b)),len(b))==0
 def sched(b):
  a=b.index(b'EHCS')+4;nb=struct.unpack_from('<Q',b,a)[0];a+=8+nb
  ns=struct.unpack_from('<Q',b,a)[0];return a+8,ns
 def cpu(b):
  a=b.index(b' UPC')+4;assert struct.unpack_from('<Q',b,a)[0]==176;return a+8
 try:
  for render in (0,1):
   assert lib.xm6_set_render_mode(h,render)==0
   for clock in (0,1,2,3,4,5,6,7):
    assert lib.xm6_set_system_clock(h,clock)==0;assert lib.xm6_reset(h)==0
    b=bytearray(save());a=cpu(b);b[a:a+176]=bytes(176)
    struct.pack_into('<I',b,a+15*4,0x80000) # A7
    struct.pack_into('<I',b,a+17*4,0x1000) # PC
    struct.pack_into('<I',b,a+26*4,0x2700) # SR, IRQ masked
    load(b)
    n=C.c_uint();ram=lib.xm6_get_main_ram(h,C.byref(n));assert n.value>=0x80000
    # ADDQ.L #1,D0 (8 cycles); BRA.S loop (10 cycles), word-swapped RAM.
    C.memmove(ram+0x1000,struct.pack('<HH',0x5280,0x60fc),4)
    start=time.perf_counter()
    for _ in range(100):assert lib.xm6_exec(h,2000)==0
    elapsed=time.perf_counter()-start
    after=save();ca=cpu(after);sa,_=sched(after)
    speed=struct.unpack_from('<I',after,sa+16)[0]
    loops=struct.unpack_from('<I',after,ca)[0]
    hus=struct.unpack_from('<I',after,sa)[0]
    expected=(979,1171,1460,1556,1689,2200,2500,4000)[clock]
    assert speed==expected and hus==200000
    assert abs(loops-speed*1000/18)<=1,('68000 loop throughput',render,clock,loops,speed)
    results.append({'render':render,'clock':clock,'selected_speed':speed,'loops':loops,'virtual_hus':hus,'wall_seconds':elapsed})
  for render in (0,1):
   ordered=[r['loops'] for r in results if r['render']==render]
   assert ordered==sorted(ordered) and len(set(ordered))==8
  report={'status':'PASS','cases':results,'guest_cpu_executed':'Only the synthetic ADDQ.L/BRA.S loop; no game execution','scope':'Core CPU throughput at all eight scheduler clocks in both video modes'}
  (ROOT/'tools/typeg_audio/build/clock_throughput.json').write_text(json.dumps(report,indent=2)+'\n')
  print('PASS: sixteen synthetic CPU throughput cases; 10 MHz < 12 MHz')
 finally:lib.xm6_destroy(h)
if __name__=='__main__':main()
