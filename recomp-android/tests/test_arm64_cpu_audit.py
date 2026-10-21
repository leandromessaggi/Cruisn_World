"""ROM-free cross-target regression checks for the generated CPU ABI audit."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1]/'tools'/'audit_arm64_cpu.py'
spec = importlib.util.spec_from_file_location('cw_arm64_audit', SCRIPT)
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class AuditTests(unittest.TestCase):
    def test_arm64_symbols(self):
        ndk = os.environ.get('ANDROID_NDK_HOME', '')
        if ndk:
            paths = sorted(Path(ndk).glob('toolchains/llvm/prebuilt/*/bin'))
            cc = paths[0]/'aarch64-linux-android28-clang'
            readelf = str(paths[0]/'llvm-readelf')
        else:
            cc = 'clang'
            readelf = 'readelf'
        with tempfile.TemporaryDirectory(prefix='cw-arm64-audit-test-') as directory:
            root = Path(directory)
            decl = root/'recomp_decls.h'
            decl.write_text('#include <stdint.h>\ntypedef struct recomp_context recomp_context;\n'
                            'void hook(uint8_t* rdram, recomp_context* ctx);\n')
            for n, body in enumerate(('void hook(void);void support(void);void func_1(void){hook();support();}',
                                      'void func_1(void);void func_2(void){func_1();}')):
                src = root/f'funcs_{n}.c'
                obj = root/f'funcs_{n}.o'
                src.write_text(body)
                cmd = [str(cc), '-std=c11', '-c', str(src), '-o', str(obj)]
                if not ndk:
                    cmd.insert(1, '--target=aarch64-linux-android26')
                    cmd.insert(2, '-ffreestanding')
                subprocess.run(cmd, check=True, capture_output=True)
            result = audit.inspect(root, decl, readelf, expected_functions=2)
            self.assertEqual(result['unresolved_symbol_count'], 2)
            self.assertEqual(result['unresolved_runtime_hooks'], ['hook'])
            self.assertEqual(result['unresolved_support_symbols'], ['support'])
            self.assertEqual(result['object_count'], 2)
            self.assertFalse(result['gameplay_executable'])
            with self.assertRaisesRegex(ValueError, 'Expected 3'):
                audit.inspect(root, decl, readelf, expected_functions=3)

    def test_rejects_wrong_architecture(self):
        with tempfile.TemporaryDirectory(prefix='cw-host-audit-test-') as tmp:
            root = Path(tmp)
            src = root/'funcs_0.c'
            src.write_text('void func_1(void){}')
            subprocess.run(['cc', '-c', src, '-o', root/'funcs_0.o'], check=True, capture_output=True)
            decl=root/'recomp_decls.h';decl.write_text('')
            with self.assertRaisesRegex(ValueError,'AArch64 ELF64'):
                audit.inspect(root, decl)


if __name__ == '__main__':
    unittest.main()
