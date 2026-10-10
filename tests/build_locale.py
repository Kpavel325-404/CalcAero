"""Compile bounded UTF-8 catalogs and subset the chosen fonts for PSP memory.
JSON catalogs stay editable; font subsets retain shaping GSUB/GPOS and licenses.
"""
from pathlib import Path
import argparse,json,re,struct,zlib,shutil
from fontTools import subset
ROOT=Path(__file__).resolve().parent.parent
LANGS=['en','de','it','pl','es','nl','et','fi','sv','ro','el','ar','kk','ru','be','uk','sr','bs','hr','cs','sk','zh-TW','zh-CN','hi','ko','ja']
def fnv(s):
 h=2166136261
 for b in s.encode():h=((h^b)*16777619)&0xffffffff
 return h
PRINTF=re.compile(r'%(?:[-+ #0]*\d*(?:\.\d+)?(?:ll|[hlLzjt])?[diuoxXfFeEgGaAcspn%])')
def fmt(s):return [x for x in PRINTF.findall(s) if x!='%%']
def main():
 p=argparse.ArgumentParser();p.add_argument('fonts');args=p.parse_args();upstream=Path(args.fonts)
 source=json.loads((ROOT/'assets/locale/source.json').read_text())
 entries=sorted(source,key=fnv);assert len({fnv(s) for s in entries})==len(entries)
 report={'status':'PASS','languages':26,'messages':len(entries),'catalogs':{},'fonts':{}}
 base_chars=set(chr(i) for i in range(32,127))|set('ЁёЎўІіЇїЄєҚқҒғҢңӘәӨөҰұҮүҺһΕλληνικάČeskySlovenčinaRomânăEspañol')
 all_catalogs={}
 for lang in LANGS:
  data={s:s for s in entries} if lang=='en' else json.loads((ROOT/'assets/locale'/f'{lang}.json').read_text())
  assert set(data)==set(source),(lang,'missing messages')
  for key,text in data.items():
   assert text and '\0' not in text and (not fmt(key) or fmt(key)==fmt(text)),(lang,key,fmt(key),fmt(text))
  payload=bytearray(b'CALNG013'+struct.pack('<I',len(entries))+bytes(len(entries)*8))
  for i,key in enumerate(entries):
   struct.pack_into('<II',payload,12+i*8,fnv(key),len(payload));payload+=data[key].encode()+b'\0'
  payload+=struct.pack('<I',zlib.crc32(payload));assert len(payload)<262144
  (ROOT/'assets/locale'/f'{lang}.bin').write_bytes(payload)
  all_catalogs[lang]=data;report['catalogs'][lang]={'bytes':len(payload),'entries':len(data),'printf_signatures':'PASS'}
  if lang not in ['ar','hi','zh-TW','zh-CN','ko','ja']:base_chars.update(''.join(data.values()))
 for name,original,chars in [('base.ttf',Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'),base_chars)]+[(name,upstream/original,set(''.join(all_catalogs[lang].values()))) for name,original,lang in [('ar.ttf','NotoSansArabic-Regular.ttf','ar'),('hi.ttf','NotoSansDevanagari-Regular.ttf','hi'),('zh-TW.otf','NotoSansCJKtc-Regular.otf','zh-TW'),('zh-CN.otf','NotoSansCJKsc-Regular.otf','zh-CN'),('ko.otf','NotoSansCJKkr-Regular.otf','ko'),('ja.otf','NotoSansCJKjp-Regular.otf','ja')]]:
  # Mathematical input syntax remains ASCII. Also retain punctuation/symbols.
  chars.update(chr(i) for i in range(32,127))
  font=subset.load_font(str(original),subset.Options());cmap=font.getBestCmap()
  needed={ord(c) for c in chars if ord(c)>31};missing=needed-set(cmap)
  # Latin letters are rendered by the base face for Arabic/Devanagari.
  if name in ['ar.ttf','hi.ttf']:missing={c for c in missing if (0x600<=c<=0x8ff if name=='ar.ttf' else 0x900<=c<=0x97f)}
  assert not missing,(name,[hex(c) for c in sorted(missing)])
  opts=subset.Options();opts.layout_features=['*'];opts.notdef_glyph=True;opts.notdef_outline=True;opts.name_IDs=['*'];opts.name_languages=['*']
  cutter=subset.Subsetter(options=opts);cutter.populate(unicodes=needed&set(cmap));cutter.subset(font)
  # Avoid using reserved Noto font names for modified subset files.
  if name!='base.ttf':
   for record in font['name'].names:
    if record.nameID in [1,3,4,6,16]:record.string=('CalcAero '+name.split('.')[0]+' Subset').encode(record.getEncoding(),errors='replace')
  dest=ROOT/'assets/fonts'/name;font.save(dest);assert dest.stat().st_size<2097152
  report['fonts'][name]={'bytes':dest.stat().st_size,'unicode_characters':len(needed),'missing_required_glyphs':len(missing)}
 for filename in ['noto-cjk-OFL.txt','noto-arabic-OFL.txt','noto-devanagari-OFL.txt']:shutil.copy2(upstream/filename,ROOT/'LICENSES'/filename)
 (ROOT/'build/localization-verification.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report,indent=2))
if __name__=='__main__':main()
