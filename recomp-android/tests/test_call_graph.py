import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from call_graph import collect_calls, inspect

class CallGraphTests(unittest.TestCase):
    def test_mips_jal_decoding_and_sections(self):
        rom = bytearray(0xC00000)
        # Real fixed entrypoint: wrapper at 0x802AF6EC calls 0x802AF5F4.
        rom[0x32f4:0x32f8] = bytes.fromhex('0c0abd7d')
        matches=collect_calls(rom,{0x802AF5F4})
        self.assertEqual([{'rom_offset':'0x0032F4',
                           'caller_instruction':'0x802AF6F4',
                           'target':'0x802AF5F4'}], matches)
        # Same opcode in the data segment is NOT counted.
        rom[0x90000:0x90004] = bytes.fromhex('0c0abd7d')
        self.assertEqual(matches,collect_calls(rom,{0x802AF5F4}))
        self.assertEqual([],collect_calls(rom,{0x802AF5E4}))
    def test_unknown_or_modified_rom_is_rejected(self):
        with self.assertRaises(ValueError):
            inspect(bytes(0xC00000), {'functions': [{'vram':'0x802AF5F4'}]})

if __name__ == '__main__': unittest.main()
