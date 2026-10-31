import unittest
from types import CapsuleType

import tree_sitter_diagnostic_sh

from tree_sitter import Language, Parser


class BindingsTest(unittest.TestCase):
    def test_language_capsule_parses_source_and_exposes_faults(self):
        capsule = tree_sitter_diagnostic_sh.language()
        self.assertIsInstance(capsule, CapsuleType)
        language = Language(capsule)
        self.assertEqual(language.name, "diagnostic_sh")
        source = b"cat >\n"
        tree = Parser(language).parse(source)
        self.assertEqual(tree.root_node.type, "program")
        self.assertFalse(tree.root_node.has_error)
        self.assertEqual(tree.root_node.text, source)
        self.assertIn("fault: (redirection_target_gap)", str(tree.root_node))


if __name__ == "__main__":
    unittest.main()
