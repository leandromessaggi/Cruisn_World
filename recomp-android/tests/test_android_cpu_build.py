"""Test actual NDK cross compilation without any original ROM or game code."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from zipfile import ZipFile

SCRIPT = Path(__file__).resolve().parents[1] / 'tools' / 'build_android_cpu.py'

with tempfile.TemporaryDirectory(prefix='cw-ndk-smoke-') as temporary:
    root = Path(temporary)
    (root/'cpu').mkdir()
    (root/'include').mkdir()
    (root/'include'/'recomp.h').write_text('#define RECOMP_FUNC extern inline __attribute__((weak,noinline))\n')
    (root/'cpu'/'funcs.h').write_text('void cw_smoke(void);\n')
    (root/'cpu'/'recomp_decls.h').write_text('#pragma once\n')
    (root/'cpu'/'funcs_0.c').write_text('#include "recomp.h"\n#include "funcs.h"\nRECOMP_FUNC void cw_smoke(void) {}\n')
    (root/'cpu'/'funcs_1.c').write_text('#include "recomp.h"\n#include "funcs.h"\nvoid cw_other(void) { cw_smoke(); }\n')
    archive = root/'synthetic.zip'
    with ZipFile(archive, 'w') as z:
        for p in (root/'cpu').iterdir():
            z.write(p, p.relative_to(root))
        z.write(root/'include'/'recomp.h', 'include/recomp.h')
    out = root/'out'
    r = subprocess.run([sys.executable, str(SCRIPT), '--input', str(archive),
                        '--ndk', os.environ['ANDROID_NDK_HOME'], '--output', str(out),
                        '--min-sources', '2'], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    result = json.loads((out/'report.json').read_text())
    assert (result['target'],result['source_count'],result['object_count']) == ('aarch64-linux-android28',2,2), result
    assert result['unresolved_symbol_count'] == 0, result['unresolved_symbols']
    assert result['declared_hooks_header'] and not result['runtime_linked'] and not result['gameplay_executable']
    assert (out/'libcw_recompiled_cpu_arm64.a').is_file()
    bad = root/'bad.zip'
    with ZipFile(bad, 'w') as z:
        z.writestr('../escape.txt', 'unsafe')
    r = subprocess.run([sys.executable, str(SCRIPT), '--input', str(bad),
                        '--ndk', os.environ['ANDROID_NDK_HOME'],
                        '--output', str(root/'badout'), '--min-sources', '1'],
                        capture_output=True, text=True)
    assert r.returncode != 0 and 'Unsafe ZIP entry' in r.stderr, r.stderr
    assert not (root/'escape.txt').exists()
    print('PASS: NDK AArch64 object compilation, declarations, ELF validation and ZIP traversal checks')
