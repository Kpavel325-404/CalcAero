"""Development-only Kazakh review pass with NLLB, preserving syntax and reviewed labels.
No translation model or runtime is included in the PSP release.
"""
from pathlib import Path
import ast, json, re, sys
from translate_catalog import ROOT, segments

def reviewed_keys(path):
    tree = ast.parse(path.read_text())
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'KEYS' for t in node.targets):
            if isinstance(node.value, ast.Call):
                return ast.literal_eval(node.value.func.value).split('|')
            return ast.literal_eval(node.value)
    raise ValueError('Missing review key list')

def main():
    import ctranslate2, sentencepiece
    model = Path(sys.argv[1])
    sp = sentencepiece.SentencePieceProcessor(model_file=str(model/'sentencepiece.bpe.model'))
    tr = ctranslate2.Translator(str(model), device='cpu', compute_type='int8', intra_threads=4)
    path = ROOT/'assets/locale/kk.json'
    old = json.loads(path.read_text())
    keys = set(reviewed_keys(ROOT/'tests/review_terms.py') + reviewed_keys(ROOT/'tests/review_settings.py'))
    keys.update(['Three graphs / table', 'CalcAero Laboratory', 'Matrix editor A / B', 'Textbook formula: full screen'])
    source = json.loads((ROOT/'assets/locale/source.json').read_text())
    parts = sorted({s.strip() for text in source if text not in keys for fixed,s in segments(text)
                    if not fixed and re.search('[a-zA-Z]', s)})
    translated = {}
    for i in range(0, len(parts), 32):
        batch = parts[i:i+32]
        results = tr.translate_batch([['eng_Latn'] + sp.encode(s, out_type=str) + ['</s>'] for s in batch],
            target_prefix=[['kaz_Cyrl']]*len(batch), beam_size=3, max_decoding_length=160,
            repetition_penalty=1.05)
        for text,result in zip(batch,results):
            translated[text] = sp.decode(result.hypotheses[0][1:]).strip()
        print(i, '/', len(parts), flush=True)
    out = {}
    for text in source:
        if text in keys:
            out[text] = old[text]
            continue
        value = ''
        for fixed,s in segments(text):
            if fixed or s.strip() not in translated:
                value += s
            else:
                value += s[:len(s)-len(s.lstrip())] + translated[s.strip()] + s[len(s.rstrip()):]
        out[text] = value
    out.update({
        'Memory / constants': 'Жад / тұрақтылар',
        'Memory and history: autosaved to card.': 'Жад пен тарих картаға автоматты түрде сақталады.',
        "Memory A..F: uppercase; e: Euler's number.": 'Жад A..F: бас әріптер; e: Эйлер саны.',
        'Fair coin: probability of %d heads in a row?': 'Тиынның %d рет қатарынан елтаңба жағымен түсу ықтималдығы?',
        'DIV/MOD and SHR use unsigned integers.': 'DIV/MOD және SHR таңбасыз бүтін сандарды қолданады.',
        'DEC S shows the same bits as a signed integer.': 'DEC S осы биттерді таңбалы бүтін сан ретінде көрсетеді.',
        'Check expression and parentheses': 'Өрнек пен жақшаларды тексеріңіз',
        'Intersection: x~=%.12g, y~=%.9g': 'Қиылысу: x~=%.12g, y~=%.9g',
        'Root: x ~= %.14g': 'Түбір: x ~= %.14g',
        'Roots: %d; intersections: %d; extrema: %d': 'Түбірлер: %d; қиылысулар: %d; экстремумдар: %d',
        '129 samples; discontinuities may be missed': '129 нүкте; үзілістер анықталмай қалуы мүмкін',
        'Enable complex mode': 'Комплекс сандар режимін қосыңыз',
        'Input/calculation limit reached': 'Енгізу немесе есептеу шегіне жетті',
        'X: enter  O: delete  []: cursor  START: =': 'X: енгізу  O: өшіру  []: курсор  START: =',
        'L/R: keys  SELECT: DEG/RAD/GRAD  /\\: menu': 'L/R: пернелер  SELECT: DEG/RAD/GRAD  /\\: мәзір',
        '[]: enter f(x)  START: plot': '[]: f(x) енгізу  START: график',
        'WINDOW: arrows move; L/R: zoom': 'ТЕРЕЗЕ: бағыттау пернелері; L/R: масштаб',
        'Choose your language / 28': 'Тілді таңдаңыз / 28',
        'Language is saved in config.json': 'Тіл config.json файлына сақталады',
        'X: select and save  O: calculator': 'X: таңдау және сақтау  O: калькулятор',
        'Arrows: select  X/START: save language': 'Бағыттар: таңдау  X/START: тілді сақтау',
        'Language save failed: check assets and memory card': 'Тіл сақталмады: assets пен жад картасын тексеріңіз'})
    path.write_text(json.dumps(out, ensure_ascii=False, indent=2)+'\n')
    print('PASS: Kazakh review pass, protected syntax and reviewed labels preserved', flush=True)

if __name__ == '__main__': main()
