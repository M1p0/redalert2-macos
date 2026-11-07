"""Validate the CSF wire format and reversible localization installation."""
import importlib.util
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("zh_hans", Path(__file__).parents[1] / "install_zh_hans.py")
zh = importlib.util.module_from_spec(spec)
spec.loader.exec_module(zh)


class LocalizationTests(unittest.TestCase):
    def test_csf_utf16_and_extra(self):
        text = "\u7ea2\u8272\u8b66\u6212\U0001f600"
        data = zh.build_csf({"TEST": {"Value": text, "Extra": "clip"}})
        self.assertEqual(struct.unpack_from("<4s5I", data), (b" FSC", 3, 1, 1, 0, 9))
        self.assertEqual(struct.unpack_from("<4sII", data, 24), (b" LBL", 1, 4))
        self.assertEqual(data[36:40], b"TEST")
        tag, units = struct.unpack_from("<4sI", data, 40)
        self.assertEqual(tag, b"WRTS")
        end = 48 + 2 * units
        self.assertEqual(bytes(b ^ 0xFF for b in data[48:end]).decode("utf-16le"), text)
        self.assertEqual(data[end:], struct.pack("<I", 4) + b"clip")

    def test_integrity_rejects_changed_input(self):
        with self.assertRaises(ValueError):
            zh.checked(b"changed", "0" * 64, "sample")

    def test_install_backs_up_case_variant_and_marks_new_files(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as root:
            root = Path(root)
            game, backup = root / "game", root / "backup"
            game.mkdir()
            (game / "game.exe").write_bytes(b"test-only placeholder")
            (game / "RA2.CSF").write_bytes(b"original")
            zh.install(game, backup, {"ra2.csf": b"translated", "game.fnt": b"font"})
            self.assertEqual((game / "RA2.CSF").read_bytes(), b"translated")
            self.assertFalse((game / "ra2.csf").exists() and not (game / "RA2.CSF").samefile(game / "ra2.csf"))
            self.assertEqual((backup / "RA2.CSF").read_bytes(), b"original")
            manifest = json.loads((backup / "restore.json").read_text())
            self.assertTrue(manifest["files"]["RA2.CSF"])
            self.assertFalse(manifest["files"]["game.fnt"])
            self.assertEqual(json.loads((game / "Localization/zh-Hans-source.json").read_text())["language"], "zh-Hans")
            with self.assertRaises(FileExistsError):
                zh.install(game, backup, {"ra2.csf": b"again"})


if __name__ == "__main__":
    unittest.main()
