"""Read-only root admission and diagnostics routing; never invoke a package build."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ReleaseOutputRoutingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='datalab-release-routing-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.package = self.root / 'datalab/build/release-authenticated/candidate'
        self.package.parent.mkdir(parents=True)
        self.logs = self.root / 'logs'
        self.logs.mkdir()

    def make(self, *args):
        env = {k: v for k, v in os.environ.items()
               if k in ('PATH', 'HOME', 'TMPDIR', 'DEVELOPER_DIR', 'SDKROOT')}
        return subprocess.run(['make', '--no-print-directory', '-C', str(ROOT),
                               'MAKE=/usr/bin/false', *args], env=env,
                              capture_output=True, text=True, timeout=15)

    def refuse(self, root, message):
        result = self.make(f'RELEASE_ROOT={root}',
                           f'RELEASE_DIAGNOSTICS_PARENT={self.logs}',
                           'release-output-root-conformance')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(message, result.stdout + result.stderr)

    def test_existing_package_is_preserved(self):
        self.package.mkdir()
        sentinel = self.package / 'sentinel'
        sentinel.write_text('keep')
        self.refuse(self.package, 'RELEASE_ROOT must be absent')
        self.assertEqual(sentinel.read_text(), 'keep')
        self.assertEqual(list(self.logs.iterdir()), [])

    def test_dangling_package_symlink_is_refused(self):
        self.package.symlink_to(self.root / 'missing')
        self.refuse(self.package, 'RELEASE_ROOT must be absent')
        self.assertTrue(self.package.is_symlink())
        self.assertFalse((self.root / 'missing').exists())

    def test_existing_or_symlinked_diagnostics_are_preserved(self):
        diagnostic = self.logs / 'candidate.diagnostics'
        for symlink in (False, True):
            with self.subTest(symlink=symlink):
                if symlink:
                    diagnostic.symlink_to(self.root / 'missing')
                else:
                    diagnostic.mkdir()
                self.refuse(self.package, 'Release diagnostics root must be absent')
                self.assertFalse(self.package.exists())
                if symlink:
                    self.assertTrue(diagnostic.is_symlink())
                    diagnostic.unlink()
                else:
                    diagnostic.rmdir()

    def test_unsafe_roots_are_refused(self):
        for suffix in ('/../escape', '//nested', '/./nested'):
            with self.subTest(suffix=suffix):
                result = self.make(f'RELEASE_ROOT={self.package}{suffix}', 'release-output-root-contract')
                self.assertNotEqual(result.returncode, 0)
        self.refuse(self.root / 'outside', 'RELEASE_ROOT must be a job-scoped')
        self.assertFalse(self.package.exists())

    def test_default_and_selected_diagnostics_parent(self):
        probe = self.root / 'probe.mk'
        probe.write_text('qualification-routing:\n\t@echo "$(RELEASE_DIAGNOSTICS_DIR)"\n')
        common = ('-f', 'Makefile', '-f', str(probe), f'RELEASE_ROOT={self.package}')
        default = self.make(*common, 'qualification-routing')
        selected = self.make(*common, f'RELEASE_DIAGNOSTICS_PARENT={self.logs}', 'qualification-routing')
        self.assertEqual(default.returncode, 0, default.stderr)
        self.assertEqual(selected.returncode, 0, selected.stderr)
        self.assertEqual(default.stdout.strip(), str(self.package) + '.diagnostics')
        self.assertEqual(selected.stdout.strip(), str(self.logs / 'candidate.diagnostics'))


class DeveloperToolTests(unittest.TestCase):
    def test_selected_developer_and_missing_tool_refusal(self):
        with tempfile.TemporaryDirectory(prefix='datalab-developer-tool-') as tmp:
            root = Path(tmp) / 'Selected Xcode.app'
            directory = root / 'Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin'
            directory.mkdir(parents=True)
            tool = directory / 'otool'
            tool.write_text('#!/bin/sh\nprintf "%s\\n" "selected:$1"\n')
            tool.chmod(0o755)
            env = dict(os.environ, DEVELOPER_DIR=str(root))
            helper = ROOT / 'tools/packaging/macos/developer-tool.sh'
            result = subprocess.run(['sh', str(helper), 'otool', '-L'], env=env,
                                    capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.strip(), 'selected:-L')
            missing = subprocess.run(['sh', str(helper), 'lipo', '-archs'], env=env,
                                     capture_output=True, text=True, timeout=5)
            self.assertNotEqual(missing.returncode, 0)
            self.assertIn('selected developer directory lacks', missing.stderr)
            unsupported = subprocess.run(['sh', str(helper), 'notarytool'], env=env,
                                         capture_output=True, text=True, timeout=5)
            self.assertEqual(unsupported.returncode, 2)


if __name__ == '__main__':
    unittest.main()
