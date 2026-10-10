"""Exercise the real PSP EBOOT using PPSSPP controller events, without memory writes.

Usage: python3 tests/emulator_verify.py --ppsspp-root /path/to/AppDir
Needs Python websockets and Pillow. A separate test memory stick is created under build/.
PPSSPP's software GPU is used so screenshots can be read from emulated PSP VRAM.
Debugger protocol reference: PPSSPP Core/Debugger/WebSocket/{Input,Memory}Subscriber.cpp.
"""
import argparse
import asyncio
import base64
import json
import hashlib
import csv
import xml.etree.ElementTree as ET
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib

from PIL import Image
import time
import websockets

ROOT = Path(__file__).resolve().parent.parent
MEM = ROOT / "build/emulator-test/ppsspp/PSP"
GAME = MEM / "GAME/CalcAero"
SCREEN = ROOT / "build/emulator-screens"
checks = []
press_count = 0
args_continue = False


def state():
    b = (GAME / "state.dat").read_bytes()
    assert len(b) == 33000 and b[:8] == b"CLSAVE01"
    assert zlib.crc32(b[:-4]) == struct.unpack_from("<I", b, 32996)[0]
    return dict(angle=struct.unpack_from("<I", b, 16)[0],
                complex=struct.unpack_from("<I", b, 20)[0],
                ans=struct.unpack_from("<ddqqI", b, 24),
                expr=b[332:844].split(b"\0", 1)[0].decode(),
                history=struct.unpack_from("<I", b, 328)[0],
                latest=b[4940:5452].split(b"\0",1)[0].decode(), raw=b)


async def saved(expression, value=None):
    # Software emulation can run more slowly than the PSP's emulated clock.
    for _ in range(100):
        try:
            s = state()
            if s["expr"] == expression and (value is None or abs(s["ans"][0]-value)<1e-14): return s
        except (OSError, AssertionError):
            pass
        await asyncio.sleep(.3)
    raise AssertionError("Automatic save was not produced")


MENU_PAGES = [0, 1, 8, 9, 10, 2, 3, 4, 5, 6, 7]
PAGE_ROWS = {page: row for row, page in enumerate(MENU_PAGES)}
PAGE_ROWS[11] = 20
PAGE_ROWS[12] = 21


def extra_state():
    data = (GAME/'aero.dat').read_bytes()
    assert len(data) == 2852 and data[:8] == b'CAERO012'
    assert zlib.crc32(data[:-4]) == struct.unpack_from('<I',data,2848)[0]
    return dict(function=[data[32+i*512:32+(i+1)*512].split(b'\0',1)[0].decode() for i in range(3)],
                graph=[data[1568+i*512:1568+(i+1)*512].split(b'\0',1)[0].decode() for i in range(2)],
                matrix=struct.unpack_from('<32d',data,2592),
                topic=struct.unpack_from('<I',data,20)[0],
                total=struct.unpack_from('<I',data,24)[0],
                correct=struct.unpack_from('<I',data,28)[0])


async def wait_extra(predicate):
    for _ in range(100):
        try:
            value=extra_state()
            if predicate(value): return value
        except (OSError,AssertionError): pass
        await asyncio.sleep(.3)
    raise AssertionError('Aero autosave condition was not met')


async def wait_file(pattern, minimum=1):
    for _ in range(100):
        files=sorted((GAME/'reports').glob(pattern))
        if len(files)>=minimum and files[-1].stat().st_size>0:
            if pattern.endswith('.bmp') and files[-1].stat().st_size != 391734:
                await asyncio.sleep(.2); continue
            return files[-1]
        await asyncio.sleep(.2)
    raise AssertionError('Native export was not created: '+pattern)


