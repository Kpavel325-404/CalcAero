"""Package CalcAero 1.3 after exact release hashes and verification gates pass."""
from pathlib import Path
import hashlib, json, shutil, struct, sys, tempfile, zipfile
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parent.parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def load(name):return json.loads((ROOT/'build'/name).read_text())
native=load('final-native-verification.json');audit=load('psp-audit.json')
numeric=load('numeric-verification.json');extra=load('extensions-verification.json')
locale=load('localization-verification.json');exports=load('export-verification.json');ui=load('ui-qa.json')
for r in [native,audit,numeric,extra,locale,exports]:assert r['status']=='PASS'
signed=(ROOT/'EBOOT-SIGNED.PBP').read_bytes();unsigned=(ROOT/'EBOOT.PBP').read_bytes()
assert sha(signed)==native['signed_eboot_sha256']==audit['signed_eboot_sha256']
assert sha(unsigned)==native['unsigned_eboot_sha256']==audit['unsigned_eboot_sha256']
off=struct.unpack_from('<8I',unsigned,8)
assert sha(unsigned[off[6]:off[7]])==native['unsigned_data_psp_sha256']==native['completed_prefix']['unsigned_data_psp_sha256']
game=ROOT/native['game_directory'];assert (game/'EBOOT.PBP').read_bytes()==signed
assert native['ram_mib']==32 and native['cpu_mhz']==222 and native['hle_firmware_profiles']==[660,661]
assert not native['physical_ofw_test'] and audit['sfo']['MEMSIZE']==0
assert locale['languages']==26 and all(v['entries']==locale['messages'] for v in locale['catalogs'].values())
assert all(v['missing_required_glyphs']==0 for v in locale['fonts'].values())
assert ui['geometry_in_bounds'] and ui['antialias_font'] and ui['max_vertices']<ui['capacity']
for name,h in native['assets'].items():assert sha((ROOT/name).read_bytes())==h and sha((game/name).read_bytes())==h
host=(ROOT/'build/final-host-all.log').read_text()
for needle in ['CALCAERO SELFTEST 62/62','30000 controller actions','30000 malformed expressions','4000 malformed extension expressions','5000 malformed configs','real CalcLab KP 1.0']:assert needle in host
for name in ['final-host-all.log','final-ui.log','finish-native-controller.log','final-build.log']:
    text=(ROOT/'build'/name).read_text()
    assert not any(s in text for s in ['AddressSanitizer:','runtime error:','AssertionError','Traceback','SELFTEST FAIL','error:'])
assert 'PASS: UI 1694 states' in (ROOT/'build/final-ui.log').read_text()
assert 'PASS: release signed EBOOT' in (ROOT/'build/finish-native-controller.log').read_text()
stage=Path(tempfile.mkdtemp(prefix='release-v13-',dir=ROOT/'build'))
def copy(src,target):
    dest=stage/target;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
def write(target,text):
    dest=stage/target;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(text,encoding='utf-8')
def png(src,target):
    dest=stage/target;dest.parent.mkdir(parents=True,exist_ok=True);Image.open(src).save(dest)
copy(ROOT/'EBOOT-SIGNED.PBP','PSP/GAME/CalcAero/EBOOT.PBP');copy(ROOT/'EBOOT.PBP','UNSIGNED/EBOOT.PBP')
write('UNSIGNED/README.txt','Unsigned User Mode build for rebuilding / ebootsigner / EasyPBPRX input.\nNormal installation uses the already signed PSP/GAME/CalcAero/EBOOT.PBP.\n')
for folder in ['assets/fonts','assets/locale']:
    for f in (ROOT/folder).iterdir():
        if f.is_file() and f.suffix in ['.bin','.ttf','.otf']:copy(f,'PSP/GAME/CalcAero/'+str(f.relative_to(ROOT)))
docs=['README-RU.md','README-RU.txt','FEATURES.md','BUILD.md','OFW.md','LOCALIZATION.md','TESTING-PSP.md','THIRD-PARTY.md','LICENSE']
for name in docs:copy(ROOT/name,name);copy(ROOT/name,'SOURCE/'+name)
for name in ['Makefile','ICON0.png','PIC1.png']:copy(ROOT/name,'SOURCE/'+name)
for folder in ['src','tests','assets','LICENSES','vendor']:
    for f in sorted((ROOT/folder).rglob('*')):
        if not f.is_file() or '__pycache__' in f.parts or f.suffix=='.pyc':continue
        if f.name.endswith(('.o','.elf','.prx','.so')):continue
        copy(f,'SOURCE/'+str(f.relative_to(ROOT)))
for f in (ROOT/'LICENSES').iterdir():
    if f.is_file():copy(f,'LICENSES/'+f.name)
