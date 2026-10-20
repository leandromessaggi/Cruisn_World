import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

source=Path(__file__).resolve().parents[1]/"tools/prepare_full_recomp.py"
spec=importlib.util.spec_from_file_location("prepare_full_recomp",source)
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

class OfflineROMChecks(unittest.TestCase):
    def test_rejects_incomplete_rom(self):
        with self.assertRaises(mod.RecompError):
            mod.normalize(b"\x80\x37\x12\x40")
    def test_rejects_unknown_revision(self):
        with self.assertRaises(mod.RecompError):
            mod.normalize(bytes.fromhex("80371240")+b"\0"*(mod.ROM_SIZE-4))
    def test_normalizes_n64_byteorders(self):
        original_hash=mod.SHA256
        canonical=bytes.fromhex("80371240")+b"\0"*(mod.ROM_SIZE-4)
        mod.SHA256=hashlib.sha256(canonical).hexdigest()
        try:
            self.assertEqual(mod.normalize(canonical),canonical)
            data=bytearray(len(canonical))
            data[::2],data[1::2]=canonical[1::2],canonical[::2]
            self.assertEqual(mod.normalize(bytes(data)),canonical)
            reversed_words=b"".join(canonical[i:i+4][::-1] for i in range(0,len(canonical),4))
            self.assertEqual(mod.normalize(reversed_words),canonical)
        finally:
            mod.SHA256=original_hash
    def test_rejects_incomplete_toolchain(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/"bad.zip"
            with zipfile.ZipFile(path,"w") as z:
                z.writestr("malicious","payload")
            with self.assertRaises(mod.RecompError):
                mod.extract_toolchain(path,Path(tmp)/"out")

if __name__=="__main__":
    unittest.main()
