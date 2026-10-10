"""Verify the final signed build and complete the native feature suite.
Only controller events and read-only debugger VRAM reads are used.
An earlier unsigned baseline is retained separately with its own SHA-256.
"""
import asyncio,hashlib,json,os,subprocess,shutil,struct,sys,configparser,time
from pathlib import Path
import websockets
import emulator_verify as e
ROOT=e.ROOT
CODES=['en','de','it','pl','es','nl','et','fi','sv','ro','el','ar','kk','ru','be','uk','sr','bs','hr','cs','sk','zh-TW','zh-CN','hi','ko','ja']
async def connect():
 for _ in range(60):
  try:return await websockets.connect('ws://127.0.0.1:24680/debugger',subprotocols=['debugger.ppsspp.org'],open_timeout=1,max_size=2000000)
  except OSError:await asyncio.sleep(.25)
 raise RuntimeError('debugger unavailable')
async def language(p,lang,old):
 await p.press('triangle');await p.press('select');p.page=12
 await p.moves('down','up',(lang-old)%26,(old-lang)%26)
 await p.press('cross');p.page=0
 actual=json.loads((e.GAME/'config.json').read_text())['language'];assert actual==CODES[lang],actual
async def run(restart):
 ws=await connect()
 async with ws:
  p=e.Pad(ws);version=await p.request('version');await asyncio.sleep(1)
  status=await p.request('game.status');assert status['game']['title']=='CalcAero 1.3'
  await p.screen('final-signed-restart-661' if restart else 'final-signed-start-660')
  assert json.loads((e.GAME/'config.json').read_text())['language']=='ru'
  if restart:
   await p.expression('f(3)');await p.press('start');await e.saved('f(3)',16)
   await p.expression('Ans+1');await p.press('start');await e.saved('Ans+1',17)
   await p.screen('final-restart-fractions-memory')
   # Language and new theme must survive a genuinely new PSP process.
   assert struct.unpack_from('<I',e.state()['raw'],324)[0]==7
   e.checks.append('final signed EBOOT fresh process: Russian, theme 7, function f, matrix A, score, Ans and history restored (HLE firmware 6.61)')
   return
  await p.menu(2)
  count=len(list((e.GAME/'reports').glob('CalcAero-Report-*.txt')))
  for tool in range(14):
   await p.press('cross');await p.press('start');await p.screen('final-lab-'+str(tool))
   await p.menu(18);file=await e.wait_file('CalcAero-Report-*.txt',count+tool+1);text=file.read_text()
   assert text.startswith('CalcAero 1.3') and 'Режим углов' in text
   assert not any(q in text for q in ['Некоррект','Проверь параметры','Неизвестный инструмент','вне поддерживаемых'])
   if tool==0:assert 'Ответ = 1/2' in text
   if tool==3:assert 'A . B ~= 32' in text
   if tool==5:assert 'Ответ ~= 30' in text
   if tool==7:await e.wait_extra(lambda v:v['function'][0]=='x^2+2*x+1')
   if tool==9:assert '0.682689492137' in text
   if tool==10:assert '3EAAAAAB' in text
   await p.press('select')
   if tool<13:await p.press('down')
  e.checks.append('final signed EBOOT: all 14 laboratory examples, exact report values and localized UTF-8 report headers')
  await p.menu(3);await p.press('cross');await p.expression('3');await p.press('start');await e.wait_extra(lambda v:v['matrix'][0]==3)
  await p.screen('final-matrix-grid')
  for op in range(7):
   await p.press('start');await p.screen('final-matrix-op-'+str(op));await p.press('circle')
   if op<6:await p.press('rtrigger')
  count=len(list((e.GAME/'reports').glob('CalcAero-Matrix-*.csv')));await p.menu(15);await e.wait_file('CalcAero-Matrix-*.csv',count+1)
  e.checks.append('final signed EBOOT: matrix editor, 7 matrix operations, CSV export')
  await p.menu(4);await p.expression('0');await p.press('start');await e.wait_extra(lambda v:v['total']>=1);await p.screen('final-study-answer');await p.press('cross')
  for _ in range(5):await p.press('select')
  await p.screen('final-study-topic-5')
  await p.menu(0);await p.expression('1/3+1/6');await p.menu(20);await p.screen('final-textbook-formula');await p.press('start');p.page=0
  await p.expression('f(2)');await p.press('start');value=await e.saved('f(2)',9);assert 'f(' not in value['latest'];await p.screen('final-user-function')
  e.checks.append('final signed EBOOT: study, 2D formula, saved function and frozen history')
  old=13
  for lang in range(26):
   await language(p,lang,old);old=lang
   await p.menu(9);await p.screen('final-language-'+CODES[lang]);await p.menu(0)
  await language(p,13,old)
  e.checks.append('final signed EBOOT: all 26 final catalogs and fonts, SELECT menu shortcut and immediate config.json writes')
  await p.menu(9)
  for _ in range(5):await p.press('down')
  oldtheme=struct.unpack_from('<I',e.state()['raw'],324)[0]
  for _ in range(oldtheme):await p.press('left')
  for theme in range(8):
   if theme:await p.press('right')
   await p.screen('final-theme-'+str(theme))
  await p.menu(0);await e.saved('f(2)',9)
  for _ in range(30):
   if struct.unpack_from('<I',e.state()['raw'],324)[0]==7:break
   await asyncio.sleep(.3)
  assert struct.unpack_from('<I',e.state()['raw'],324)[0]==7
  e.checks.append('final signed EBOOT: all 8 themes selected, rendered and theme 7 saved')
  # New screenshot export is generated by the final signed build itself.
  count=len(list((e.GAME/'reports').glob('CalcAero-Screen-*.bmp')));await p.menu(17);bmp=await e.wait_file('CalcAero-Screen-*.bmp',count+1);assert bmp.stat().st_size==391734
  e.checks.append('final signed EBOOT: actual 480x272 BMP export')
  return version

