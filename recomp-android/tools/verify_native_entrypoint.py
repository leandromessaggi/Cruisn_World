#!/usr/bin/env python3
"""Execute the game's original recompiled entrypoint against a bounded host RDRAM.

Consumes PRIVATE N64Recomp-generated sources from the user's verified US ROM.
No ROM, generated function bodies or derived binaries go to the public repository.
This is a bounded entrypoint smoke test; it does not run main, libultra, or graphics.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ORIGINAL_ENTRY_SHA = "e8eb7931b271583b977146912c5b10c91e67ee9d999c7a34a72caa21cc2fba5a"
ROM_SHA = "5320594232f0c492a672464832425351bd06497c95fb243745e2493bc5df9c2c"

def extract_zip_safely(path, destination):
    with zipfile.ZipFile(path) as z:
        for member in z.infolist():
            rel=Path(member.filename)
            if rel.is_absolute() or '..' in rel.parts or member.is_dir() and member.filename.startswith('/'):
                raise ValueError('Unsafe ZIP entry: ' + member.filename)
            if (member.is_dir()): continue
            if member.external_attr >> 16 & 0o170000 == 0o120000:
                raise ValueError('Symlink in ZIP: ' + member.filename)
            dest=destination/rel
            dest.parent.mkdir(parents=True,exist_ok=True)
            with z.open(member) as f, dest.open('wb') as out:
                shutil.copyfileobj(f,out)

def run_probe(source_root, harness, compiler='clang', output=None, allow_synthetic=False):
    cpu=source_root/'cpu'/'funcs_0.c'
    header=source_root/'cpu'/'funcs.h'
    recomp=source_root/'include'/'recomp.h'
    if not all(x.is_file() for x in (cpu,header,recomp,source_root/'cpu'/'recomp_decls.h')):
        raise RuntimeError('Missing generated funcs_0.c, funcs.h or upstream recomp.h')
    source_sha=hashlib.sha256(cpu.read_bytes()).hexdigest()
    if not allow_synthetic and source_sha != ORIGINAL_ENTRY_SHA:
        raise RuntimeError('Unexpected original entrypoint source fingerprint; refuse unverified code')
    with tempfile.TemporaryDirectory(prefix='cw-entry-') as tmp:
        binary=Path(tmp)/'guest_entrypoint_probe'
        args=[compiler,'-std=c11','-O1','-ffunction-sections','-fdata-sections',
              '-fno-lto','-fno-strict-aliasing','-I',str(source_root/'include'),
              '-I',str(source_root/'cpu'),'-include',str(source_root/'cpu'/'recomp_decls.h'),str(cpu),str(harness),'-Wl,--gc-sections',
              '-lm','-o',str(binary)]
        p=subprocess.run(args,capture_output=True,text=True)
        if p.returncode:
            raise RuntimeError('Compilation/link failure:\n'+p.stderr[-5000:])
        p=subprocess.run([str(binary)],capture_output=True,text=True,timeout=20)
        if p.returncode or not p.stdout.startswith('PASS:'):
            raise RuntimeError('Guest entrypoint did not pass:\n'+p.stdout+'\n'+p.stderr)
        report={'status':'PASS','test':'original_recompiled_entrypoint',
                'first_dispatch_pc':'0x802AD450','initial_sp':'0x80150400',
                'bss_bytes':0x94760,'first_dispatch_count':1,
                'original_code_executed':not allow_synthetic,'source_sha256':source_sha,
                'private_source_verified':not allow_synthetic,'main_executed':False,
                'android_binary':False,'gameplay_executable':False,'stdout':p.stdout.strip()}
        if output:
            Path(output).parent.mkdir(parents=True,exist_ok=True)
            Path(output).write_text(json.dumps(report,indent=2)+'\n')
        return report

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--input',required=True,help='Private generated source tree or .zip')
    ap.add_argument('--compiler',default=os.environ.get('CC','clang'))
    ap.add_argument('--output')
    ap.add_argument('--allow-synthetic',action='store_true',help='Test harness plumbing only; report is NOT a run of original code')
    ap.add_argument('--rom',help='Optional original ROM (.z64), verifies exact US revision SHA-256')
    a=ap.parse_args()
    if a.rom:
        if hashlib.sha256(Path(a.rom).read_bytes()).hexdigest()!=ROM_SHA:
            raise RuntimeError('ROM SHA-256 mismatch')
    harness=Path(__file__).resolve().parents[1]/'tests'/'native_entrypoint_probe.c'
    if a.input.endswith('.zip'):
        with tempfile.TemporaryDirectory(prefix='cw-private-') as d:
            extract_zip_safely(a.input,Path(d))
            report=run_probe(Path(d),harness,a.compiler,a.output,a.allow_synthetic)
    else:
        report=run_probe(Path(a.input),harness,a.compiler,a.output,a.allow_synthetic)
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    try:main()
    except (OSError,RuntimeError,ValueError,subprocess.TimeoutExpired,zipfile.BadZipFile) as e:
        print('ERROR:',e,file=sys.stderr);sys.exit(1)