reports=['numeric-verification.json','extensions-verification.json','ui-qa.json','localization-verification.json','export-verification.json','psp-audit.json','final-native-verification.json','previous-native-payload.json','functional-baseline-hash.json','final-host-all.log','final-ui.log','final-build.log','psp-audit.log','finish-native-controller.log','finish-native-runtime.log','previous-native-controller.log']
for name in reports:copy(ROOT/'build'/name,'VERIFY/'+name)
for f in sorted((ROOT/native['screens_directory']).glob('*.png')):
    assert not f.name.startswith('failed');copy(f,'VERIFY/PSP-1.3-RELEASE/'+f.name)
prefix=native['completed_prefix']
for f in sorted((ROOT/prefix['screens_directory']).glob('final-*.png')):
    if any(f.name.startswith('final-'+x) for x in ['lab-','matrix-','study-','textbook-','user-function']):copy(f,'VERIFY/PSP-1.3-CODE/'+f.name)
for f in sorted((game/'reports').glob('*')):
    if f.suffix in ['.txt','.csv','.svg','.bmp']:copy(f,'VERIFY/NATIVE-EXPORTS/'+f.name)
for name in ['config.json','state.dat','aero.dat']:copy(game/name,'VERIFY/NATIVE-SAVED-STATE/'+name)
for name in ['three-graphs','after-keyboard','direct-graph-analysis']:
    for suffix in ['','-alternate']:
        f=ROOT/prefix['screens_directory']/(name+suffix+'.png')
        if f.is_file():copy(f,'VERIFY/EARLIER-1.3-BASELINE/'+f.name)
write('VERIFY/EARLIER-1.3-BASELINE/README.txt','Earlier unsigned functional baseline: e8ec50dc040aecf6b9a632adeef8698cf6748953c0c4d4e1a273e70ac8b8c69f.\nOSK / three-graph screenshots precede SELECT shortcut, TXT localization and reduced decorative geometry.\nThese are not screenshots of the final signed EBOOT.\n')
uploads=Path(sys.argv[1]).resolve()
for i in range(1,9):
    f=uploads/f'CalcAero-Screen-{i:04d}.bmp'
    with Image.open(f) as image:assert image.size==(480,272)
    assert f.stat().st_size==391734;copy(f,'VERIFY/USER-1.2-BMP/'+f.name)
