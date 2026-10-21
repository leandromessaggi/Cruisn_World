#!/usr/bin/env python3
"""Audit relocatable AArch64 game objects without executing or linking ROM-derived code.

The source archive and resulting objects MUST remain private.  Only this generic
analysis program and ROM-free tests belong in the public repository.
"""
import argparse
import json
import re
import subprocess
from collections import defaultdict
from pathlib import Path

SYMBOL = re.compile(r'^\s*\d+:\s+[0-9a-fA-F]+\s+\d+\s+(\w+)\s+(\w+)\s+(\w+)\s+(\w+)\s+(\S+)\s*$')
DECLARATION = re.compile(r'^void\s+(\w+)\s*\(uint8_t\s*\*\s*rdram\s*,\s*recomp_context\s*\*\s*ctx\s*\)\s*;', re.M)


def run(*cmd):
    p = subprocess.run([str(x) for x in cmd], text=True, capture_output=True)
    if p.returncode:
        raise RuntimeError(f'Command failed: {cmd[0]}: {p.stderr.strip()}')
    return p.stdout


def inspect(objects, declarations, readelf='readelf', expected_functions=0):
    object_files = sorted(Path(objects).glob('funcs_*.o'))
    if not object_files:
        raise ValueError('No funcs_*.o found')
    exports = defaultdict(list)
    imports = set()
    for obj in object_files:
        header = run(readelf, '-h', obj)
        if 'Machine:' not in header or 'AArch64' not in header or 'ELF64' not in header:
            raise ValueError(f'Object is not AArch64 ELF64: {obj}')
        syms = run(readelf, '-W', '-s', obj)
        for line in syms.splitlines():
            m = SYMBOL.match(line)
            if not m:
                continue
            kind, binding, _, section, name = m.groups()
            if section == 'UND' and name and not name.startswith('$'):
                imports.add(name)
            elif kind == 'FUNC' and binding in ('WEAK', 'GLOBAL'):
                exports[name].append(obj.name)
    runtime = set(DECLARATION.findall(Path(declarations).read_text(encoding='utf-8')))
    missing = imports - exports.keys()
    # Object imports can be satisfied by exports in a different recompilation unit.
    unresolved = sorted(missing)
    runtime_missing = sorted(missing & runtime)
    platform_missing = sorted(missing - runtime)
    body_names = sorted(k for k in exports if k.startswith('func_') or k.startswith('recomp_') or k.startswith('asp'))
    report = {
        'format': 'CW_ARM64_UNLINKED_AUDIT_V1',
        'object_count': len(object_files),
        'aarch64_elf_verified': True,
        'defined_functions': len(exports),
        'guest_function_exports': len(body_names),
        'declared_runtime_hooks': len(runtime),
        'referenced_runtime_hooks_unresolved': len(runtime_missing),
        'unresolved_symbol_count': len(unresolved),
        'unresolved_runtime_hooks': runtime_missing,
        'unresolved_support_symbols': platform_missing,
        'unresolved_symbols': unresolved,
        'runtime_linked': False,
        'android_ndk_abi_validated': False,
        'gameplay_executable': False,
    }
    if expected_functions and len(exports) != expected_functions:
        raise ValueError(f'Expected {expected_functions} function exports; got {len(exports)}')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', required=True)
    parser.add_argument('--declarations', required=True)
    parser.add_argument('--readelf', default='readelf')
    parser.add_argument('--expected-functions', type=int, default=0)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    report = inspect(args.objects, args.declarations, args.readelf, args.expected_functions)
    Path(args.output).write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(f"Verified {report['object_count']} AArch64 objects, {report['defined_functions']} functions; "
          f"{report['unresolved_symbol_count']} unresolved symbols.")


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError) as e:
        raise SystemExit('ERROR: '+str(e))
