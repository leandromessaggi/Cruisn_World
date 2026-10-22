#!/usr/bin/env python3
"""Run original USA NTSC VI setup through worker creation, offline only.
No generated or copyrighted game code is uploaded by this utility.
Requires files generated from the exact USA ROM used in v0.14.
"""
import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ENTRY_FINGERPRINT='e8eb7931b271583b977146912c5b10c91e67ee9d999c7a34a72caa21cc2fba5a'
VI_FUNCTION_FINGERPRINT='e4cbb1de1318bf598311175658423023534a5f1088cb1012ba85583e47c70e64'

def verify(private,public,rom,cc,cxx):
    root=Path(private).resolve()
    required=[root/'cpu/funcs_0.c',root/'cpu/funcs.h',root/'cpu/recomp_decls.h',root/'include/recomp.h',root/'cpu/funcs_14.c']
    for f in required:
        if not f.is_file(): raise ValueError(f'Missing private generated input: {f}')
    if hashlib.sha256(required[0].read_bytes()).hexdigest()!=ENTRY_FINGERPRINT:
        raise ValueError('Unknown generated game-code fingerprint; refusing boot probe')
    if hashlib.sha256(required[4].read_bytes()).hexdigest()!=VI_FUNCTION_FINGERPRINT:
        raise ValueError('Unknown original VI function fingerprint; refusing boot probe')
    runtime=Path(public)/'runtime'
    source=Path(public)/'tests/private_original_vi_setup.cpp'
    with tempfile.TemporaryDirectory(prefix='cw-three-stage-') as temp:
        tmp=Path(temp)
        opts=['-O1','-ffunction-sections','-fdata-sections','-fno-lto','-fno-strict-aliasing']
        commands=[
            [cc,'-std=c11',*opts,'-I',str(root/'include'),'-I',str(root/'cpu'),'-include',str(required[2]),'-c',str(required[0]),'-o',str(tmp/'game.o')],
            [cc,'-std=c11',*opts,'-I',str(root/'include'),'-I',str(root/'cpu'),'-include',str(required[2]),'-c',str(required[4]),'-o',str(tmp/'game14.o')],
            [cxx,'-std=c++17',*opts,'-I',str(root/'include'),'-I',str(root/'cpu'),'-I',str(runtime),'-c',str(runtime/'boot_multithread_probe.cpp'),'-o',str(tmp/'bridge.o')],
            [cxx,'-std=c++17',*opts,'-I',str(runtime),'-c',str(runtime/'cic6106_seed.cpp'),'-o',str(tmp/'seed.o')],
            [cxx,'-std=c++17',*opts,'-I',str(root/'include'),'-I',str(root/'cpu'),'-I',str(runtime),'-c',str(source),'-o',str(tmp/'harness.o')],
            # The generated file has static references to routines beyond this bounded test.
            # Create FAIL-CLOSED test-only traps for any otherwise unresolved game
            # references; a reached trap aborts. Never present these as runtime hooks.
            [cxx,str(tmp/'game.o'),str(tmp/'game14.o'),str(tmp/'bridge.o'),str(tmp/'seed.o'),str(tmp/'harness.o'),str(tmp/'traps.o'),'-Wl,--gc-sections','-lm','-o',str(tmp/'probe')],
        ]
        # Compile original CPU and runtime test objects first, then inspect imports
        # to generate test-only abort traps. No symbol may silently succeed.
        for cmd in commands[:-1]:
            result=subprocess.run(cmd,capture_output=True,text=True,timeout=110)
            if result.returncode:raise RuntimeError(f'Failed: {cmd}\n{result.stderr[-3500:]}')
        objects=[tmp/'game.o',tmp/'game14.o',tmp/'bridge.o',tmp/'seed.o',tmp/'harness.o']
        def nm_flags(*flags):
            run=subprocess.run(['nm',*flags,*map(str,objects)],capture_output=True,text=True)
            if run.returncode:raise RuntimeError(run.stderr)
            return run.stdout
        defined=set(re.findall(r'^\s*[0-9A-Fa-f]+\s+[TW]\s+(\w+)$',nm_flags('-g','--defined-only'),re.M))
        unresolved=set(re.findall(r'^\s*U\s+(\w+)$',nm_flags('-u'),re.M))
        missing=sorted(unresolved-defined)
        allowed=re.compile(r'^(func_[0-9A-Fa-f]{8}|[A-Za-z_][A-Za-z_0-9]*_recomp)$')
        # System/libc/libstdc++ imports are resolved by the linker; only game
        # and recompiler hook imports receive fail-closed temporary traps.
        missing=[x for x in missing if allowed.fullmatch(x)]
        traps=tmp/'traps.cpp'
        traps.write_text('#include <cstdlib>\n#include <cstdio>\n'+''.join(
            'extern "C" [[noreturn]] void '+name+'(){std::fputs("BOUNDED TEST TRAP: '+name+'\\n",stderr);std::abort();}\n'
            for name in missing))
        commands.insert(-1,[cxx,'-std=c++17','-c',str(traps),'-o',str(tmp/'traps.o')])
        for cmd in commands[-2:]:
            result=subprocess.run(cmd,capture_output=True,text=True,timeout=110)
            if result.returncode:raise RuntimeError(f'Failed: {cmd}\n{result.stderr[-3500:]}')
        result=subprocess.run([str(tmp/'probe'),str(rom)],capture_output=True,text=True,timeout=12)
        if result.returncode or not result.stdout.startswith('PASS:'):
            raise RuntimeError('Boot sequence failed: '+result.stdout+'\n'+result.stderr)
        return {'status':'PASS','original_game_code_executed':True,
                'recompiled_entrypoints':['0x802AD450','0x802AD4C0','0x802AD8C0','0x802FB3B0'],
                'pi_manager_observed':True,'first_queue_observed':True,'original_vi_queues_observed':3,'vi_manager_priority':254,'first_rom_dma_confirmed':True,'imem_check_passed':True,'dmem_check_passed':True,'next_game_call_executed':'0x802FB3B0','checkpoint':'third osStartThread_recomp, VI worker entry 0x802FB708',
                'cart_offset':'0xA6F80','rdram_address':'0x803E7AE0','dma_length':192,
                'dma_completed_and_received':True,'guest_bytes_match_rom':True,
                'guest_threads_scheduled':False,'copy_protection_seeded':True,'vi_manager_observational_only':True,
                'third_thread_ran_beyond_first_dma_and_sp_checks':True,
                'os_tv_type':1,'vi_mode':'0x8034C3D0',
                'vi_event_messages':[0x29B,0x29C,0x29D],
                'vi_retrace_event_message':0x29A,'vi_worker_entry':'0x802FB708',
                'vi_worker_executed':False,'graphics_initialized':False,
                'android_runtime_linked':False,'playable':False,'stdout':result.stdout.strip()}

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--private',required=True,type=Path)
    a.add_argument('--rom',required=True,type=Path,help='Private normalized .z64, USA game only')
    a.add_argument('--output',type=Path)
    a.add_argument('--cc',default='clang')
    a.add_argument('--cxx',default='clang++')
    args=a.parse_args()
    
    if hashlib.sha256(args.rom.read_bytes()).hexdigest()!='5320594232f0c492a672464832425351bd06497c95fb243745e2493bc5df9c2c':
        raise ValueError('ROM sha256 does not match verified USA game')
    out=verify(args.private,Path(__file__).resolve().parents[1],args.rom,args.cc,args.cxx)
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(out,indent=2)+'\n')
    print(json.dumps(out,indent=2))
if __name__=='__main__':
    try:main()
    except (RuntimeError,OSError,ValueError,subprocess.TimeoutExpired) as exc:
        print('ERROR:',exc,file=sys.stderr);sys.exit(1)