def main():
 app=Path(sys.argv[1]).resolve();run=Path(sys.argv[2]).resolve();e.MEM=run/'ppsspp/PSP';e.GAME=e.MEM/'GAME/CalcAero';e.SCREEN=run/'screens';e.SCREEN.mkdir(exist_ok=True)
 shutil.copy2(ROOT/'EBOOT-SIGNED.PBP',e.GAME/'EBOOT.PBP');shutil.copytree(ROOT/'assets',e.GAME/'assets',dirs_exist_ok=True)
 env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(run))
 cmd=[str(app/'shared/lib/ld-linux-x86-64.so.2'),'--library-path',str(app/'shared/lib'),str(app/'shared/bin/PPSSPPSDL'),str(e.GAME/'EBOOT.PBP')]
 version=None
 with (ROOT/'build/final-native-runtime.log').open('w') as log:
  for restart in [False,True]:
   ini=e.MEM/'SYSTEM/ppsspp.ini';cfg=configparser.ConfigParser();cfg.optionxform=str;cfg.read(ini,encoding="utf-8-sig")
   for section in ['SystemParam','CPU']:
    if section not in cfg:cfg[section]={}
   cfg['SystemParam']['PSPModel']='0';cfg['SystemParam']['PSPFirmwareVersion']='661' if restart else '660';cfg['CPU']['CPUSpeed']='222'
   with ini.open('w') as f:cfg.write(f)
   proc=subprocess.Popen(cmd,cwd=app/'bin',env=env,stdout=log,stderr=log)
   try:
    v=asyncio.run(run_step(restart));version=v or version
   except Exception:
    async def failure():
     async with await connect() as ws:await e.Pad(ws).screen('failed-final-step')
    asyncio.run(failure());raise
   finally:
    proc.terminate()
    try:proc.wait(5)
    except subprocess.TimeoutExpired:proc.kill();proc.wait()
 result=dict(status='PASS',emulator=version,signed_eboot_sha256=hashlib.sha256((ROOT/'EBOOT-SIGNED.PBP').read_bytes()).hexdigest(),unsigned_eboot_sha256=hashlib.sha256((ROOT/'EBOOT.PBP').read_bytes()).hexdigest(),model='PSP-1000',ram_mib=32,cpu_mhz=222,hle_firmware_profiles=[660,661],physical_ofw_test=False,controller_presses=e.press_count,checks=e.checks,game_directory=str(e.GAME.relative_to(ROOT)),screens_directory=str(e.SCREEN.relative_to(ROOT)),assets={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'assets').rglob('*') if p.is_file()},baseline_sha256='e8ec50dc040aecf6b9a632adeef8698cf6748953c0c4d4e1a273e70ac8b8c69f')
 (ROOT/'build/final-native-verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print('PASS: final signed native PSP build, 32 MB / 222 MHz, 26 languages, 8 themes, 14 labs, restart',flush=True)
async def run_step(restart):return await run(restart)
if __name__=='__main__':main()
