#!/usr/bin/env python3
"""The compiled module manifest has one deterministic owner per kernel."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("kernel_exports", Path(__file__).resolve().parents[1] / "tools/generate_cuda_exports.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class Exports(unittest.TestCase):
    def test_ownership_and_order(self):
        images = [".visible .entry beta() {}\n.visible .entry alpha() {}",
                  ".visible .entry gamma() {}"]
        result = module.exports(images)
        self.assertEqual(result, module.exports(images))
        self.assertLess(result.index('"alpha"'), result.index('"beta"'))
        self.assertIn('{"alpha", 0u}', result)
        self.assertIn('{"beta", 0u}', result)
        self.assertIn('{"gamma", 1u}', result)
        self.assertIn("MODULE_COUNT 2u", result)

    def test_comments_are_not_symbols(self):
        self.assertEqual(module.exports([".entry real(){}"]), module.exports([
            "// .entry fake()\n/* .entry another() */ .entry real(){}"] ))

    def test_fail_closed(self):
        fixtures = [[], [""], [".entry"], [".entry bad-name(){}"],
                    [".entry one(){} .entry one(){}"],
                    [".entry one(){}", ".entry one(){}"],
                    [".entry one(){} .entry malformed"], [".entry one(){}", "// empty"]]
        for images in fixtures:
            with self.subTest(images=images), self.assertRaises(ValueError):
                module.exports(images)


if __name__ == "__main__":
    unittest.main()