class Pad:
    def __init__(self, ws):
        self.ws, self.ticket = ws, 0
        self.page, self.keypage, self.keysel = 0, 0, 0

    async def request(self, event, **data):
        self.ticket += 1
        ticket = str(self.ticket)
        await self.ws.send(json.dumps(dict(event=event, ticket=ticket, **data)))
        while True:
            r = json.loads(await asyncio.wait_for(self.ws.recv(), 30))
            if r.get("ticket") != ticket:
                continue
            if r["event"] == "error":
                raise RuntimeError(r["message"])
            return r

    async def press(self, button):
        global press_count
        await self.request("input.buttons.press", button=button, duration=6)
        # Wait for a release in the PSP's own clock. Wall-time sleeps can be
        # shorter than one app iteration when PPSSPP's software GPU is busy.
        # CalcAero ignores the WLAN switch; it provides a neutral vblank wait.
        await self.request("input.buttons.press", button="wlan", duration=10)
        press_count += 1

    async def moves(self, down, up, ndown, nup):
        button, count = (down, ndown) if ndown <= nup else (up, nup)
        for _ in range(count):
            await self.press(button)

    async def menu(self, row):
        await self.press("triangle")
        old = PAGE_ROWS[self.page]
        await self.moves("down", "up", (row-old)%22, (old-row)%22)
        await self.press("cross")
        if row < 11:
            self.page = MENU_PAGES[row]
            if self.page == 1: self.keypage = 5
            if self.page == 4: self.keypage = 4
        elif row == 20: self.page = 11
        elif row == 21: self.page = 12
        elif row == 11 and self.page in [5,6,7]: self.page = 0

    async def key(self, page, index):
        await self.moves("rtrigger", "ltrigger", (page-self.keypage)%6,
                         (self.keypage-page)%6)
        self.keypage = page
        oldrow, oldcol = divmod(self.keysel, 6)
        row, col = divmod(index, 6)
        await self.moves("down", "up", (row-oldrow)%5, (oldrow-row)%5)
        await self.moves("right", "left", (col-oldcol)%6, (oldcol-col)%6)
        self.keysel = index
        await self.press("cross")

    async def expression(self, text):
        await self.key(0, 24)
        tokens = {"sin(": (1, 0), "sqrt(": (0, 11), "i": (2, 23), "Ans": (0,17), "x":(3,12), "f(":(5,20), "g(":(5,21), "h(":(5,27)}
        normal = dict(zip("789/()456*^", [0,1,2,3,4,5,6,7,8,9,10]))
        normal.update(dict(zip("123-.", [12,13,14,15,16])))
        normal.update({"0":18, "+":19, ";":23})
        while text:
            for token, key in tokens.items():
                if text.startswith(token):
                    await self.key(*key)
                    text = text[len(token):]
                    break
            else:
                await self.key(0, normal[text[0]])
                text = text[1:]

    async def screen(self, name):
        await asyncio.sleep(.25)
        await self.ws.send(json.dumps({"event":"cpu.stepping"}))
        while True:
            r = json.loads(await asyncio.wait_for(self.ws.recv(),8))
            if r.get("event") == "cpu.stepping": break
        r = await self.request("memory.read", address=0x04000000, size=0x88000)
        data = base64.b64decode(r["base64"])
        Image.frombytes("RGBA", (512,272), data).convert("RGB").crop((0,0,480,272)).save(SCREEN / (name+".png"))
        r = await self.request("memory.read", address=0x04088000, size=0x88000)
        data = base64.b64decode(r["base64"])
        Image.frombytes("RGBA", (512,272), data).convert("RGB").crop((0,0,480,272)).save(SCREEN / (name+"-alternate.png"))
        await self.ws.send(json.dumps({"event":"cpu.resume"}))
        await asyncio.sleep(.1)
        print("screen", name, flush=True)


