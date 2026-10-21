#!/usr/bin/env python3
"""ROM-free integration tests for the entrypoint verifier infrastructure."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT/'tools'/'verify_native_entrypoint.py'

RECOMP = '''#pragma once
#include <stdint.h>
typedef struct { uint64_t r9, r10, r29; } recomp_context;
typedef void (recomp_func_t)(uint8_t*, recomp_context*);
recomp_func_t* get_function(int32_t);
'''
FUNCS = '''#include <stdint.h>
#include <string.h>
#include "recomp.h"
#include "funcs.h"
void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx) {
    memset(rdram + 0x353380, 0, 0x94760);
    ctx->r9=0; ctx->r10=0x802ad450; ctx->r29=0x80150400;
    get_function((int32_t)ctx->r10)(rdram,ctx);
}
'''

with tempfile.TemporaryDirectory() as d:
    p=Path(d); (p/'cpu').mkdir(); (p/'include').mkdir()
    (p/'include'/'recomp.h').write_text(RECOMP)
    (p/'cpu'/'funcs.h').write_text('void recomp_entrypoint(uint8_t*, recomp_context*);\n')
    (p/'cpu'/'recomp_decls.h').write_text('')
    (p/'cpu'/'funcs_0.c').write_text(FUNCS)
    out=p/'report.json'
    args=[sys.executable,str(SCRIPT),'--input',str(p),'--output',str(out)]
    run=subprocess.run(args,capture_output=True,text=True)
    assert run.returncode != 0 and 'fingerprint' in run.stderr, run.stderr
    run=subprocess.run(args+['--allow-synthetic'],capture_output=True,text=True)
    assert run.returncode == 0, run.stderr
    report=json.loads(out.read_text())
    assert report['status'] == 'PASS'
    assert report['original_code_executed'] is False
    assert report['private_source_verified'] is False
    assert report['main_executed'] is False
    (p/'cpu'/'funcs_0.c').write_text(FUNCS.replace('0x94760','0x9475c'))
    run=subprocess.run(args+['--allow-synthetic'],capture_output=True,text=True)
    assert run.returncode != 0 and 'BSS not cleared' in run.stderr,run.stdout+run.stderr
    print('PASS: synthetic harness, guard outside BSS, original-source fingerprint gate')
