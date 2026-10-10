"""Run the signed Portuguese/French update with PSP controller events and read-only VRAM.

Usage: python3 tests/verify_language_update.py --ppsspp-root /path/to/AppDir
The test creates a separate Memory Stick, exports real TXT/BMP files, and starts
a fresh emulator process to verify config.json and mathematical state restore.
"""
import argparse
import asyncio
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess

import websockets
import emulator_verify as e
from build_locale import LANGS

ROOT = e.ROOT
VERSION = re.search(r'#define AERO_VERSION "([^"]+)"',
                    (ROOT / 'src/aero.h').read_text()).group(1)
PT = LANGS.index('pt')
FR = LANGS.index('fr')
HISTORY_PATTERN = 'CalcAero-[0-9][0-9][0-9][0-9].txt'


async def connect():
    for _ in range(80):
        try:
            return await websockets.connect('ws://127.0.0.1:24680/debugger',
                subprotocols=['debugger.ppsspp.org'], open_timeout=1, max_size=2000000)
        except OSError:
            await asyncio.sleep(.25)
    raise RuntimeError('PPSSPP debugger unavailable')


async def chosen_language(expected):
    for _ in range(100):
        try:
            if json.loads((e.GAME / 'config.json').read_text())['language'] == expected:
                return
        except (OSError, ValueError):
            pass
        await asyncio.sleep(.1)
    raise AssertionError('Language was not saved: ' + expected)


async def select(p, target, old):
    await p.press('triangle')
    await p.press('select')
    p.page = 12
    await p.moves('down', 'up', (target-old) % len(LANGS), (old-target) % len(LANGS))
    await p.press('cross')
    p.page = 0
    await chosen_language(LANGS[target])


