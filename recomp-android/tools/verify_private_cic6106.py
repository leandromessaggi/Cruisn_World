#!/usr/bin/env python3
"""Verify real recompilation of two CIC-6106 boot checks, without publishing ROM.
Linux-only bounded test. Not the emulated CIC, not an Android gameplay runtime.
"""
import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROM_SHA='5320594232f0c492a672464832425351bd06497c95fb243745e2493bc5df9c2c'
CPU_SHA='e8eb7931b271583b977146912c5b10c91e67ee9d999c7a34a72caa21cc2fba5a'

def run(cmd):
    p=subprocess.run(cmd,text=True,capture_output=True,timeout=120)
    if p.returncode:
        raise RuntimeError('Command failed: '+ ' '.join(map(str,cmd))+'\n'+p.stderr[-6000:])
    return p.stdout

def verify(private,rom,compiler):
    private=Path(private).resolve()
    public=Path(__file__).resolve().parents[1]
    sources=private/'cpu/funcs_0.c'
    if hashlib.sha256(sources.read_bytes()).hexdigest()!=CPU_SHA:
        raise ValueError('Incorrect private function source fingerprint')
    if hashlib.sha256(Path(rom).read_bytes()).hexdigest()!=ROM_SHA:
        raise ValueError('Incorrect US ROM fingerprint')
    with tempfile.TemporaryDirectory(prefix='cw21-sp-check-') as t:
        t=Path(t)
        cflags=['-O1','-ffunction-sections','-fdata-sections','-fno-lto',
                '-fno-strict-aliasing']
        run(['clang','-std=c11',*cflags,'-I',str(private/'include'),
             '-I',str(private/'cpu'),'-include',str(private/'cpu/recomp_decls.h'),
             '-c',str(sources),'-o',str(t/'cpu.o')])
        run([compiler,'-std=c++17',*cflags,'-I',str(public/'runtime'),
             '-c',str(public/'runtime/cic6106_seed.cpp'),'-o',str(t/'seed.o')])
        run([compiler,'-std=c++17',*cflags,'-I',str(private/'include'),
             '-I',str(private/'cpu'),'-I',str(public/'runtime'),
             '-c',str(public/'tests/private_original_cic_checks.cpp'),'-o',str(t/'probe.o')])
        # --gc-sections allows uncalled recompiled routines to be discarded.
        # No fake-success stubs: the only two guest functions required must link.
        run([compiler,str(t/'cpu.o'),str(t/'seed.o'),str(t/'probe.o'),
             '-Wl,--gc-sections','-lm','-o',str(t/'test')])
        output=run([str(t/'test'),str(rom)]).strip()
        if not output.startswith('PASS'):raise RuntimeError(output)
    return {'result':'PASS','original_mips_checks_executed':2,
            'imem_expected_and_corrupt_paths_checked':True,
            'dmem_expected_and_corrupt_paths_checked':True,
            'ipl3_copied_to_cartridge_window':4096,
            'guest_threads_scheduled':False,'rom_included_in_public_repository':False,
            'android_gameplay_executable':False,'stdout':output}

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--private',required=True,type=Path)
    a.add_argument('--rom',required=True,type=Path)
    a.add_argument('--output',type=Path)
    args=a.parse_args()
    result=verify(args.private,args.rom,'clang++')
    if args.output:
        args.output.parent.mkdir(exist_ok=True,parents=True)
        args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':
    try: main()
    except (ValueError,RuntimeError,OSError,subprocess.TimeoutExpired) as e:
        print('ERROR:',e,file=sys.stderr)
        sys.exit(1)
