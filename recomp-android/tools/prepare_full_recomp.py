#!/usr/bin/env python3
"""Recompile a user's own Cruis'n World USA ROM offline into CPU/RSP C sources.

Inputs: exact USA N64 ROM and the ROM-free GitHub Actions toolchain ZIP.
Outputs: private generated C/C++ code. This is not yet an Android game.
No ROM bytes are ever uploaded to GitHub.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile

SHA256 = "5320594232f0c492a672464832425351bd06497c95fb243745e2493bc5df9c2c"
ROM_SIZE = 0xC00000
ROM_NAME = "Cruis'n World (U) [!].z64"
REQUIRED = {
    "bin/N64Recomp", "bin/RSPRecomp",
    "games/cruisn-world/config/recomp-symbols.toml",
    "games/cruisn-world/input/cruisn-world.syms.toml",
    "games/cruisn-world/rsp/aspMain.us.toml"
}

class RecompError(Exception):
    pass

def normalize(data):
    if len(data) != ROM_SIZE:
        raise RecompError("Expected a 12 MiB ROM")
    if data[:4] == bytes.fromhex("80371240"):
        canonical = data
    elif data[:4] == bytes.fromhex("37804012"):
        output = bytearray(len(data))
        output[::2], output[1::2] = data[1::2], data[::2]
        canonical = bytes(output)
    elif data[:4] == bytes.fromhex("40123780"):
        canonical = b"".join(data[i:i+4][::-1] for i in range(0,len(data),4))
    else:
        raise RecompError("Unsupported ROM byte order or invalid header")
    if hashlib.sha256(canonical).hexdigest() != SHA256:
        raise RecompError("Unsupported ROM revision; original USA SHA256 mismatch")
    return canonical

def read_user_rom(filename):
    if not zipfile.is_zipfile(filename):
        return normalize(filename.read_bytes())
    with zipfile.ZipFile(filename) as z:
        matches = [i for i in z.infolist() if not i.is_dir()
                   and i.filename.lower().endswith((".z64",".n64",".v64"))]
        if len(matches) != 1 or matches[0].file_size != ROM_SIZE:
            raise RecompError("ZIP must contain exactly one 12 MiB N64 ROM")
        return normalize(z.read(matches[0]))

def extract_toolchain(filename, root):
    with zipfile.ZipFile(filename) as z:
        allnames = set(z.namelist())
        prefix = "toolchain/" if "toolchain/bin/N64Recomp" in allnames else ""
        names = {s[len(prefix):] for s in allnames if s.startswith(prefix)}
        if not REQUIRED.issubset(names):
            raise RecompError("Toolchain ZIP missing required executables/metadata")
        for item in z.infolist():
            if item.is_dir() or (prefix and not item.filename.startswith(prefix)):
                continue
            rel = item.filename[len(prefix):]
            parts = Path(rel).parts
            if (not rel.startswith(("bin/","include/","games/cruisn-world/"))
                    and rel != "PINNED_COMMITS.txt"):
                raise RecompError("Unexpected toolchain member "+rel)
            if ".." in parts or Path(rel).is_absolute() or "\\" in rel or item.file_size > 64000000:
                raise RecompError("Unsafe toolchain entry")
            target = root / rel
            target.parent.mkdir(parents=True,exist_ok=True)
            with z.open(item) as src, target.open("wb") as dst:
                shutil.copyfileobj(src,dst)
    for name in ("N64Recomp","RSPRecomp"):
        exe = root / "bin" / name
        exe.chmod(exe.stat().st_mode | stat.S_IXUSR)

def build(rom_path,toolchain_path,output):
    if output.exists() and any(output.iterdir()):
        raise RecompError("Destination must be empty")
    data = read_user_rom(rom_path)
    with tempfile.TemporaryDirectory(prefix="cruisn-world-recomp-") as temporary:
        workspace=Path(temporary)
        extract_toolchain(toolchain_path,workspace)
        rom = workspace / "roms" / ROM_NAME
        rom.parent.mkdir(parents=True,exist_ok=True)
        rom.write_bytes(data)
        cfg = workspace / "games/cruisn-world/config/recomp-symbols.toml"
        cpu = subprocess.run([str(workspace/"bin/N64Recomp"),str(cfg)],cwd=workspace,
                             text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=300)
        if cpu.returncode:
            raise RecompError("CPU recompilation failed: "+cpu.stdout[-4000:])
        generated=workspace/"games/cruisn-world/generated"
        files=list(generated.glob("*.c"))
        if not files:
            raise RecompError("CPU recompiler produced no C sources")
        cfg_rsp=workspace/"games/cruisn-world/rsp/aspMain.us.toml"
        rsp=subprocess.run([str(workspace/"bin/RSPRecomp"),str(cfg_rsp)],cwd=workspace,
                           text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
        if rsp.returncode:
            raise RecompError("RSP recompilation failed: "+rsp.stdout[-4000:])
        rspfile=workspace/"games/cruisn-world/rsp/aspMain.cpp"
        if not rspfile.is_file():
            raise RecompError("RSP audio C++ output missing")
        (output/"cpu").mkdir(parents=True)
        (output/"rsp").mkdir()
        for file in generated.iterdir():
            if file.is_file() and file.suffix in (".c",".h"):
                shutil.copy2(file,output/"cpu"/file.name)
        shutil.copy2(rspfile,output/"rsp/aspMain.cpp")
        if (workspace/"include").exists():
            shutil.copytree(workspace/"include",output/"include",dirs_exist_ok=True)
        report={"rom_sha256":SHA256,"cpu_c_files":len(files),"rsp_generated":True,
                "android_playable":False,"runtime_linked":False,
                "message":"Private generated sources; N64ModernRuntime/RT64 integration still missing."}
        (output/"report.json").write_text(json.dumps(report,indent=2)+"\n")
        print(json.dumps(report,indent=2))
    # tempfile wipes the private ROM copy, never an upload or public commit.

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--rom",type=Path,required=True)
    p.add_argument("--toolchain",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    try:
        build(a.rom,a.toolchain,a.output)
    except (OSError,RecompError,zipfile.BadZipFile,subprocess.TimeoutExpired) as error:
        print("ERROR:",error,file=sys.stderr)
        return 1
    return 0
if __name__ == "__main__":
    sys.exit(main())
