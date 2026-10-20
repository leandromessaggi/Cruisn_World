#!/usr/bin/env python3
"""Cruis'n World (USA) ROM forensics. Never exports proprietary ROM bytes by default.

This is a ROM-aware analysis pipeline for the N64 static recompilation effort,
not a playable emulator or a complete recovered source tree.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zipfile

EXPECTED_SHA256 = '5320594232f0c492a672464832425351bd06497c95fb243745e2493bc5df9c2c'
EXPECTED_SHA1 = '6da1a6a2bda687d50e798d80c342948ad1738202'
EXPECTED_SIZE = 0xC00000
EXPECTED_GAME_CODE = 'NCWE'
# Configured from independently published symbol map for this exact ROM.
SECTIONS = (
    {'name':'entry','rom_start':0x1000,'rom_end':0x1038,'vram_start':0x802ad400,'kind':'MIPS boot entry'},
    {'name':'main','rom_start':0x1050,'rom_end':0x84e20,'vram_start':0x802ad450,'kind':'MIPS code / game, libultra'},
    {'name':'game_overlay','rom_start':0xa7040,'rom_end':0xbfe90,'vram_start':0x80050400,'kind':'MIPS overlay code'},
    {'name':'audio_rsp','rom_start':0x86230,'rom_end':0x87050,'vram_start':0x04001080,'kind':'RSP microcode'},
)
BOOT_WORDS = (
    0x3c088035, # lui t0,0x8035
    0x3c090009, # lui t1,0x9
    0x25083380, # addiu t0,t0,0x3380
    0x35294760, # ori t1,t1,0x4760
    0x2129fff8, # addi t1,t1,-8
    0xad000000, # sw zero,0(t0)
    0xad000004, # sw zero,4(t0)
    0x1520fffc, # bnez t1,loop
    0x21080008, # addi t0,t0,8 (branch delay)
)

def convert_endianness(raw):
    magic = raw[:4].hex()
    if magic == '80371240':
        return raw, 'big endian .z64'
    if magic == '37804012':
        if len(raw) % 2: raise ValueError('Invalid .v64 byte count')
        x=bytearray(raw)
        x[::2],x[1::2]=raw[1::2],raw[::2]
        return bytes(x), 'byte-swapped .v64/.n64'
    if magic == '40123780':
        if len(raw) % 4: raise ValueError('Invalid .n64 word count')
        x=bytearray(len(raw))
        for i in range(0,len(raw),4): x[i:i+4]=raw[i:i+4][::-1]
        return bytes(x), 'little endian .n64'
    raise ValueError('ROM does not have a recognized Nintendo 64 magic')

def load_rom(path):
    p=Path(path)
    if zipfile.is_zipfile(p):
        with zipfile.ZipFile(p) as z:
            members=[f for f in z.infolist() if not f.is_dir() and f.filename.lower().endswith(('.n64','.z64','.v64'))]
            if len(members)!=1: raise ValueError('ZIP must contain exactly one .n64/.z64/.v64 ROM')
            if members[0].file_size>128*1024*1024: raise ValueError('Unusually large ROM')
            b=z.read(members[0]); member=members[0].filename
    else: b=p.read_bytes();member=p.name
    b,ordering=convert_endianness(b)
    return b,ordering,member

def header(rom):
    s=rom[:0x40]
    return {
        'name':s[0x20:0x34].decode('ascii','replace').strip(),
        'game_code':s[0x3b:0x3f].decode('ascii','replace'),
        'country_code_hex':f'0x{s[0x3e]:02X}',
        'version':s[0x3f],
        'header_entrypoint':f'0x{struct.unpack_from(">I",s,8)[0]:08X}',
        'crc1':f'{struct.unpack_from(">I",s,16)[0]:08X}',
        'crc2':f'{struct.unpack_from(">I",s,20)[0]:08X}',
    }

def scan_magic(rom):
    # Signatures are indicators, not necessarily compressed archives.
    names=('MIO0','Yaz0','RNC','F3DEX','RSP')
    out={}
    for name in names:
        sig=name.encode('ascii'); i=0;hits=[]
        while 1:
            i=rom.find(sig,i)
            if i<0: break
            hits.append(f'0x{i:X}'); i+=len(sig)
            if len(hits)>=100:break
        out[name]=hits
    return out

def decode_entry(rom):
    words=[struct.unpack_from('>I',rom,0x1000+i*4)[0] for i in range(9)]
    exact=tuple(words)==BOOT_WORDS
    return {'first_9_instructions':[f'{x:08X}' for x in words],
        'matches_known_boot_loop':exact,
        'clears_bss_from':'0x80353380' if exact else None,
        'clears_bss_bytes':0x94760 if exact else None,
        'boot_jump':'0x802AD450' if exact else None}

def make_elf(rom,start,end,vram,path):
    # ELF32 MIPS BE with one .text section for LLVM objdump. For ANALYSIS ONLY.
    payload=rom[start:end];strs=b'\0.text\0.shstrtab\0';off=0x100
    st=(off+len(payload)+3)&~3;shoff=(st+len(strs)+3)&~3
    elf=bytearray(shoff+40*3)
    elf[:16]=bytes.fromhex('7f454c46010201000000000000000000')
    struct.pack_into('>HHIIIIIHHHHHH',elf,16,2,8,1,vram,0,shoff,0,52,0,0,40,3,2)
    elf[off:off+len(payload)]=payload;elf[st:st+len(strs)]=strs
    struct.pack_into('>IIIIIIIIII',elf,shoff+40,1,1,6,vram,off,len(payload),0,0,4,0)
    struct.pack_into('>IIIIIIIIII',elf,shoff+80,7,3,0,0,st,len(strs),0,0,1,0)
    Path(path).write_bytes(elf)

def probe(path, report_dir, disassembler=None):
    rom,ordering,member=load_rom(path)
    sha256=hashlib.sha256(rom).hexdigest();sha1=hashlib.sha1(rom).hexdigest()
    h=header(rom)
    valid=(len(rom)==EXPECTED_SIZE and sha256==EXPECTED_SHA256 and sha1==EXPECTED_SHA1 and
      h['game_code']==EXPECTED_GAME_CODE and h['crc1']=='DFE61153' and h['crc2']=='D76118E6')
    result={'source_member':member,'input_byte_order':ordering,
      'normalized_size_bytes':len(rom),'sha256':sha256,'sha1':sha1,
      'matches_supported_us_rom':valid,'header':h,
      'segments':[dict(s,rom_size=s['rom_end']-s['rom_start']) for s in SECTIONS],
      'entry_analysis':decode_entry(rom),'magic_signature_offsets':scan_magic(rom)}
    out=Path(report_dir);out.mkdir(parents=True,exist_ok=True)
    (out/'rom_report.json').write_text(json.dumps(result,indent=2)+'\n')
    lines=['# Cruis\'n World — análise reproduzível da ROM', '',
           f'- ROM reconhecida: **{valid}**',f'- Arquivo original: `{member}`',
           f'- Ordem de bytes detectada: {ordering}',f'- SHA-256 após conversão: `{sha256}`',
           f'- CRC: `{h["crc1"]}` / `{h["crc2"]}`',
           f'- Entry header: `{h["header_entrypoint"]}`',
           '- Endereço do executável após o mapeamento descrito no projeto de recompilação: `0x802AD400`',
           '', '## Seções de código identificadas', '']
    for x in SECTIONS:
        lines.append(f'- **{x["name"]}**: ROM `0x{x["rom_start"]:X}-0x{x["rom_end"]:X}`, VRAM `0x{x["vram_start"]:08X}` ({x["kind"]})')
    lines+=['','## Primeiro bloco de boot','',
       '- Loop MIPS inicial zera `0x94760` bytes de BSS começando em `0x80353380`' if decode_entry(rom)['matches_known_boot_loop'] else '- Boot MIPS não reconhecido',
       '- O loop utiliza branch delay slot; jump final prepara stack e salta a `0x802AD450`.',
       '','## Limites','',
       'Esta análise **não executa o jogo** nem deduz todos os assets. A ROM não está incluída nos resultados.',
       'O símbolo candidato e a estrutura dos overlays devem ser refinados antes da recompilação completa.',
       '','## Referências','',
       '- https://github.com/navjack/cruisn-world-recomp',
       '- https://github.com/N64Recomp/N64Recomp','- https://github.com/N64Recomp/N64ModernRuntime', '']
    (out/'ROM_FINDINGS.md').write_text('\n'.join(lines))
    if disassembler:
        elf=out/'entry_for_disasm.elf'
        make_elf(rom,0x1000,0x1050,0x802ad400,elf)
        try:
            p=subprocess.run([disassembler,'-d','--triple=mips-unknown-elf',str(elf)],capture_output=True,text=True,check=True)
            (out/'entry_disassembly.txt').write_text(p.stdout)
        except (OSError,subprocess.CalledProcessError) as ex:
            (out/'disasm_error.txt').write_text(str(ex))
        # Do not put ROM bytes in public bundles; even an ELF containing the 0x50 bytes
        # from the original cartridge is local-only.
        elf.unlink(missing_ok=True)
    return result

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('rom',help='your .zip .n64 .v64 or .z64')
    ap.add_argument('--output',default='reports')
    ap.add_argument('--llvm-objdump',default=None)
    args=ap.parse_args()
    x=probe(args.rom,args.output,args.llvm_objdump)
    print('SHA-256:',x['sha256'])
    print('Exact Cruis\'n World US match:',x['matches_supported_us_rom'])
    print('Report:',Path(args.output).resolve())
    if not x['matches_supported_us_rom']:raise SystemExit(2)