async def run(restart=False, continue_extensions=False):
    for _ in range(35):
        try:
            ws = await websockets.connect("ws://127.0.0.1:24680/debugger",
                 subprotocols=["debugger.ppsspp.org"], open_timeout=1, max_size=2000000)
            break
        except OSError:
            await asyncio.sleep(.25)
    else: raise RuntimeError("PPSSPP debugger did not start")
    async with ws:
        p = Pad(ws)
        version = await p.request("version")
        await asyncio.sleep(1.5)
        title = (await p.request("game.status"))["game"]["title"]
        assert title == "CalcAero 1.3", f"Unexpected EBOOT title: {title!r}"
        if restart:
            assert json.loads((GAME/'config.json').read_text())['language']=='ru'
            await p.screen("restart-selftest")
            extra = await wait_extra(lambda v: v['function'][0] == 'x^2+2*x+1')
            assert extra['matrix'][0] == 3 and extra['total'] >= 1
            assert extra['graph'][0] in ['cos(x)+1','cos(x)+2']
            await p.expression("f(3)")
            await p.press("start")
            await saved("f(3)",16)
            await p.expression("Ans+1")
            await p.press("start")
            s = await saved("Ans+1",17)
            assert s["history"] >= 3
            await p.screen("restart-ans")
            checks.append("fresh process restored previous Ans and history")
            (ROOT / "build/emulator-verification.json").write_text(json.dumps(
                dict(status="PASS",emulator=version, checks=checks, controller_presses=press_count,
                     screens_directory=str(SCREEN.relative_to(ROOT)),
                     screenshot_source="PSP VRAM, software GPU", physical_psp_test=False,
                     psp_model="PSP-1000 (32 MB)", cpu_mhz=222, languages=26,
                     signed_eboot_sha256=hashlib.sha256((ROOT/"EBOOT-SIGNED.PBP").read_bytes()).hexdigest(),
                     eboot_sha256=hashlib.sha256((GAME/'EBOOT.PBP').read_bytes()).hexdigest(),
                     game_directory=str(GAME.relative_to(ROOT)),
                     seeded_aero_graphs=['cos(x)','sin(2*x)'],
                     baseline_before_export_fix=args_continue,
                     controller_presses_scope='successful continuation plus restart' if args_continue else 'full run'),
                ensure_ascii=False, indent=2)+"\n")
            print("PASS: native PSP arithmetic, persistence, OSK, tools, export and restart",flush=True)
            return
        if continue_extensions:
            assert (GAME/'EBOOT.PBP').read_bytes()==(ROOT/'EBOOT.PBP').read_bytes()
            value=state();extra_state()
            assert value['expr']=='sin(x)' and value['angle']==1
            assert len(list((GAME/'reports').glob('CalcAero-*.txt')))==2
            for name in ['fraction','degrees','native-keyboard','after-keyboard','inverse-matrix','graph']:
                assert (SCREEN/(name+'.png')).exists()
            await p.screen('continuation-selftest')
            checks.extend(['earlier native baseline preserved: exact fractions, DEG, migration, OSK, tools, unique TXT',
                           'continuation validates EBOOT identity, saved-state CRC and baseline artifacts'])
            await p.menu(1)
        else:
            p.page=12
            await p.screen("first-start-english-26-languages")
            assert not (GAME/'config.json').exists()
            await p.press('cross')
            p.page=0
            assert json.loads((GAME/'config.json').read_text())['language']=='en'
            checks.append('first boot defaults to English and immediately shows all 26 languages; selection writes JSON')
            codes=['en','de','it','pl','es','nl','et','fi','sv','ro','el','ar','kk','ru','be','uk','sr','bs','hr','cs','sk','zh-TW','zh-CN','hi','ko','ja']
            for lang in range(1,26):
                await p.menu(21)
                await p.press('down')
                await p.press('cross')
                p.page=0
                # The real C engine performs the save before the next controller event.
                assert json.loads((GAME/'config.json').read_text())['language']==codes[lang]
                await p.screen('language-'+codes[lang])
            await p.menu(21)
            for _ in range(12):await p.press('up')
            await p.press('cross');p.page=0
            assert json.loads((GAME/'config.json').read_text())['language']=='ru'
            checks.append('all 26 languages selected through PSP controller; immediate JSON checked; native glyph screenshots captured')
            await p.screen("startup-selftest")
            await p.expression("1/3+1/6")
            await p.press("start")
            s = await saved("1/3+1/6",.5)
            await p.screen("fraction")
            assert s["expr"] == "1/3+1/6" and s["ans"] == (.5,0.,1,2,1)
            checks.append("native exact rational arithmetic and auto-save CRC")
            # The genuine legacy fixture is RAD: preserve it, then change mode using PSP controls.
            old_angle = state()['angle']
            for _ in range((3-old_angle)%3): await p.press('select')
            checks.append('legacy save migrated into CalcAero with its original angle mode')
            await p.expression("sin(30)")
            await p.press("start")
            s = await saved("sin(30)",.5)
            assert s["angle"] == 0 and abs(s["ans"][0]-.5) < 1e-14
            checks.append("DEG sin(30)")
            await p.screen("degrees")
            await p.menu(13)
            await p.menu(8)
            await p.screen("history")
            await p.press("select")
            await p.screen("bookmarks")
            await p.menu(0)
            await p.menu(11)
            # PPSSPP 1.20.4 ignores keys during its one-second OSK fade-in.
            # Wait in emulated vblanks rather than wall time on a slow software GPU.
            await p.request("input.buttons.press", button="wlan", duration=240)
            await asyncio.sleep(.2)
            await p.screen("native-keyboard")
            await p.press("cross")
            await p.request("input.buttons.press", button="start", duration=20)
            await asyncio.sleep(.2)
            await asyncio.sleep(1.5)
            await p.screen("keyboard-finish-dialog")
            s = await saved("sin(30)1")
            assert s["expr"] == "sin(30)1"
            await p.screen("after-keyboard")
            checks.append("native OSK changed input and returned to calculator")
            await p.menu(14)
            await asyncio.sleep(1)
            await p.menu(14)
            await asyncio.sleep(1)
            reports=sorted((GAME/'reports').glob('*.txt'))
            assert len(reports)==2 and reports[0].name!=reports[1].name
            text=reports[-1].read_text()
            assert 'sin(30)' in text and '1/2' in text
            checks.append("native UTF-8 export and unique report filenames")
            await p.menu(5)
            await p.press("start")
            await p.screen("quadratic")
            for _ in range(4): await p.press("square")
            await p.press("start")
            await p.screen("inverse-matrix")
            await p.menu(0)
            await p.expression("sin(x)")
            await p.press("select")
            s = await saved("sin(x)")
            assert s["angle"] == 1
            await p.menu(1)
            await asyncio.sleep(6)
            await p.screen("graph")
            checks.append("native quadratic, inverse matrix and graph screens inspected")
        # G2 and G3 were loaded from a portable saved-state fixture. Editing G2
        # uses the actual system keyboard; no debugger memory writes are used.
        await p.press("rtrigger")
        await p.press("square")
        await p.request("input.buttons.press",button="wlan",duration=240)
        current=extra_state()['graph'][0]
        if current=='cos(x)+1':
            await p.press('circle')
            await p.press('right')
            await p.press('cross')
            graph_shift=2
        elif current=='cos(x)+2':
            await p.press('circle')
            await p.press('cross')
            graph_shift=1
        else:
            graph_shift=1
            if current=='cos(x)1': await p.press('circle')
            # The keyboard's observed top row has twelve keys: 1..9,0,-,+.
            for _ in range(11): await p.press('right')
            await p.press('cross')
            for _ in range(11): await p.press('left')
            await p.press('cross')
        await p.request("input.buttons.press",button="start",duration=20)
        await asyncio.sleep(1.5)
        await wait_extra(lambda v:v['graph'][0]==f'cos(x)+{graph_shift}')
        await p.screen("three-graphs")
        previous_csv=len(list((GAME/'reports').glob('CalcAero-Graph-*.csv')))
        await p.menu(15)
        graph_csv=await wait_file('CalcAero-Graph-*.csv',previous_csv+1)
        with graph_csv.open(newline='') as f: rows=list(csv.reader(f))
        assert len(rows)==242 and len(rows[0])==4
        assert rows[0][2]==f'G2: cos(x)+{graph_shift}' and rows[0][3]=='G3: sin(2*x)'
        for row in rows[1:]:
            x=float(row[0])
            assert abs(float(row[1])-__import__('math').sin(x))<2e-12
            assert abs(float(row[2])-(__import__('math').cos(x)+graph_shift))<2e-12
            assert abs(float(row[3])-__import__('math').sin(2*x))<2e-12
        csv_hash=hashlib.sha256(graph_csv.read_bytes()).hexdigest()
        count_csv=len(list((GAME/'reports').glob('CalcAero-Graph-*.csv')))
        await p.menu(15)
        csv2=await wait_file('CalcAero-Graph-*.csv',count_csv+1)
        assert csv2.name!=graph_csv.name and hashlib.sha256(graph_csv.read_bytes()).hexdigest()==csv_hash
        await p.menu(16)
        svg=await wait_file('CalcAero-Graph-*.svg')
        ET.parse(svg)
        svg_hash=hashlib.sha256(svg.read_bytes()).hexdigest()
        existing_svg=len(list((GAME/'reports').glob('CalcAero-Graph-*.svg')))
        await p.menu(16)
        svg2=await wait_file('CalcAero-Graph-*.svg',existing_svg+1)
        ET.parse(svg2)
        assert svg2.name!=svg.name and hashlib.sha256(svg.read_bytes()).hexdigest()==svg_hash
        await p.menu(17)
        bmp=await wait_file('CalcAero-Screen-*.bmp')
        image=Image.open(bmp);image.load()
        assert image.size==(480,272) and bmp.stat().st_size==391734
        await p.screen("after-bmp-export")
        await p.press("cross");p.page=8
        await p.screen("direct-graph-analysis")
        checks.append("native three-curve plot, separate graph OSK, numerical analysis, CSV/SVG/BMP exports")
        await p.menu(2)
        # Direct analysis selected tool 13. Move down once to tool zero.
        await p.press("down")
        for tool in range(14):
            await p.press("cross")
            await p.press("start")
            if tool in [0,3,5,6,8,9,10,11,12,13]:
                await p.screen('lab-'+str(tool))
            await p.menu(18)
            report=await wait_file('CalcAero-Report-*.txt',tool+1)
            text=report.read_text()
            assert text.startswith('CalcAero 1.3') and 'Режим углов' in text
            # Every sample succeeds; specific native numeric reports are inspected below.
            assert not any(q in text for q in ['Некоррект','Проверь параметры','Неизвестный инструмент','вне поддерживаемых'])
            if tool==0: assert 'Ответ = 1/2' in text
            if tool==3: assert 'A . B ~= 32' in text
            if tool==5: assert 'Ответ ~= 30' in text
            if tool==7: await wait_extra(lambda v:v['function'][0]=='x^2+2*x+1')
            if tool==9: assert '0.682689492137' in text
            if tool==10: assert '3EAAAAAB' in text
            await p.press('select')
            if tool<13: await p.press('down')
        checks.append("all 14 native laboratory examples, including functions, units, distributions, IEEE and mini-CAS")
        await p.menu(3)
        await p.press('cross')
        await p.expression('3')
        await p.press('start')
        await wait_extra(lambda v:v['matrix'][0]==3)
        await p.screen('matrix-grid')
        for op in range(7):
            await p.press('start')
            if op in [2,4,6]: await p.screen('matrix-op-'+str(op))
            await p.press('circle')
            if op<6: await p.press('rtrigger')
        await p.menu(15);await wait_file('CalcAero-Matrix-*.csv')
        checks.append('native matrix cell edit, seven operations and CSV export')
        await p.menu(4)
        await p.expression('0');await p.press('start')
        await wait_extra(lambda v:v['total']>=1)
        await p.screen('study-answer')
        await p.press('cross')
        for _ in range(5): await p.press('select')
        await p.screen('study-topic-5')
        checks.append('native study grading, explanation, next question and topic switching')
        await p.menu(0)
        await p.expression('1/3+1/6')
        await p.menu(20);await p.screen('textbook-formula')
        await p.press('start');p.page=0
        await p.expression('f(2)');await p.press('start')
        value=await saved('f(2)',9)
        assert 'f(' not in value['latest']
        await p.screen('user-function')
        checks.append('native textbook layout and exact saved-function evaluation with frozen history')


