#!/usr/bin/env python3
"""Report recompiled Cruis'n World address ranges, without exporting ROM content.

The symbols here were independently cross-checked using the published
Cruis'n World US recomp map and actual ROM. A nonmatching input must fail.
"""
import argparse
import json
from pathlib import Path
from rom_probe import load_rom, EXPECTED_SHA256, EXPECTED_SIZE
import hashlib

VIRTUAL_MAIN = 0x802AD400
MAIN_ROM = 0x1000
FUNCS = (
 (0x802AEE9C, 0x803334C0, 1, (0x240E0001,0x3C018033,0x03E00008,0xAC2E34C0)),
 (0x802AEEAC, 0x803334C0, 0, (0x3C018033,0x03E00008,0xAC2034C0)),
 (0x802AF5E4, 0x803334C8, 1, (0x240E0001,0x3C018033,0x03E00008,0xAC2E34C8)),
 (0x802AF5F4, 0x803334C8, 0, (0x3C018033,0x03E00008,0xAC2034C8)),
)

def inspect(rom):
    if len(rom)!=EXPECTED_SIZE or hashlib.sha256(rom).hexdigest()!=EXPECTED_SHA256:
        raise ValueError('requires exact supported Cruis\'n World USA SHA-256')
    report={'verified_rom_sha256': EXPECTED_SHA256,
            'first_main_vram':f'0x{VIRTUAL_MAIN:08X}',
            'mapped_main': {'rom_start':'0x1000','rom_end':'0xA6F80','ram_address':'0x802AD400'},
            'zero_bss':{'address':'0x80353380','byte_size':0x94760},
            'code_overlay':{'rom_start':'0xA7040','rom_end':'0xC12A0','ram_address':'0x80050400','byte_size':0xC12A0-0xA7040},
            'functions':[]}
    catalog=json.loads((Path(__file__).with_name('recompiled_functions.json')).read_text())
    for entry in catalog['functions']:
        pc=int(entry['vram'],16)
        off=MAIN_ROM + (pc-VIRTUAL_MAIN)
        code=rom[off:off+4*entry['instructions']]
        if len(code)!=4*entry['instructions'] or hashlib.sha256(code).hexdigest()!=entry['sha256']:
            raise ValueError(f'Function signature mismatch at 0x{pc:08X}')
        report['functions'].append({
            'entry_vram':entry['vram'], 'rom_offset':f'0x{off:06X}',
            'label':entry['name'], 'instruction_count':entry['instructions'],
            'instruction_sha256':entry['sha256'],
            'status':'direct C++ static translation; NOT N64 gameplay',
        })
    h=14695981039346656037
    for b in rom[0xA7040:0xC12A0]:
        h=((h^b)*1099511628211) & ((1<<64)-1)
    report['code_overlay']['fnv1a64']=f'{h:016X}'
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('rom',help='user supplied ROM in any recognized N64 byte order or zip')
    parser.add_argument('--output',default='recomp_v03_report.json')
    args=parser.parse_args()
    rom,_,_=load_rom(args.rom)
    result=inspect(rom)
    Path(args.output).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(len(result['functions']),'MIPS function signatures verified; overlay FNV64:',result['code_overlay']['fnv1a64'])
