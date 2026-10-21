#!/usr/bin/env python3
"""Verify the first ORIGINAL guest thread handoff with PRIVATE generated C.
Never commits or uploads the generated code, ROM or binaries.
The test deliberately mocks title boot protection and does not run the child.
"""
import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ENTRY_FINGERPRINT = 'e8eb7931b271583b977146912c5b10c91e67ee9d999c7a34a72caa21cc2fba5a'

def verify(root: Path, public_root: Path, cc: str, cxx: str):
    entry = root/'cpu/funcs_0.c'
    real = root/'include/recomp.h'
    decls = root/'cpu/recomp_decls.h'
    for file in (entry, real, decls, root/'cpu/funcs.h'):
        if not file.is_file(): raise ValueError('Missing private generated file: '+str(file))
    if hashlib.sha256(entry.read_bytes()).hexdigest() != ENTRY_FINGERPRINT:
        raise ValueError('Refusing unknown generated game-code fingerprint')
    runtime=public_root/'runtime'
    harness=public_root/'tests/private_original_boot_handoff.cpp'
    with tempfile.TemporaryDirectory(prefix='cw-first-thread-') as d:
        out=Path(d)
        compiler_opts=['-O1','-ffunction-sections','-fdata-sections','-fno-lto','-fno-strict-aliasing']
        commands=[
            [cc,'-std=c11',*compiler_opts,'-I',str(root/'include'),'-I',str(root/'cpu'),
             '-include',str(decls),'-c',str(entry),'-o',str(out/'generated.o')],
            [cxx,'-std=c++17',*compiler_opts,'-I',str(root/'include'),'-I',str(root/'cpu'),
             '-I',str(runtime),'-c',str(runtime/'boot_thread_handoff.cpp'),'-o',str(out/'bridge.o')],
            [cxx,'-std=c++17',*compiler_opts,'-I',str(root/'include'),'-I',str(root/'cpu'),
             '-I',str(runtime),'-c',str(harness),'-o',str(out/'probe.o')],
            [cxx,str(out/'generated.o'),str(out/'bridge.o'),str(out/'probe.o'),
             '-Wl,--gc-sections','-lm','-o',str(out/'test')]
        ]
        for args in commands:
            p=subprocess.run(args, text=True, capture_output=True, timeout=90)
            if p.returncode:raise RuntimeError('Command failed: '+str(args)+'\n'+p.stderr[-4000:])
        run=subprocess.run([str(out/'test')],text=True,capture_output=True,timeout=15)
        if run.returncode or not run.stdout.startswith('PASS:'):
            raise RuntimeError('Original handoff failed: '+run.stdout+'\n'+run.stderr)
        return {'status':'PASS','original_game_code_executed':True,
                'first_main_function_executed':True,'thread_created':True,'thread_started':True,
                'expected_child_entry':'0x802AD4C0','child_executed':False,
                'protection_seed_simulated':True,'runtime_linked':False,
                'android_binary':False,'playable':False,'stdout':run.stdout.strip()}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--private',required=True,type=Path)
    p.add_argument('--output',type=Path)
    p.add_argument('--cc',default='clang')
    p.add_argument('--cxx',default='clang++')
    args=p.parse_args()
    public_root=Path(__file__).resolve().parents[1]
    report=verify(args.private,public_root,args.cc,args.cxx)
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    try:main()
    except (ValueError,OSError,RuntimeError,subprocess.TimeoutExpired) as exc:
        print('ERROR:',exc,file=sys.stderr)
        sys.exit(1)
