"""Finish native release verification after an interrupted controller process.

The previous run completed all labs, matrices, study and function checks.
Its exact program payload is compared with the release; only XMB pictures changed.
The release itself is booted again with no config, all final locales, all themes
and then a genuinely new process with the saved Russian configuration.
Only controller events and read-only debugger inspection are used.
"""
import asyncio, configparser, hashlib, json, os, shutil, struct, subprocess, sys, tempfile
from pathlib import Path
import emulator_verify as e
from final_native import CODES, connect, language

ROOT=e.ROOT
sha=lambda b:hashlib.sha256(b).hexdigest()

def payload(path):
    b=path.read_bytes();off=struct.unpack_from('<8I',b,8)
    return b[off[6]:off[7]]

def verify_prefix(old):
    meta=json.loads((ROOT/'build/previous-native-payload.json').read_text())
    assert sha(payload(ROOT/'EBOOT.PBP'))==meta['unsigned']['data_psp_sha256']
    assert sha((old/'ppsspp/PSP/GAME/CalcAero/EBOOT.PBP').read_bytes())==meta['signed']['pbp_sha256']
    trace=(ROOT/'build/previous-native-controller.log').read_text()
    assert 'Traceback' not in trace and 'screen final-language-ro' in trace
    files=sorted((old/'ppsspp/PSP/GAME/CalcAero/reports').glob('CalcAero-Report-*.txt'))[-14:]
    assert len(files)==14
    for tool,file in enumerate(files):
        text=file.read_text()
        assert text.startswith('CalcAero 1.3') and 'Режим углов' in text
        assert not any(x in text for x in ['Некоррект','Проверь параметры','Неизвестный инструмент','вне поддерживаемых'])
        assert f'screen final-lab-{tool}\n' in trace
        if tool==0:assert 'Ответ = 1/2' in text
        if tool==3:assert 'A . B ~= 32' in text
        if tool==5:assert 'Ответ ~= 30' in text
        if tool==9:assert '0.682689492137' in text
        if tool==10:assert '3EAAAAAB' in text
    for name in ['final-matrix-grid']+[f'final-matrix-op-{i}' for i in range(7)]+['final-study-answer','final-study-topic-5','final-textbook-formula','final-user-function']:
        assert f'screen {name}\n' in trace
        assert (old/'screens'/(name+'.png')).is_file()
    game=old/'ppsspp/PSP/GAME/CalcAero'
    e.GAME=game
    s=e.state();a=e.extra_state()
    assert s['expr']=='f(2)' and s['ans'][0]==9
    assert a['function'][0]=='x^2+2*x+1' and a['matrix'][0]==3 and a['topic']==5 and a['total']>=1
    return dict(status='PASS',signed_eboot_sha256=meta['signed']['pbp_sha256'],
        unsigned_data_psp_sha256=meta['unsigned']['data_psp_sha256'],
        note='Controller run interrupted after Romanian/Greek selection; completed prefix revalidated from exports, CRC state and trace. Release has identical unsigned DATA.PSP, regenerated XMB pictures and updated Kazakh assets.',
        game_directory=str(game.relative_to(ROOT)),screens_directory=str((old/'screens').relative_to(ROOT)),
        checks=['14 native laboratory examples and localized numeric TXT reports',
            'matrix editor, seven operations and CSV export',
            'study grading, topic switching, textbook notation, saved function and frozen history'])

