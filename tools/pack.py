#!/usr/bin/env python3
"""IFJ26: strict submission packaging, not compiler implementation.

Prompt: tady jsou materiály k projektu z IFJ. chci aby jsi použil šablonu a upravil ji tak, aby byla ready na tento projekt. nechci v ní nic zbytečně navíc
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def regular(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError(f'Chybí běžný soubor nebo jde o symlink: {path.name}')
    return path


def distribution(path, team_size):
    raw = regular(path).read_bytes()
    if not raw.endswith(b'\n') or b'\r' in raw:
        raise ValueError('rozdeleni musí končit LF a nesmí obsahovat CRLF.')
    try:
        lines = raw.decode('ascii').splitlines()
    except UnicodeDecodeError as error:
        raise ValueError('rozdeleni musí být ASCII.') from error
    if len(lines) != team_size:
        raise ValueError(f'rozdeleni musí mít {team_size} členů.')
    ids, logins, total = set(), set(), 0
    for line in lines:
        match = re.fullmatch(r'([0-9]+):(x[a-z0-9]+):([0-9]{1,3})', line)
        if not match:
            raise ValueError('rozdeleni: použij VUT_ID:xlogin:PROCENTA bez placeholderů a mezer.')
        person, login, share = match.groups()
        if person in ids or login in logins or int(person) == 0:
            raise ValueError('rozdeleni: neplatná nebo opakovaná identita.')
        if int(share) > 100:
            raise ValueError('Podíl musí být 0 až 100.')
        ids.add(person)
        logins.add(login)
        total += int(share)
    if total != 100:
        raise ValueError('Součet podílů v rozdeleni musí být 100.')


def inputs(root, team_size):
    if team_size not in {3, 4}:
        raise ValueError("Tým musí mít čtyři členy, výjimečně schválené tři.")
    source = root / 'src'
    if source.is_symlink() or not source.is_dir():
        raise ValueError('src musí být běžná složka.')
    selected = {}
    for path in sorted(source.iterdir()):
        if path.name == '.gitkeep' and path.is_file() and not path.is_symlink():
            continue
        regular(path)
        if path.suffix not in {'.c', '.h'} or not re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9_.]*', path.name):
            raise ValueError(f'Nepovolený zdroj: {path.name}; pouze ploché .c/.h s jednoduchým názvem.')
        selected[f'src/{path.name}'] = path
    for name in ('main.c', 'symtable.c', 'symtable.h'):
        if f'src/{name}' not in selected:
            raise ValueError(f'Chybí src/{name}; main.c je konvence této kostry.')
    for name in ('Makefile', 'dokumentace.pdf', 'rozdeleni'):
        selected[name] = regular(root / name)
    if not selected['dokumentace.pdf'].read_bytes().startswith(b'%PDF-'):
        raise ValueError('dokumentace.pdf nemá PDF hlavičku.')
    distribution(root / 'rozdeleni', team_size)
    return selected


def package(root=ROOT, team_size=4):
    selected = inputs(root, team_size)
    # Build only staged inputs; tools/, docs sources and local output cannot help.
    with tempfile.TemporaryDirectory(prefix='ifj26-submission-') as tmp:
        directory = Path(tmp)
        staged = directory / 'unpacked'
        staged.mkdir()
        archive = directory / 'ifj26.zip'
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
            for name, path in sorted(selected.items()):
                target = staged / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, target)
                output.write(path, name)
        # Do not inherit outer make state or caller-specific compiler flags.
        environment = dict(os.environ)
        for name in ('MAKEFLAGS', 'MFLAGS', 'MAKELEVEL', 'MAKEOVERRIDES', 'MAKEFILES',
                     'CC', 'CFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LDLIBS'):
            environment.pop(name, None)
        subprocess.run(['make'], cwd=staged, env=environment, check=True, timeout=120)
        if not (staged / 'ifj26').is_file() or not os.access(staged / 'ifj26', os.X_OK):
            raise ValueError('Samostatné make nevytvořilo spustitelný ifj26.')
        destination = root / 'dist'
        if destination.is_symlink():
            raise ValueError('dist nesmí být symlink.')
        destination.mkdir(exist_ok=True)
        pending = destination / 'ifj26.zip.tmp'
        if pending.is_symlink():
            raise ValueError('Dočasný archiv nesmí být symlink.')
        shutil.copyfile(archive, pending)
        pending.replace(destination / 'ifj26.zip')
    print('Vytvořeno dist/ifj26.zip; sestavení ze samotných vstupů archivu prošlo.')
    for name in sorted(selected):
        print(f'  {name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--team-size', type=int, choices=[3, 4], default=4)
    args = parser.parse_args()
    try:
        package(team_size=args.team_size)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit(f'ERROR: {error}')
