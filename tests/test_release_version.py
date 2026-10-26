import importlib.util
from pathlib import Path
import tempfile
import tomllib
import unittest


spec = importlib.util.spec_from_file_location(
    "bump_version", Path(__file__).resolve().parents[1] / "scripts/bump-version.py"
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReleaseVersionTest(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)
        fixture = {
            "Cargo.toml": '[package]\nname = "pulsepointer"\nversion = "1.2.3"\n',
            "Cargo.lock": (
                'version = 4\n\n[[package]]\nname = "dependency"\nversion = "1.2.3"\n'
                '\n[[package]]\nname = "pulsepointer"\nversion = "1.2.3"\n'
            ),
            "kwin-effect/CMakeLists.txt": (
                "project(pulsepointer-kwin-effect VERSION 1.2.3 LANGUAGES CXX)\n"
            ),
            "README.md": "Install pulsepointer_1.2.3-1+ubuntu26.10_amd64.deb\nOther version: 1.2.30\n",
            "docs/distribution-packaging.md": "Push v1.2.3\n",
        }
        for name, content in fixture.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)

    def snapshot(self):
        return {str(p.relative_to(self.root)): p.read_bytes()
                for p in self.root.rglob("*") if p.is_file()}

    def test_increments_keep_versions_and_examples_in_sync(self):
        for increment, expected in (("patch", "1.2.4"), ("minor", "1.3.0"), ("major", "2.0.0")):
            with self.subTest(increment=increment):
                # Restore the fixture between independent increments.
                before = self.snapshot()
                self.assertEqual(module.bump_version(self.root, increment), expected)
                manifest = tomllib.loads((self.root / "Cargo.toml").read_text())
                lock = tomllib.loads((self.root / "Cargo.lock").read_text())
                self.assertEqual(manifest["package"]["version"], expected)
                self.assertEqual(lock["package"][0]["version"], "1.2.3")
                self.assertEqual(lock["package"][1]["version"], expected)
                self.assertIn(f"VERSION {expected} LANGUAGES", (self.root / "kwin-effect/CMakeLists.txt").read_text())
                self.assertIn(f"pulsepointer_{expected}-1+ubuntu26.10", (self.root / "README.md").read_text())
                self.assertIn("Other version: 1.2.30", (self.root / "README.md").read_text())
                self.assertEqual((self.root / "docs/distribution-packaging.md").read_text(), f"Push v{expected}\n")
                for name, content in before.items():
                    (self.root / name).write_bytes(content)

    def test_mismatched_versions_do_not_modify_files(self):
        for name in ("Cargo.lock", "kwin-effect/CMakeLists.txt"):
            with self.subTest(file=name):
                path = self.root / name
                original = path.read_text()
                path.write_text(original.replace("1.2.3", "1.2.2"))
                before = self.snapshot()
                with self.assertRaisesRegex(ValueError, "must match"):
                    module.bump_version(self.root, "patch")
                self.assertEqual(self.snapshot(), before)
                path.write_text(original)

    def test_prerelease_and_unknown_increment_are_rejected(self):
        before = self.snapshot()
        with self.assertRaisesRegex(ValueError, "Unknown"):
            module.bump_version(self.root, "invalid")
        self.assertEqual(self.snapshot(), before)
        manifest = self.root / "Cargo.toml"
        manifest.write_text(manifest.read_text().replace("1.2.3", "1.2.3-beta.1"))
        before = self.snapshot()
        with self.assertRaisesRegex(ValueError, "stable"):
            module.bump_version(self.root, "patch")
        self.assertEqual(self.snapshot(), before)


if __name__ == "__main__":
    unittest.main()