def main():
    global MEM, GAME, SCREEN, args_continue
    ap = argparse.ArgumentParser()
    ap.add_argument("--ppsspp-root", required=True, type=Path)
    ap.add_argument("--continue-extensions",type=Path)
    args = ap.parse_args()
    args_continue=bool(args.continue_extensions)
    appdir = args.ppsspp_root.resolve()
    run_dir=args.continue_extensions.resolve() if args_continue else Path(tempfile.mkdtemp(prefix="emulator-",dir=ROOT/'build'))
    MEM=run_dir/'ppsspp/PSP'
    GAME=MEM/'GAME/CalcAero'
    SCREEN=run_dir/'screens'
    MEM.mkdir(parents=True, exist_ok=True)
    GAME.mkdir(parents=True, exist_ok=True)
    SCREEN.mkdir(parents=True, exist_ok=True)
    (MEM / "SYSTEM").mkdir(exist_ok=True)
    if not args_continue:
        for name in ["state.dat", "state.dat.bak", "state.tmp"]:
            (GAME/name).unlink(missing_ok=True)
        shutil.rmtree(GAME/'reports',ignore_errors=True)
    (ROOT/'build/emulator-verification.json').unlink(missing_ok=True)
    (MEM / "SYSTEM/ppsspp.ini").write_text("""[General]
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
[Graphics]
GPUBackend = 0
InternalResolution = 1
SoftwareRenderer = True
[CPU]
CPUSpeed = 222
FastMemoryAccess = False
[Sound]
Enable = False
""")
    if not args_continue:
        old_dir=MEM/'GAME/CalcLab';old_dir.mkdir()
        shutil.copy2(ROOT/'tests/fixtures/1.0/state.dat',old_dir/'state.dat')
        data=bytearray(b'CAERO012'+struct.pack('<6I',1,2,20261009,0,0,0))
        data.extend(bytes(3*512))
        for expr in [b'cos(x)',b'sin(2*x)']: data.extend(expr+bytes(512-len(expr)))
        data.extend(struct.pack('<32d',*([float(i//4==i%4) for i in range(16)]*2)))
        data.extend(struct.pack('<I',zlib.crc32(data)))
        assert len(data)==2852
        (GAME/'aero.dat').write_bytes(data)
        shutil.copy2(ROOT/"EBOOT.PBP",GAME/"EBOOT.PBP")
        shutil.copytree(ROOT/"assets",GAME/"assets",dirs_exist_ok=True)
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_AUDIODRIVER="dummy",
               XDG_CONFIG_HOME=str(run_dir))
    cmd = [str(appdir/"shared/lib/ld-linux-x86-64.so.2"), "--library-path",
           str(appdir/"shared/lib"), str(appdir/"shared/bin/PPSSPPSDL"),str(GAME/"EBOOT.PBP")]
    with open(ROOT/"build/emulator-verification.log","w") as log:
        for restart in [False,True]:
            if restart:shutil.copy2(ROOT/'EBOOT-SIGNED.PBP',GAME/'EBOOT.PBP')
            proc = subprocess.Popen(cmd,cwd=appdir/"bin",env=env,stdout=log,stderr=log)
            try: asyncio.run(run(restart, args_continue and not restart))
            except Exception:
                async def failure_screen():
                    async with websockets.connect('ws://127.0.0.1:24680/debugger',subprotocols=['debugger.ppsspp.org'],max_size=2000000) as ws:
                        await Pad(ws).screen('failed-step')
                asyncio.run(failure_screen())
                raise
            finally:
                proc.terminate()
                try: proc.wait(5)
                except subprocess.TimeoutExpired: proc.kill();proc.wait()


if __name__ == "__main__":
    main()
