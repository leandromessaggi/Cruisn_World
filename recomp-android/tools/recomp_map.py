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
    for pc,dest,value,words in FUNCS:
        off=MAIN_ROM+(pc-VIRTUAL_MAIN)
        actual=tuple(int.from_bytes(rom[off+4*j:off+4*j+4],'big') for j in range(len(words)))
        if actual!=words:
            raise ValueError(f'MIPS instructions diverged at 0x{pc:X}')
        report['functions'].append({'entry_vram':f'0x{pc:08X}','rom_offset':f'0x{off:05X}',
            'mapped_global':f'0x{dest:08X}','written_value':value,
            'instruction_count':len(words),'translated_semantics':'store word constant in jr $ra delay slot'})
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
    print('Four MIPS functions verified; overlay FNV64:',result['code_overlay']['fnv1a64'])
