"""IFJ26 infrastructure checks using temporary fixtures, not compiler tests.

AI prompt: tady jsou materiály k projektu z IFJ. chci aby jsi použil šablonu a upravil ji tak, aby byla ready na tento projekt. nechci v ní nic zbytečně navíc
"""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

REPO = Path(__file__).resolve().parents[2]


def load(name):
    spec = importlib.util.spec_from_file_location(name, REPO / 'tools' / f'{name}.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


pack = load('pack')
dev = load('dev')


class InfrastructureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        self.stack.enter_context(patch.object(dev, 'ROOT', self.root))
        self.stack.enter_context(contextlib.redirect_stdout(io.StringIO()))

    def write(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data.encode() if isinstance(data, str) else data)
        return path

    def fixture(self):
        # Tiny standalone executable, NOT an IFJ implementation or symbol table.
        self.write('src/main.c', 'int main(void) { return 0; }\n')
        self.write('src/symtable.c', '#include "symtable.h"\nint fixture = 0;\n')
        self.write('src/symtable.h', 'extern int fixture;\n')
        self.write('src/.gitkeep', '')
        shutil.copyfile(REPO / 'Makefile', self.root / 'Makefile')
        self.write('dokumentace.pdf', b'%PDF-1.4\nFixture: header check only, not a document.\n')
        self.write('rozdeleni', ''.join(f'{100 + i}:xtest{i}:25\n' for i in range(4)))

    def fake_programs(self, output='ok\n'):
        compiler = self.write('ifj26', '#!/bin/sh\nprintf ".IFJcode26\\n"\n')
        compiler.chmod(0o755)
        interpreter = self.write('fake-interpreter', '#!/bin/sh\ntest -f "$1" || exit 55\ncat\n')
        interpreter.chmod(0o755)
        self.write('tests/system/basic.ifj', 'test fixture, not IFJ source\n')
        self.write('tests/system/basic.json', json.dumps({'compiler_exit': 0, 'interpreter_exit': 0}))
        self.write('tests/system/basic.in', 'ok\n')
        self.write('tests/system/basic.out', output)
        self.stack.enter_context(patch.dict(os.environ, {'IC26INT': str(interpreter)}))
        return interpreter

    def test_pack_has_exact_allowlist_and_builds_without_tools(self):
        self.fixture()
        self.write('README.md', 'Must not ship')
        self.write('.github/README.md', 'Must not ship')
        self.write('docs/report.md', 'Must not ship')
        self.write('tests/unit/test_demo.c', 'Must not ship')
        self.write('ifj26', b'old binary must not ship')
        pack.package(self.root)
        with zipfile.ZipFile(self.root / 'dist/ifj26.zip') as archive:
            self.assertEqual(set(archive.namelist()), {
                'Makefile', 'dokumentace.pdf', 'rozdeleni',
                'src/main.c', 'src/symtable.c', 'src/symtable.h'})
            self.assertIsNone(archive.testzip())
        # The plain build already ran in isolation inside package().
        self.assertFalse((self.root / 'build').exists())

    def test_invalid_distribution_and_placeholders_fail(self):
        self.fixture()
        for contents in ('DOPLNIT\n', '1:xa:25\r\n2:xb:25\r\n3:xc:25\r\n4:xd:25\r\n',
                         '1:xa:25\n2:xb:25\n3:xc:25\n4:xd:24\n',
                         '1:xa:25\n1:xb:25\n3:xc:25\n4:xd:25\n',
                         '1:xa:25\n2:xa:25\n3:xc:25\n4:xd:25\n',
                         '1:xa:25\n2:xb:25\n3:xc:25\n4:xd:25'):
            with self.subTest(contents=contents):
                path = self.write('rozdeleni', contents)
                with self.assertRaises(ValueError):
                    pack.distribution(path, 4)

    def test_approved_three_member_distribution(self):
        path = self.write('rozdeleni', '1:xa:34\n2:xb:33\n3:xc:33\n')
        pack.distribution(path, 3)
        with self.assertRaises(ValueError):
            pack.distribution(path, 4)

    def test_unexpected_source_and_subdirectory_rejected(self):
        self.fixture()
        for name in ('src/module.o', 'src/.env', 'src/bad-name.c'):
            with self.subTest(name=name):
                path = self.write(name, 'not allowed')
                with self.assertRaises(ValueError):
                    pack.inputs(self.root, 4)
                path.unlink()
        (self.root / 'src/nested').mkdir()
        with self.assertRaises(ValueError):
            pack.inputs(self.root, 4)

    def test_symlink_and_missing_pdf_rejected(self):
        self.fixture()
        path = self.root / 'dokumentace.pdf'
        path.unlink()
        with self.assertRaises(ValueError):
            pack.inputs(self.root, 4)
        path.symlink_to(self.root / 'Makefile')
        with self.assertRaises(ValueError):
            pack.inputs(self.root, 4)

    def test_missing_source_does_not_fake_success(self):
        shutil.copyfile(REPO / 'Makefile', self.root / 'Makefile')
        run = subprocess.run(['make'], cwd=self.root, capture_output=True)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn(b'Chybi zdroje', run.stderr)

    def test_empty_test_suites_fail(self):
        with self.assertRaisesRegex(ValueError, 'unit'):
            dev.unit()
        with self.assertRaisesRegex(ValueError, 'systémové'):
            dev.system()

    def test_system_uses_runtime_input_and_compares_exact_bytes(self):
        self.fake_programs()
        dev.system()
        self.write('tests/system/basic.out', 'ok')
        with self.assertRaisesRegex(ValueError, 'bajtově'):
            dev.system()

    def test_compile_only_does_not_claim_runtime_execution(self):
        interpreter = self.fake_programs()
        interpreter.unlink()
        dev.system(compile_only=True)
        with self.assertRaisesRegex(ValueError, 'interpret'):
            dev.system()

    def test_bad_compiler_exit_fails(self):
        self.fake_programs()
        self.write('tests/system/basic.json', '{"compiler_exit": 1}')
        with self.assertRaisesRegex(ValueError, 'překladač vrátil'):
            dev.system(compile_only=True)

    def test_unit_program_is_built_and_executed(self):
        self.fixture()
        self.write('tests/unit/test_fixture.c', '#include "symtable.h"\nint main(void) { return fixture; }\n')
        dev.unit()
        self.assertTrue((self.root / 'build/unit/test_fixture').exists())
        self.write('tests/unit/test_fixture.c', 'int main(void) { return 1; }\n')
        with self.assertRaises(subprocess.CalledProcessError):
            dev.unit()


if __name__ == '__main__':
    unittest.main()
