#!/usr/bin/env python3
"""IFJ26: build/test support, not compiler implementation.

Prompt: tady jsou materiály k projektu z IFJ. chci aby jsi použil šablonu a upravil ji tak, aby byla ready na tento projekt. nechci v ní nic zbytečně navíc
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CODES = {0, 1, 2, 3, 4, 5, 6, 7, 10, 99}


def files():
    return sorted(p for folder in (ROOT / 'src', ROOT / 'tests/unit')
                  for p in folder.glob('*') if p.suffix in {'.c', '.h'})


def unit():
    tests = sorted((ROOT / 'tests/unit').glob('test_*.c'))
    if not tests:
        raise ValueError('Žádné unit testy: doplň tests/unit/test_*.c s vlastním main().')
    sources = sorted(p for p in (ROOT / 'src').glob('*.c') if p.name != 'main.c')
    directory = ROOT / 'build/unit'
    directory.mkdir(parents=True, exist_ok=True)
    flags = lambda name, default='': shlex.split(os.environ.get(name, default))
    for test in tests:
        target = directory / test.stem
        command = (flags('CC', 'gcc') + flags('CPPFLAGS', '-Isrc')
                   + flags('CFLAGS', '-std=c11 -Wall -Wextra -Wpedantic -O2')
                   + [str(test), *map(str, sources)] + flags('LDFLAGS')
                   + flags('LDLIBS') + ['-o', str(target)])
        subprocess.run(command, cwd=ROOT, check=True)
        subprocess.run([str(target)], cwd=ROOT, check=True, timeout=10)
        print(f'OK unit: {test.name}', flush=True)
    print(f'Unit: {len(tests)} testovacích programů.')


def system(compile_only=False):
    tests = sorted((ROOT / 'tests/system').rglob('*.ifj'))
    if not tests:
        raise ValueError('Žádné systémové případy: doplň .ifj a .json do tests/system/.')
    compiler = ROOT / 'ifj26'
    if not compiler.is_file():
        raise ValueError('Chybí ifj26; nejprve make.')
    interpreter = None
    if not compile_only:
        interpreter = shutil.which(os.environ.get('IC26INT', 'ic26int'))
        if not interpreter:
            raise ValueError('Chybí školní interpret. Nastav IC26INT na spustitelný ic26int.')
    completed = 0
    runtime_count = 0
    with tempfile.TemporaryDirectory(prefix='ifj26-tests-') as tmp:
        for test in tests:
            expected = json.loads(test.with_suffix('.json').read_text())
            if not isinstance(expected, dict) or set(expected) - {'compiler_exit', 'interpreter_exit'}:
                raise ValueError(f'{test}: neplatný manifest.')
            code = expected.get('compiler_exit')
            if type(code) is not int or code not in CODES:
                raise ValueError(f'{test}: neplatný compiler_exit.')
            if code == 0:
                runtime_code = expected.get('interpreter_exit')
                if type(runtime_code) is not int or not 0 <= runtime_code <= 255:
                    raise ValueError(f'{test}: úspěšný překlad potřebuje interpreter_exit.')
                if not test.with_suffix('.out').is_file():
                    raise ValueError(f'{test}: chybí očekávaný výstup .out (i prázdný).')
            with test.open('rb') as source:
                result = subprocess.run([str(compiler)], stdin=source, capture_output=True,
                                        cwd=ROOT, timeout=10)
            if result.returncode != code:
                raise ValueError(f'{test}: překladač vrátil {result.returncode}, očekáváno {code}.\n'
                                 + result.stderr.decode(errors='replace')[:2000])
            # AI prompt: Při chybě překladu ověř prázdný stdout a diagnostiku na stderr.
            # Model: GPT-6, Codex.
            if code != 0:
                if result.stdout:
                    raise ValueError(f'{test}: překladač při chybě zapisuje na stdout.')
                if not result.stderr.strip():
                    raise ValueError(f'{test}: chybí diagnostika překladače na stderr.')
            if code == 0:
                if not result.stdout.startswith(b'.IFJcode26\n') and not result.stdout.startswith(b'.IFJcode26\r\n'):
                    raise ValueError(f'{test}: chybí úvodní řádek .IFJcode26.')
                if not compile_only:
                    program = Path(tmp) / 'program.code'
                    program.write_bytes(result.stdout)
                    input_path = test.with_suffix('.in')
                    data = input_path.read_bytes() if input_path.exists() else b''
                    run = subprocess.run([interpreter, str(program)], input=data,
                                         capture_output=True, cwd=ROOT, timeout=10)
                    if run.returncode != runtime_code:
                        raise ValueError(f'{test}: interpret vrátil {run.returncode}, očekáváno {runtime_code}.\n'
                                         + run.stderr.decode(errors='replace')[:2000])
                    if run.stdout != test.with_suffix('.out').read_bytes():
                        raise ValueError(f'{test}: výstup interpretu se bajtově liší od .out.')
                    runtime_count += 1
            completed += 1
            print(f'OK {"compile" if compile_only else "system"}: {test.relative_to(ROOT)}', flush=True)
    if compile_only:
        print(f'Pouze překlad: {completed} případů. Interpretace NEPROBĚHLA.')
    else:
        if runtime_count == 0:
            raise ValueError('Chybí alespoň jeden úspěšně přeložený a interpretovaný případ.')
        print(f'System: {completed} případů, {runtime_count} spuštěno interpretem.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['unit', 'system', 'format', 'format-check', 'check'])
    parser.add_argument('--compile-only', action='store_true')
    args = parser.parse_args()
    if args.compile_only and args.command != 'system':
        parser.error('--compile-only patří jen k system')
    if args.command == 'unit':
        unit()
    elif args.command == 'system':
        system(args.compile_only)
    else:
        sources = files()
        if not sources:
            raise ValueError('Žádné C soubory ke kontrole.')
        if args.command in {'format', 'format-check'}:
            mode = ['-i'] if args.command == 'format' else ['--dry-run', '--Werror']
            command = ['clang-format', *mode, *map(str, sources)]
        else:
            command = ['cppcheck', '--enable=warning,style,performance,portability',
                       '--error-exitcode=1', '--check-level=exhaustive', '--std=c11', '--inline-suppr',
                       '--suppress=missingIncludeSystem', '-Isrc', 'src']
        subprocess.run(command, cwd=ROOT, check=True)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit(f'ERROR: {error}')
