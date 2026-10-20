import importlib.util
from pathlib import Path
import unittest

SRC=Path(__file__).resolve().parents[1]/'tools'/'rom_probe.py'
spec=importlib.util.spec_from_file_location('rom_probe',SRC)
rp=importlib.util.module_from_spec(spec);spec.loader.exec_module(rp)

class TestHeaderAndByteOrders(unittest.TestCase):
    def test_three_n64_orders(self):
        magic=bytes.fromhex('80371240')+bytes(60)
        self.assertEqual(rp.convert_endianness(magic)[0],magic)
        swapped=bytearray(magic)
        swapped[::2],swapped[1::2]=magic[1::2],magic[::2]
        self.assertEqual(rp.convert_endianness(swapped)[0],magic)
        little=bytearray(magic)
        for i in range(0,len(magic),4):little[i:i+4]=magic[i:i+4][::-1]
        self.assertEqual(rp.convert_endianness(little)[0],magic)
    def test_invalid(self):
        with self.assertRaises(ValueError):rp.convert_endianness(b'garbage')
    def test_sections_in_range(self):
        for section in rp.SECTIONS:
            self.assertLess(section['rom_start'],section['rom_end'])
            self.assertLess(section['rom_end'],rp.EXPECTED_SIZE)
    def test_entry_known_words(self):
        rom=bytearray(0x1100)
        import struct
        for i,word in enumerate(rp.BOOT_WORDS):struct.pack_into('>I',rom,0x1000+i*4,word)
        self.assertTrue(rp.decode_entry(rom)['matches_known_boot_loop'])
        rom[0x1014]^=1
        self.assertFalse(rp.decode_entry(rom)['matches_known_boot_loop'])

if __name__=='__main__':unittest.main()
