"""Development-only bulk translation. No model or network is used on PSP.
English source is human edited. Printf, keys, units and formula syntax are protected.
Run with CTranslate2 and SentencePiece; see LOCALIZATION.md for review status.
"""
from pathlib import Path
import json,re,time,argparse
ROOT=Path(__file__).resolve().parent.parent
LANGS=['de','it','pl','es','nl','et','fi','sv','ro','el','ar','kk','be','uk','sr','bs','hr','cs','sk','zh-CN','hi','ko','ja']
# Immutable application syntax is never submitted for translation.
TOKEN=re.compile(r'%(?:[-+ #0]*\d*(?:\.\d+)?(?:ll|[hlLzjt])?[diuoxXfFeEgGaAcsp%])|\n|\b(?:[A-Z][A-Z0-9_/.-]*|(?:sin|cos|tan|asin|atan2|sqrt|cbrt|root|ln|log|log2|polar|arg|re|im|conj|abs|mod|fact|ncr|npr|int64|double|float32|epsilon|NaN|state\.dat(?:\.bak)?|aero\.dat(?:\.bak)?|config\.json|reports/|linear/exp/log/power|kB|KiB|kb|hplanck|qe|na|g0|c0))\b|(?:[a-zA-Z\d][\w]*(?:[|^=*+<>;/()\[\].-][\w/()^=*+<>;|.-]+)+)|(?:\[\]|/\\)|\b\d+(?:\.\d+)?\b')
def fragments(text):return TOKEN.split(text)
def segments(text):
 out=[];at=0
 for m in TOKEN.finditer(text):
  out.append((0,text[at:m.start()]));out.append((1,m.group()));at=m.end()
 out.append((0,text[at:]));return out

def main():
 import ctranslate2,sentencepiece
 p=argparse.ArgumentParser();p.add_argument('model');a=p.parse_args()
 model=Path(a.model);sp=sentencepiece.SentencePieceProcessor(model_file=str(model/'sentencepiece.bpe.model'))
 tr=ctranslate2.Translator(str(model),device='cpu',compute_type='int8',inter_threads=1,intra_threads=4)
 source=json.loads((ROOT/'assets/locale/source.json').read_text())
 parts={s.strip() for text in source for fixed,s in segments(text) if not fixed and re.search('[a-zA-Z]',s)}
 parts=sorted(parts,key=lambda s:(len(sp.encode(s)),s))
 print('source',len(source),'fragments',len(parts),flush=True)
 for lang in LANGS:
  dest=ROOT/'assets/locale'/f'{lang}.json'
  if dest.exists():continue
  start=time.monotonic();target='zh' if lang=='zh-CN' else lang
  translated={}
  for i in range(0,len(parts),48):
   batch=parts[i:i+48]
   out=tr.translate_batch([['__en__']+sp.encode(s,out_type=str)+['</s>'] for s in batch],target_prefix=[['__'+target+'__']]*len(batch),beam_size=2,max_decoding_length=160,repetition_penalty=1.05)
   for s,r in zip(batch,out):translated[s]=sp.decode(r.hypotheses[0][1:]).strip()
   if i%192==0:print(lang,i,'/',len(parts),flush=True)
  catalog={}
  for text in source:
   out=''
   for fixed,s in segments(text):
    if fixed or s.strip() not in translated:out+=s;continue
    core=s.strip();left=s[:len(s)-len(s.lstrip())];right=s[len(s.rstrip()):]
    out+=left+translated[core]+right
   catalog[text]=out
  dest.write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n')
  print('DONE',lang,round(time.monotonic()-start,1),flush=True)
if __name__=='__main__':main()
