"""Validate files created by test_extensions.c using independent standard parsers."""
import csv
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
directory=ROOT/(ROOT/'build/export-directory.txt').read_text().strip()/'reports'
checks=[]
for file in directory.glob('*.svg'):
    text=file.read_text();root=ET.fromstring(text)
    assert root.attrib['viewBox']=='0 0 480 272'
    assert not any(word in text for word in ['nan','inf'])
    checks.append('SVG XML, resolution and finite coordinates')
for file in directory.glob('*Graph*.csv'):
    with file.open(newline='') as stream: rows=list(csv.reader(stream))
    assert len(rows)==242 and len(rows[0])==4
    assert rows[121][1]=='' and rows[121][2]=='0'
    assert math.isclose(float(rows[1][1]),-.1,rel_tol=1e-15)
    checks.append('Graph CSV 241 samples, all curves and blank discontinuity')
for file in directory.glob('*History*.csv'):
    with file.open(newline='') as stream: rows=list(csv.reader(stream))
    assert rows[1][3]=='1/3+1/6' and rows[1][4]=='1/2'
    checks.append('History CSV exact rational and quoted expression')
assert len(checks)>=3
report=dict(status='PASS',checks=checks)
(ROOT/'build/export-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
