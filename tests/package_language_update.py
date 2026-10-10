"""Package the verified 1.3.1 installer and an overlay for the existing repository.

The native report must identify these exact EBOOT/assets bytes. Historical
verification and owner screenshots stay untouched; new records use UPDATE-1.3.1.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent.parent
BASE = '1c6e6f89b3de161db202906f351b053500b6b99d'
VERSION = '1.3.1'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def changed_paths():
    paths = set(subprocess.check_output(['git','diff','--name-only','-z',BASE],cwd=ROOT).decode().split('\0'))
    paths.update(subprocess.check_output(['git','ls-files','--others','--exclude-standard','-z'],cwd=ROOT).decode().split('\0'))
    return sorted(p for p in paths if p and (ROOT/p).is_file())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build = ROOT/'build'
    native = json.loads((build/'language-update-native-verification.json').read_text())
    audit = json.loads((build/'psp-audit.json').read_text())
    locale = json.loads((build/'localization-verification.json').read_text())
    ui = json.loads((build/'ui-qa.json').read_text())
    assert native['status'] == audit['status'] == locale['status'] == 'PASS'
    assert native['version'] == VERSION and native['languages'] == locale['languages'] == 28
    assert native['verified_codes'] == ['pt','fr']
    assert native['signed_eboot_sha256'] == audit['signed_eboot_sha256'] == sha(ROOT/'EBOOT-SIGNED.PBP')
    assert sha(ROOT/'PSP/GAME/CalcAero/EBOOT.PBP') == native['signed_eboot_sha256']
    for path, expected in native['assets'].items():
        assert sha(ROOT/path) == expected, path
    log = (build/'host-1.3.1.log').read_text()
    assert '"checks": 22537' in log and '"checks": 2878' in log
    assert 'PASS: 28 language catalogs' in log and 'PASS: UI 1748 states' in log
    assert ui['states'] == 1748 and ui['geometry_in_bounds']
    assert 'Traceback' not in log and 'ERROR: AddressSanitizer' not in log
    assert not any(p.name.startswith(('config.json','state.dat','aero.dat'))
                   for p in (ROOT/'PSP/GAME/CalcAero').iterdir())
    verify = ROOT/'VERIFY/UPDATE-1.3.1'
    verify.mkdir(parents=True,exist_ok=True)
    for name in ['language-update-native-verification.json','psp-audit.json',
                 'localization-verification.json','ui-qa.json','numeric-verification.json',
                 'extensions-verification.json','host-1.3.1.log','psp-1.3.1.log',
                 'language-update-controller.log','language-update-final-controller.log']:
        shutil.copy2(build/name,verify/name)
    screens = ROOT/native['screens_directory']
    shutil.copytree(screens,verify/'screens',dirs_exist_ok=True)
    game = build/'language-update-native/ppsspp/PSP/GAME/CalcAero'
    shutil.copytree(game/'reports',verify/'exports',dirs_exist_ok=True)
    shutil.copy2(screens/'english-first-boot-28-languages.png',ROOT/'PREVIEW/languages/en.png')
    (verify/'README.md').write_text(f'''# CalcAero 1.3.1 verification

The final signed EBOOT SHA-256 is `{native['signed_eboot_sha256']}`.
These are fresh records for the Portuguese/French update, with 28 catalogs and
535 messages per catalog. Older verification and physical reports remain in
their original directories and still describe their original release.

- Numerical regression: 22,537 + 2,878 = **25,415 checks**, PASS.
- Host ASan/UBSan: malformed input, save CRC/rollback, legacy language IDs,
  Portuguese/French configuration restore and controller navigation, PASS.
- UI: **{ui['states']} states**; {ui['max_vertices']} / {ui['capacity']} vertices;
  geometry within 480×272 and no missing required glyphs.
- ELF/SFO/container audit: User Mode, MEMSIZE=0, 4 MiB heap, 256 KiB stack,
  OFW 6.60 target, no CPU clock change, int-0/ebootsigner.
- Exact signed EBOOT executed in PPSSPP: PSP-1000, 32 MiB, 222 MHz,
  HLE 6.60/6.61; English first boot, 28 choices, Portuguese/French selection,
  immediate config.json writes, exact fractions, real TXT/BMP exports and
  fresh-process restore. All pages/help were covered by the host UI suite.

Native screenshots in `screens` are read from PSP VRAM. Each scene has both
VRAM buffers; one may contain the previous frame. The public English language
picker was checked visually. `exports` contains files produced by the signed
application itself, including Portuguese and French UTF-8 history.
Host images and an XMB background are produced by the actual C renderer.

The first controller run completed English/PT and the French selection/export.
Its last assertion incorrectly expected recalculating an identical formula to
add a duplicate history item. CalcAero deliberately deduplicates that item.
The corrected final check verifies a fresh autosave and the unchanged exact
answer/history. It was rerun in another fresh process after validating the
retained prefix, EBOOT, assets, exports and saved-state CRC. Both the original
trace and the successful final trace are included; the JSON records this scope.

Physical OFW/CFW tests for **1.3.1 are not recorded yet**. The owner's confirmed
OFW 6.61 and ARK-5 5.1.0 cIPL tests describe **1.3**. HLE is not Sony's loader.
The two new translations were prepared with AI assistance; native-speaker review
is welcome. English owner screenshots from 1.3 have not been altered.
''')
    # Full repository manifest verifies both the overlay and retained base files.
    tracked = set(subprocess.check_output(['git','ls-files','-z'],cwd=ROOT).decode().split('\0'))
    tracked.update(changed_paths())
    (ROOT/'SHA256SUMS.txt').write_text(''.join(f'{sha(ROOT/p)}  {p}\n'
        for p in sorted(tracked) if p and p!='SHA256SUMS.txt' and (ROOT/p).is_file()))
    stage = build/'language-update-package'
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir()
    shutil.copytree(ROOT/'PSP',stage/'PSP')
    for path in changed_paths():
        dst = stage/'GitHub-Update'/path
        dst.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/path,dst)
    (stage/'INSTALL-RU.txt').write_text('''CalcAero 1.3.1: Português + Français, 28 языков.

PSP: скопируй папку PSP на карту памяти с объединением папок.
Замени EBOOT.PBP и всю папку assets в PSP/GAME/CalcAero.
Не удаляй папку CalcAero и свои config.json, state.dat, aero.dat, backups, reports.
Новые языки: △ -> SELECT -> Português (27) / Français (28) -> X/START.

GitHub: содержимое GitHub-Update скопируй в существующую папку репозитория.
В GitHub Desktop: Commit to main -> Push origin.
Подробности: GitHub-Update/GITHUB-SETUP.md.
Не копируй GitHub-Update на PSP.

Проверки сборки/подписи, 25 415 численных проверок, 1 748 UI-состояний
и запуск/сохранения обоих языков в PPSSPP прошли.
Физический тест новой 1.3.1 на OFW/CFW ещё ожидается.
''')
    (stage/'README.md').write_text('''# CalcAero 1.3.1 update

**28 languages**, including Português and Français; 535 messages each.

- `PSP`: ready signed installation. Merge into existing storage; retain saves.
- `GitHub-Update`: changed files to merge into the existing repository.
- `INSTALL-RU.txt`: PSP and GitHub Desktop instructions.

Build, source, release notes, screenshots, test reports and hashes are included
in GitHub-Update. Firmware is not modified. This release's physical console
testing is pending; version 1.3's confirmed hardware reports stay separate.
''')
    files = sorted(p for p in stage.rglob('*') if p.is_file())
    (stage/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.relative_to(stage).as_posix()}\n' for p in files))
    files.append(stage/'SHA256SUMS.txt')
    output = args.output.resolve()
    output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for path in sorted(files):
            z.write(path,path.relative_to(stage).as_posix())
    with zipfile.ZipFile(output) as z:
        assert z.testzip() is None
        for row in z.read('SHA256SUMS.txt').decode().splitlines():
            digest,path = row.split('  ',1)
            assert hashlib.sha256(z.read(path)).hexdigest() == digest,path
        assert z.read('PSP/GAME/CalcAero/EBOOT.PBP') == (ROOT/'EBOOT-SIGNED.PBP').read_bytes()
        assert not any('/.git/' in x or x.endswith(('config.json','state.dat','aero.dat')) for x in z.namelist())
    result = dict(status='PASS',version=VERSION,languages=28,archive=str(output),
        bytes=output.stat().st_size,sha256=sha(output),files=len(files),
        signed_eboot_sha256=native['signed_eboot_sha256'],base_commit=BASE,
        new_physical_psp_test=False)
    (build/'language-update-package-verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__ == '__main__':
    main()