async def exercise(restart):
    async with await connect() as ws:
        p = e.Pad(ws)
        version = await p.request('version')
        game = None
        for _ in range(100):
            game = (await p.request('game.status')).get('game')
            if game:
                break
            await asyncio.sleep(.1)
        assert game and game['title'] == 'CalcAero ' + VERSION
        # Hold a neutral input through startup: Unicode's first frame can be slow
        # under the software GPU. Do not send keypad arrows while it is drawing.
        for _ in range(24):
            await p.request('input.buttons.press', button='wlan', duration=10)
        if restart == 2:
            await chosen_language('fr')
            assert e.state()['expr'] == '1/2' and e.state()['ans'][0] == .5
            before = e.state()['history']
            before_mtime = (e.GAME / 'state.dat').stat().st_mtime_ns
            await p.press('start')
            for _ in range(100):
                if (e.GAME / 'state.dat').stat().st_mtime_ns != before_mtime:
                    break
                await asyncio.sleep(.1)
            assert (e.GAME / 'state.dat').stat().st_mtime_ns != before_mtime
            assert e.state()['history'] == before and e.state()['ans'][0] == .5
            await p.screen('french-restart-661')
            e.checks.append('Third fresh process restored French, exact fraction and history; existing formula recalculated')
            return version
        if restart == 1:
            await chosen_language('pt')
            assert e.state()['expr'] == '1/2' and e.state()['ans'][0] == .5
            await p.screen('portuguese-restart-661')
            await select(p, FR, PT)
            await p.screen('french-calculator')
            count = len(list((e.GAME / 'reports').glob(HISTORY_PATTERN)))
            await p.menu(14)
            french_txt = await e.wait_file(HISTORY_PATTERN, count + 1)
            assert 'Historique des calculs' in french_txt.read_text()
            assert french_txt.read_text().startswith('CalcAero ' + VERSION)
            await p.press('triangle')
            await p.press('select')
            p.page = 12
            await p.screen('french-28-language-picker')
            await p.press('circle')
            p.page = 0
            e.checks.append('Fresh 6.61 process restored Portuguese, fraction and history; French selected and exported UTF-8 TXT')
            return version
        assert not (e.GAME / 'config.json').exists()
        p.page = 12
        await p.screen('english-first-boot-28-languages')
        await p.press('up')
        await p.press('up')
        await p.screen('english-portuguese-selected')
        await p.press('cross')
        p.page = 0
        await chosen_language('pt')
        for _ in range(12):
            await p.request('input.buttons.press', button='wlan', duration=10)
        await p.expression('1/2')
        await p.press('start')
        await e.saved('1/2', .5)
        await p.screen('portuguese-calculator')
        await p.menu(17)
        bmp = await e.wait_file('CalcAero-Screen-*.bmp')
        assert bmp.stat().st_size == 391734
        await p.menu(14)
        history = await e.wait_file(HISTORY_PATTERN)
        assert 'Histórico de cálculos' in history.read_text()
        assert history.read_text().startswith('CalcAero ' + VERSION)
        await p.press('triangle')
        await p.press('select')
        p.page = 12
        await p.screen('portuguese-28-language-picker')
        await p.press('circle')
        p.page = 0
        e.checks.append('Fresh 6.60 process: English default, 28 choices, Portuguese immediate save, exact fraction, UTF-8 TXT and native BMP')
        return version


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--ppsspp-root', type=Path, required=True)
    parser.add_argument('--resume-final', action='store_true', help='Verify the successful controller prefix and rerun only the final fresh process')
    args = parser.parse_args()
    app = args.ppsspp_root.resolve()
    run = ROOT / 'build/language-update-native'
    if run.exists() and not args.resume_final:
        shutil.rmtree(run)
    e.MEM = run / 'ppsspp/PSP'
    e.GAME = e.MEM / 'GAME/CalcAero'
    e.SCREEN = run / 'screens'
    e.GAME.mkdir(parents=True,exist_ok=True)
    e.SCREEN.mkdir(exist_ok=True)
    (e.MEM / 'SYSTEM').mkdir(exist_ok=True)
    if args.resume_final:
        trace = (ROOT / 'build/language-update-controller.log').read_text()
        assert 'screen french-28-language-picker' in trace
        assert (e.GAME / 'EBOOT.PBP').read_bytes() == (ROOT / 'EBOOT-SIGNED.PBP').read_bytes()
        assert json.loads((e.GAME / 'config.json').read_text())['language'] == 'fr'
        assert e.state()['expr'] == '1/2' and e.state()['ans'][0] == .5
        texts = [f.read_text() for f in (e.GAME / 'reports').glob(HISTORY_PATTERN)]
        assert any('Histórico de cálculos' in t for t in texts)
        assert any('Historique des calculs' in t for t in texts)
        for f in (ROOT / 'assets').rglob('*'):
            if f.is_file():assert (e.GAME / f.relative_to(ROOT)).read_bytes() == f.read_bytes()
        e.checks.extend(['Verified retained prefix: English first boot with 28 choices, Portuguese exact fraction and exports', 'Verified retained prefix: Portuguese fresh-process restore, French selection and UTF-8 export'])
    shutil.copy2(ROOT / 'EBOOT-SIGNED.PBP', e.GAME / 'EBOOT.PBP')
    shutil.copytree(ROOT / 'assets', e.GAME / 'assets',dirs_exist_ok=True)
    env = dict(os.environ, SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy',
               XDG_CONFIG_HOME=str(run))
    cmd = [str(app / 'shared/lib/ld-linux-x86-64.so.2'), '--library-path',
           str(app / 'shared/lib'), str(app / 'shared/bin/PPSSPPSDL'),
           str(e.GAME / 'EBOOT.PBP')]
    with (ROOT / 'build/language-update-native.log').open('a' if args.resume_final else 'w') as log:
        for restart in ([2] if args.resume_final else [0, 1, 2]):
            (e.MEM / 'SYSTEM/ppsspp.ini').write_text(f'''[General]
FirstRun = False
RemoteDebuggerOnStartup = True
RemoteDebuggerLocal = True
RemoteISOPort = 24680
IgnoreBadMemAccess = False
Enable Logging = True
ReportingHost =
CheckForNewVersion = False
[SystemParam]
PSPModel = 0
PSPFirmwareVersion = {661 if restart else 660}
[Graphics]
GPUBackend = 0
InternalResolution = 1
SoftwareRenderer = True
[CPU]
CPUSpeed = 222
FastMemoryAccess = False
[Sound]
Enable = False
''')
            proc = subprocess.Popen(cmd, cwd=app / 'bin', env=env, stdout=log, stderr=log)
            try:
                version = asyncio.run(exercise(restart))
            finally:
                proc.terminate()
                try:
                    proc.wait(5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
    assert (e.GAME / 'EBOOT.PBP').read_bytes() == (ROOT / 'EBOOT-SIGNED.PBP').read_bytes()
    for f in (ROOT / 'assets').rglob('*'):
        if f.is_file():
            assert (e.GAME / f.relative_to(ROOT)).read_bytes() == f.read_bytes()
    result = dict(status='PASS',version=VERSION,emulator=version,
        model='PSP-1000',ram_mib=32,cpu_mhz=222,hle_firmware_profiles=[660,661],
        signed_eboot_sha256=hashlib.sha256((ROOT / 'EBOOT-SIGNED.PBP').read_bytes()).hexdigest(),
        languages=len(LANGS),verified_codes=['pt','fr'],controller_presses=e.press_count,
        controller_presses_scope='final resumed process' if args.resume_final else 'full run',
        verified_controller_prefix=args.resume_final,checks=e.checks,physical_ofw_660_test=False,physical_ofw_661_test=False,
        screens_directory=str(e.SCREEN.relative_to(ROOT)),
        assets={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                for p in (ROOT / 'assets').rglob('*') if p.is_file()})
    (ROOT / 'build/language-update-native-verification.json').write_text(
        json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print('PASS: signed Portuguese/French build, first boot, persistence, exports and fresh process',flush=True)


if __name__ == '__main__':
    main()
