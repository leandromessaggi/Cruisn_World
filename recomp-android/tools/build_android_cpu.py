#!/usr/bin/env python3
"""Build a static Android ARM64 archive from PRIVATE N64Recomp-generated C.
No runtime linkage, graphics, ROM redistribution, or playable APK."""
import argparse, hashlib, json, os, re, shutil, subprocess, sys, tempfile, zipfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

def cmd(*args):
    return subprocess.run([str(a) for a in args], capture_output=True, text=True)

def safe_extract(path, dest):
    with zipfile.ZipFile(path) as z:
        for entry in z.infolist():
            name=Path(entry.filename)
            if name.is_absolute() or '..' in name.parts:
                raise ValueError('Unsafe ZIP entry: '+entry.filename)
            if entry.is_dir(): continue
            target=dest/name
            target.parent.mkdir(parents=True,exist_ok=True)
            with z.open(entry) as src, target.open('wb') as out:
                shutil.copyfileobj(src,out)

def tools(ndk,api):
    base=ndk/'toolchains'/'llvm'/'prebuilt'
    for host in sorted(base.glob('*')):
        bin=host/'bin'
        paths=(bin/f'aarch64-linux-android{api}-clang',bin/'llvm-ar',
               bin/'llvm-nm',bin/'llvm-readelf')
        if all(p.is_file() for p in paths): return paths
    raise RuntimeError(f'Android NDK LLVM tools not found in {base}')

def build(a):
    if not a.ndk: raise RuntimeError('Provide --ndk or ANDROID_NDK_HOME')
    ndk=Path(a.ndk)
    if not ndk.is_dir(): raise RuntimeError('NDK directory missing: '+str(ndk))
    cc,ar,nm,readelf=tools(ndk,a.api)
    out=Path(a.output).resolve()
    out.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='cw-recomp-') as scratch:
        if a.input.endswith('.zip'):
            safe_extract(a.input,Path(scratch))
            root=Path(scratch)
        else:
            root=Path(a.input).resolve()
        cpu=root/'cpu';inc=root/'include'
        files=[p for p in cpu.glob('funcs_*.c') if re.fullmatch(r'funcs_\d+\.c',p.name)]
        files.sort(key=lambda p:int(p.stem.split('_')[1]))
        if len(files)<a.min_sources:
            raise RuntimeError(f'Expected {a.min_sources} generated C sources, found {len(files)}')
        for p in (cpu/'funcs.h',inc/'recomp.h'):
            if not p.is_file(): raise RuntimeError('Missing '+str(p))
        objects=out/'objects';objects.mkdir(exist_ok=True)
        def compile_one(path):
            obj=objects/(path.stem+'.o')
            r=cmd(cc,'-std=gnu11','-O1','-fPIC','-fno-lto',
                  '-fno-strict-aliasing','-Wno-unused-variable',
                  '-Wno-unused-label','-I',inc,'-I',cpu,'-c',path,'-o',obj)
            if r.returncode: raise RuntimeError(f'{path.name}: {r.stderr[-3500:]}')
            info=cmd(readelf,'-h',obj)
            if info.returncode or 'AArch64' not in info.stdout:
                raise RuntimeError('Invalid AArch64 object: '+str(obj))
            return obj
        with ThreadPoolExecutor(max_workers=max(1,a.jobs)) as executor:
            futures=[executor.submit(compile_one,p) for p in files]
            objects_list=[]
            for future in as_completed(futures): objects_list.append(future.result())
        archive=out/'libcw_recompiled_cpu_arm64.a'
        r=cmd(ar,'rcs',archive,*sorted(objects_list))
        if r.returncode: raise RuntimeError(r.stderr)
        sym=cmd(nm,'-g',archive)
        if sym.returncode: raise RuntimeError(sym.stderr)
        defined=set(re.findall(r'^\s*[0-9a-fA-F]+\s+[TW]\s+(\w+)$',sym.stdout,re.M))
        needed=set(re.findall(r'^\s*U\s+(\w+)$',sym.stdout,re.M))
        unresolved=sorted(needed-defined)
        report={'target':f'aarch64-linux-android{a.api}',
                'source_count':len(files),'object_count':len(objects_list),
                'function_bodies':sum(len(re.findall(r'RECOMP_FUNC\s+void\s+\w+\s*\(',p.read_text(errors='replace'))) for p in files),
                'archive_bytes':archive.stat().st_size,
                'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
                'unresolved_symbol_count':len(unresolved),
                'unresolved_symbols':unresolved,
                'runtime_linked':False,'gameplay_executable':False}
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        (out/'unresolved_symbols.txt').write_text('\n'.join(unresolved)+'\n')
        print(json.dumps({k:v for k,v in report.items() if k!='unresolved_symbols'},indent=2))
        if not a.keep_objects: shutil.rmtree(objects)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',required=True)
    parser.add_argument('--ndk',default=os.environ.get('ANDROID_NDK_HOME') or os.environ.get('ANDROID_NDK_ROOT') or '')
    parser.add_argument('--output',required=True)
    parser.add_argument('--api',type=int,default=28)
    parser.add_argument('--jobs',type=int,default=4)
    parser.add_argument('--min-sources',type=int,default=30)
    parser.add_argument('--keep-objects',action='store_true')
    args=parser.parse_args()
    try: build(args)
    except (ValueError,RuntimeError,OSError,zipfile.BadZipFile) as err:
        print('ERROR:',err,file=sys.stderr);sys.exit(1)
