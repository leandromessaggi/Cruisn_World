#!/usr/bin/env python3
"""Compile private N64Recomp C to an ARM64 Android *static archive*.

Does NOT link a runtime, include an actual N64 ROM, or produce a playable APK.
Input: the private export from prepare_full_recomp.py or a ZIP containing
cpu/*.c, cpu/funcs.h, cpu/recomp_decls.h, include/recomp.h.
"""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from hashlib import sha256
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile


def extract_archive(archive: Path, target: Path):
    with zipfile.ZipFile(archive) as z:
        for item in z.infolist():
            path = Path(item.filename)
            if path.is_absolute() or '..' in path.parts or item.is_dir() and item.file_size:
                raise ValueError('Unsafe ZIP entry: ' + item.filename)
            if item.is_dir():
                continue
            out = target / path
            out.parent.mkdir(parents=True, exist_ok=True)
            with z.open(item) as stream, out.open('wb') as dest:
                shutil.copyfileobj(stream, dest)


def ndk_bin(ndk: Path, api: int):
    base = ndk / 'toolchains' / 'llvm' / 'prebuilt'
    roots = sorted(x for x in base.glob('*') if x.is_dir())
    if not roots:
        raise FileNotFoundError('Android NDK LLVM toolchain not found under ' + str(base))
    for root in roots:
        bindir = root / 'bin'
        cc = bindir / ('aarch64-linux-android' + str(api) + '-clang')
        ar = bindir / 'llvm-ar'
        nm = bindir / 'llvm-nm'
        readelf = bindir / 'llvm-readelf'
        if all(x.exists() for x in (cc, ar, nm, readelf)):
            return cc, ar, nm, readelf
    raise FileNotFoundError('NDK requires aarch64-linux-android' + str(api) + '-clang and LLVM tools')


def run(args):
    return subprocess.run([str(x) for x in args], text=True, capture_output=True)


def compile_source(cc, src, obj, include, cpu, extra_flags):
    cmd = [cc, '-std=gnu11', '-O1', '-fPIC', '-fno-lto', '-Werror=implicit-function-declaration', '-Wno-unused-variable',
           '-Wno-unused-label', '-fno-strict-aliasing', '-I', include, '-I', cpu,
           '-include', cpu / 'recomp_decls.h',
           *extra_flags, '-c', src, '-o', obj]
    r = run(cmd)
    return src.name, r.returncode, r.stderr[-6000:]


def build(args):
    ndk = Path(args.ndk or os.getenv('ANDROID_NDK_HOME') or os.getenv('ANDROID_NDK_ROOT') or '')
    if not str(ndk) or not ndk.is_dir():
        raise RuntimeError('Supply --ndk /path/to/android-ndk (or ANDROID_NDK_HOME)')
    cc, ar, nm, readelf = ndk_bin(ndk, args.api)
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='cw-android-cpu-') as temp:
        if args.input.lower().endswith('.zip'):
            extract_archive(Path(args.input), Path(temp))
            source_root = Path(temp)
        else:
            source_root = Path(args.input).resolve()
        cpu = source_root / 'cpu'
        include = source_root / 'include'
        sources = sorted(cpu.glob('funcs_*.c'), key=lambda p: int(re.fullmatch(r'funcs_(\d+)\.c',p.name).group(1)))
        if len(sources) < args.min_source_count:
            raise RuntimeError(f'Expected at least {args.min_source_count} funcs_*.c files, found {len(sources)}')
        for p in (cpu/'funcs.h', cpu/'recomp_decls.h', include/'recomp.h'):
            if not p.exists():
                raise RuntimeError('Missing required source: ' + str(p))
        objs = output/'objects'
        objs.mkdir(exist_ok=True)
        failures = []
        with ThreadPoolExecutor(max_workers=max(1,args.jobs)) as pool:
            futures = [pool.submit(compile_source, cc, f, objs/(f.stem+'.o'),
                                   include, cpu, []) for f in sources]
            for fut in as_completed(futures):
                name, code, diagnostic = fut.result()
                if code:
                    failures.append((name, diagnostic))
        if failures:
            for f,diagnostic in failures[:8]:
                print(f'Compile failed {f}:\n{diagnostic}',file=sys.stderr)
            raise RuntimeError(f'{len(failures)} source files failed to compile')
        target = output/'libcw_recompiled_cpu_arm64.a'
        objects = sorted(objs.glob('*.o'))
        for obj in objects:
            e = run([readelf,'-h',obj]);
            if e.returncode or 'AArch64' not in e.stdout:
                raise RuntimeError('Not an AArch64 object: '+str(obj)+' '+e.stderr)
        r = run([ar,'rcs',target,*objects])
        if r.returncode:
            raise RuntimeError('llvm-ar failed: '+r.stderr)
        sym=run([nm,'-g',target])
        if sym.returncode: raise RuntimeError('llvm-nm failed: '+sym.stderr)
        defined = set(re.findall(r'^\s*[0-9a-fA-F]+\s+[TWVD]\s+(\w+)$',sym.stdout,re.M))
        needed = set(re.findall(r'^\s*U\s+(\w+)$',sym.stdout,re.M))
        unresolved = sorted(needed - defined)
        (output/'unresolved_symbols.txt').write_text('\n'.join(unresolved)+'\n')
        report={
            'target':'aarch64-linux-android'+str(args.api),
            'source_count':len(sources),
            'recomp_function_bodies':sum(len(re.findall(r'RECOMP_FUNC\s+void\s+\w+\s*\(',f.read_text(errors='replace'))) for f in sources),
            'object_count':len(objects),
            'archive_bytes':target.stat().st_size,
            'archive_sha256':sha256(target.read_bytes()).hexdigest(),
            'recompiled_symbols':sum(x.startswith('func_') or x.startswith('recomp_') for x in defined),
            'unresolved_symbol_count':len(unresolved),
            'unresolved_symbols':unresolved,
            'declared_hooks_header':True,
            'runtime_linked':False,
            'gameplay_executable':False,
            'explanation':'ARM64 object compilation only: undefined runtime functions are expected; not a playable APK.',
        }
        (output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps({k:v for k,v in report.items() if k!='unresolved_symbols'},indent=2))
        if not args.keep_objects:
            shutil.rmtree(objs)
    return 0

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--input',required=True,help='Private recompilation ZIP or extracted directory')
    p.add_argument('--ndk',default='',help='Path to installed Android NDK')
    p.add_argument('--output',required=True)
    p.add_argument('--api',type=int,default=28)
    p.add_argument('--jobs',type=int,default=4)
    p.add_argument('--min-sources', '--min-source-count', dest='min_source_count', type=int,default=30)
    p.add_argument('--keep-objects', '--keep_objects', dest='keep_objects', action='store_true')
    x=p.parse_args()
    try: sys.exit(build(x))
    except (OSError,ValueError,RuntimeError,zipfile.BadZipFile) as e:
        print('ERROR:',e,file=sys.stderr);sys.exit(1)