write('VERIFY/USER-1.2-BMP/README.md','# Снимки пользователя: версия 1.2\n\nВосемь исходных BMP 480×272 прочитаны и сохранены без изменений. Видны Aero Green, калькулятор, справка, страницы клавиатуры и режим программиста. Текст и кнопки читаются; основная геометрия помещается на экране. По статичным BMP нельзя проверить мерцание, частоту CPU, батарею или длительную стабильность. Пользователь отдельно сообщил о стабильности 1.2. Эти снимки не подтверждают физический запуск 1.3 или работу на OFW.\n')
themes=['Aero Blue','Aero Green','Vista Night','Windows XP Luna','Windows Vista Aero','Vaporwave','Night Retro','WebCore Math']
def theme_image(i):return ROOT/'build'/('calculator.ppm' if i==0 else f'theme-{i}.ppm')
for i,name in enumerate(themes):png(theme_image(i),f'PREVIEW/themes/{i}-{name.replace(" ","-")}.png')
for code in locale['catalogs']:png(ROOT/'build'/f'locale-{code}-page-6.ppm',f'PREVIEW/languages/{code}.png')
for name in ['calculator','three-graphs-0','formula-0','lab-0-0-result','lab-0-12-result','matrix-0-4','study-0-0-correct']:png(ROOT/'build'/(name+'.ppm'),'PREVIEW/features/'+name+'.png')
canvas=Image.new('RGB',(1004,1340),(226,239,245));draw=ImageDraw.Draw(canvas)
font='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf';title=ImageFont.truetype(font,26);label=ImageFont.truetype(font,16)
draw.text((20,12),'CalcAero 1.3 / PSP',font=title,fill=(18,70,92));draw.text((20,46),'26 languages  •  8 themes  •  32 MB / 222 MHz',font=label,fill=(42,90,112))
for i,name in enumerate(themes):
    x=16+(i%2)*492;y=82+(i//2)*312
    draw.text((x,y),name,font=label,fill=(20,67,91));canvas.paste(Image.open(theme_image(i)),(x,y+22))
canvas.save(stage/'PREVIEW/CalcAero-1.3-Themes.png')
write('PREVIEW/README.txt','PNGs produced by the actual CalcAero C renderer on the host.\nNative signed-PSP screenshots: VERIFY/PSP-1.3-RELEASE.\nEach native screenshot has both PSP VRAM buffers; the alternate image can be the latest displayed frame.\n')
write('VERIFY/VERIFY-RU.txt',f'''CalcAero 1.3 — результаты проверки релиза

Математика: {numeric['checks']} + {extra['checks']} = {numeric['checks']+extra['checks']} проверок, PASS.
Эталоны: Fraction, SymPy, NumPy, mpmath (100 цифр).
ASan/UBSan: 30000 действий контроллера, 30000 старых и 4000 новых повреждённых
выражений; 256 + 256 CRC-повреждений, резерв/откат I/O, реальные данные 1.0.
Конфигурация: 5000 повреждённых JSON, отсутствие/пустой язык -> English,
все 26 языков, немедленная запись, ошибка записи -> прежний язык. Selftest: 62/62.
UI: {ui['states']} состояний 480x272; максимум {ui['max_vertices']}/{ui['capacity']} вершин, PASS.
Локализация: 26 каталогов по {locale['messages']} строк; CRC/printf/глифы: PASS.
Не все переводы вычитаны носителями; подробности в LOCALIZATION.md.

PPSSPP 1.20.4: PSP-1000, RAM 32 MiB, CPU 222 MHz, HLE-профили 6.60/6.61.
Финальный подписанный EBOOT: {native['controller_presses']} нажатий; первый English, все языки,
восемь тем, BMP/TXT, новый процесс, сохранённые Russian/тема 7/функция/матрица/
учёба/история/Ans. 14 лабораторных примеров, 7 операций матриц, учебные задания
и 2D проверены ранее с тем же программным DATA.PSP. Прерванный процесс и отдельные
SHA описаны в JSON. После него обновлены только XMB-картинки и казахские ресурсы;
финальные ресурсы проверены заново. PSP-1.3-RELEASE содержит обе VRAM-страницы:
одна может быть предыдущим кадром. PSP-1.3-CODE — завершённые сценарии того же
кода до обновления XMB-картинок. EARLIER-1.3-BASELINE — более ранняя сборка;
USER-1.2-BMP — пользовательские BMP 1.2.

ELF: User Mode, USER|VFPU, user imports, MEMSIZE=0; ebootsigner -> ~PSP.
Статическая память + зарезервированные heap/stack: {audit['reserved_image_heap_stack_bytes']} байт;
это бюджет образа, а не замер общего пика с системной OSK. CPU не повышается.
Физический запуск 1.3 на OFW и отдельные аппаратные испытания всех моделей
ещё не выполнены. PASS не доказывает отсутствие любых ошибок.
ZIP не содержит устанавливаемых чужих сохранений.

Подписанный EBOOT SHA-256: {sha(signed)}
Неподписанный EBOOT SHA-256: {sha(unsigned)}
''')
installed=[str(p.relative_to(stage)) for p in (stage/'PSP').rglob('*') if p.is_file()]
assert len([p for p in installed if p.endswith('.bin')])==26
assert len([p for p in installed if p.endswith(('.ttf','.otf'))])==7
assert not any(Path(p).name in ['config.json','state.dat','aero.dat'] for p in installed)
write('VERIFY/package-verification.json',json.dumps(dict(status='PASS',installed_files=len(installed),installed_bytes=sum((stage/p).stat().st_size for p in installed),catalogs=26,font_files=7,foreign_installed_config=False,foreign_installed_saved_state=False,signed_eboot_sha256=sha(signed)),indent=2)+'\n')
manifest={str(p.relative_to(stage)):sha(p.read_bytes()) for p in sorted(stage.rglob('*')) if p.is_file()}
write('SHA256SUMS.txt',''.join(f'{h}  {p}\n' for p,h in manifest.items()))
dist=ROOT/'dist';dist.mkdir(exist_ok=True);archive=dist/'PSP-CalcAero-1.3.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(stage.rglob('*')):
        if p.is_file():z.write(p,str(p.relative_to(stage)))
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None and len(z.namelist())==len(set(z.namelist()))==len(manifest)+1
    for p,h in manifest.items():assert sha(z.read(p))==h,p
    assert z.read('PSP/GAME/CalcAero/EBOOT.PBP')==signed
    assert not any(p.startswith('/') or '..' in Path(p).parts for p in z.namelist())
    assert not any(p.endswith(('.o','.elf','.prx','.so','.pyc')) for p in z.namelist())
result=dict(status='PASS',archive=str(archive),bytes=archive.stat().st_size,members=len(manifest)+1,archive_sha256=sha(archive.read_bytes()),signed_eboot_sha256=sha(signed),installed_files=len(installed))
(ROOT/'build/release-verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