async def run(restart):
    async with await connect() as ws:
        p=e.Pad(ws);version=await p.request('version');await asyncio.sleep(1)
        assert (await p.request('game.status'))['game']['title']=='CalcAero 1.3'
        if restart:
            assert json.loads((e.GAME/'config.json').read_text())['language']=='ru'
            assert struct.unpack_from('<I',e.state()['raw'],324)[0]==7
            await p.expression('f(3)');await p.press('start');await e.saved('f(3)',16)
            await p.expression('Ans+1');await p.press('start');await e.saved('Ans+1',17)
            await p.screen('final-restart-661-memory')
            e.checks.append('release signed EBOOT: fresh process restores Russian, theme 7, function, matrix, study, history and Ans; f(3)=16, Ans+1=17 (HLE 6.61)')
            return version
        assert not (e.GAME/'config.json').exists()
        await p.screen('final-first-start-English-26')
        await p.press('cross');assert json.loads((e.GAME/'config.json').read_text())['language']=='en'
        e.checks.append('release signed EBOOT: no config defaults to English and opens all 26 choices; first selection saves immediately')
        old=0
        for lang in range(26):
            if lang:await language(p,lang,old)
            old=lang
            assert json.loads((e.GAME/'config.json').read_text())['language']==CODES[lang]
            await p.screen('final-language-'+CODES[lang])
        e.checks.append('release signed EBOOT: all 26 final languages and fonts, SELECT shortcut, immediate JSON persistence')
        await language(p,13,old)
        await p.menu(9)
        for _ in range(5):await p.press('down')
        oldtheme=struct.unpack_from('<I',e.state()['raw'],324)[0]
        for _ in range(oldtheme):await p.press('left')
        for theme in range(8):
            if theme:await p.press('right')
            await p.screen('final-theme-'+str(theme))
        await p.menu(0)
        for _ in range(60):
            if struct.unpack_from('<I',e.state()['raw'],324)[0]==7:break
            await asyncio.sleep(.2)
        assert struct.unpack_from('<I',e.state()['raw'],324)[0]==7
        e.checks.append('release signed EBOOT: all 8 themes render and theme 7 is saved')
        count=len(list((e.GAME/'reports').glob('CalcAero-Screen-*.bmp')))
        await p.menu(17);bmp=await e.wait_file('CalcAero-Screen-*.bmp',count+1)
        assert bmp.stat().st_size==391734
        await p.menu(2);await p.press('cross');await p.press('start')
        count=len(list((e.GAME/'reports').glob('CalcAero-Report-*.txt')))
        await p.menu(18);text=(await e.wait_file('CalcAero-Report-*.txt',count+1)).read_text()
        assert text.startswith('CalcAero 1.3') and 'Режим углов' in text and 'Ответ = 1/2' in text
        await p.screen('final-native-report')
        e.checks.append('release signed EBOOT: native 480x272 BMP and localized exact-fraction TXT report')
        return version

def main():
    app=Path(sys.argv[1]).resolve();old=Path(sys.argv[2]).resolve();prefix=verify_prefix(old)
    run_dir=Path(tempfile.mkdtemp(prefix='emulator-final-',dir=ROOT/'build'))
    shutil.copytree(old/'ppsspp',run_dir/'ppsspp')
    e.MEM=run_dir/'ppsspp/PSP';e.GAME=e.MEM/'GAME/CalcAero';e.SCREEN=run_dir/'screens';e.SCREEN.mkdir()
    for name in ['config.json','config.json.bak','config.json.tmp']:(e.GAME/name).unlink(missing_ok=True)
    shutil.copy2(ROOT/'EBOOT-SIGNED.PBP',e.GAME/'EBOOT.PBP')
    shutil.copytree(ROOT/'assets',e.GAME/'assets',dirs_exist_ok=True)
    env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(run_dir))
    cmd=[str(app/'shared/lib/ld-linux-x86-64.so.2'),'--library-path',str(app/'shared/lib'),str(app/'shared/bin/PPSSPPSDL'),str(e.GAME/'EBOOT.PBP')]
    with (ROOT/'build/finish-native-runtime.log').open('w') as log:
        for restart in [False,True]:
            ini=e.MEM/'SYSTEM/ppsspp.ini';cfg=configparser.ConfigParser();cfg.optionxform=str;cfg.read(ini,encoding='utf-8-sig')
            cfg['SystemParam']['PSPModel']='0';cfg['SystemParam']['PSPFirmwareVersion']='661' if restart else '660'
            cfg['CPU']['CPUSpeed']='222'
            with ini.open('w') as f:cfg.write(f)
            proc=subprocess.Popen(cmd,cwd=app/'bin',env=env,stdout=log,stderr=log)
            try:version=asyncio.run(run(restart))
            except Exception:
                async def capture():
                    async with await connect() as ws:await e.Pad(ws).screen('failed-finish-native')
                try:asyncio.run(capture())
                except Exception:pass
                raise
            finally:
                proc.terminate()
                try:proc.wait(5)
                except subprocess.TimeoutExpired:proc.kill();proc.wait()
    hashes={str(p.relative_to(ROOT)):sha(p.read_bytes()) for p in (ROOT/'assets').rglob('*') if p.is_file()}
    for name,h in hashes.items():assert sha((e.GAME/name).read_bytes())==h
    result=dict(status='PASS',emulator=version,signed_eboot_sha256=sha((ROOT/'EBOOT-SIGNED.PBP').read_bytes()),
        unsigned_eboot_sha256=sha((ROOT/'EBOOT.PBP').read_bytes()),unsigned_data_psp_sha256=sha(payload(ROOT/'EBOOT.PBP')),
        model='PSP-1000',ram_mib=32,cpu_mhz=222,hle_firmware_profiles=[660,661],physical_ofw_test=False,
        controller_presses=e.press_count,checks=e.checks,completed_prefix=prefix,
        game_directory=str(e.GAME.relative_to(ROOT)),screens_directory=str(e.SCREEN.relative_to(ROOT)),assets=hashes)
    (ROOT/'build/final-native-verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print('PASS: release signed EBOOT, default English, 26 final languages, 8 themes, 32 MB / 222 MHz and restart',flush=True)

if __name__=='__main__':main()
