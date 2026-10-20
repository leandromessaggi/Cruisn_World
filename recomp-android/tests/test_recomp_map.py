import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from recomp_map import inspect

class RecompMappingTests(unittest.TestCase):
    def test_native_catalog(self):
        import json
        catalog=json.loads((Path(__file__).parents[1]/'tools'/'recompiled_functions.json').read_text())
        self.assertEqual(17,len(catalog['functions']))
        self.assertEqual(len(set(x['vram'] for x in catalog['functions'])),17)

    def test_rejects_nonmatching_rom(self):
        with self.assertRaises(ValueError): inspect(b'not a ROM')

    def test_known_address_to_file_offset(self):
        from recomp_map import FUNCS, MAIN_ROM, VIRTUAL_MAIN
        self.assertEqual(0x2A9C,MAIN_ROM+FUNCS[0][0]-VIRTUAL_MAIN)
        self.assertEqual(0x31E4,MAIN_ROM+FUNCS[2][0]-VIRTUAL_MAIN)
        self.assertEqual(0x1A260,0xC12A0-0xA7040)

if __name__=='__main__': unittest.main()
