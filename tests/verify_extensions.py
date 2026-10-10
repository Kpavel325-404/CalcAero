"""Independent CalcAero references: Fraction, SymPy, NumPy and mpmath.
Run after compiling build/libaero.so. All randomness is reproducible.
"""
import ctypes as C
import json
import math
import random
import re
import struct
from fractions import Fraction
from pathlib import Path
import mpmath as mp
import numpy as np
import sympy as sp

ROOT = Path(__file__).resolve().parents[1]
class Value(C.Structure):
    _fields_ = [('re', C.c_double), ('im', C.c_double), ('num', C.c_int64), ('den', C.c_int64), ('exact', C.c_int)]
class Context(C.Structure):
    _fields_ = [('angle', C.c_int), ('complex_mode', C.c_int), ('ans', Value), ('mem', Value * 6), ('x', Value)]
class Error(C.Structure):
    _fields_ = [('code', C.c_int), ('pos', C.c_int)]
class Result(C.Structure):
    _fields_ = [('line', (C.c_char * 160) * 48), ('count', C.c_int), ('ok', C.c_int), ('approx', C.c_int), ('value', C.c_double), ('error', C.c_double)]
class State(C.Structure):
    _fields_ = [('function', (C.c_char * 512) * 3), ('graph', (C.c_char * 512) * 2), ('pretty', C.c_int), ('matrix_n', C.c_int), ('matrix', (C.c_double * 16) * 2), ('seed', C.c_uint32), ('topic', C.c_uint32), ('total', C.c_uint32), ('correct', C.c_uint32)]
lib = C.CDLL(str(ROOT / 'build/libaero.so'))
lib.cl_context_init.argtypes = [C.POINTER(Context)]
lib.aero_defaults.argtypes = [C.POINTER(State)]
lib.cl_calculate.argtypes = [C.c_char_p, C.POINTER(Context), C.POINTER(Value), C.POINTER(Error)]
lib.aero_calculate.argtypes = [C.c_char_p, C.POINTER(State), C.POINTER(Context), C.POINTER(Value), C.POINTER(Error)]
lib.aero_lab_run.argtypes = [C.c_int, C.c_char_p, C.POINTER(Context), C.POINTER(State), C.POINTER(Result)]
lib.aero_matrix_run.argtypes = [C.POINTER(State), C.c_int, C.POINTER(Result)]
lib.aero_symbolic.argtypes = [C.c_char_p, C.POINTER(Context), C.c_int, C.c_char_p, C.c_char_p, C.c_size_t]
lib.aero_dimensions.argtypes = [C.c_char_p, C.POINTER(C.c_double), C.POINTER(C.c_int), C.c_char_p, C.c_size_t, C.c_char_p, C.c_size_t]
lib.aero_binomial_pmf.argtypes = lib.aero_binomial_cdf.argtypes = [C.c_int, C.c_int, C.c_double]
lib.aero_binomial_pmf.restype = lib.aero_binomial_cdf.restype = C.c_double
lib.aero_normal_interval.argtypes = [C.c_double] * 4
lib.aero_normal_interval.restype = C.c_double
lib.aero_define.argtypes = [C.POINTER(State), C.c_int, C.c_char_p, C.POINTER(Error)]
lib.aero_study_question.argtypes = [C.POINTER(State), C.c_char_p, C.POINTER(Value), C.c_char_p]
ctx = Context(); lib.cl_context_init(C.byref(ctx))
s = State(); lib.aero_defaults(C.byref(s))
rng = random.Random(20261009)
mp.mp.dps = 100
checks = 0
groups = {}
def checked(group):
    global checks
    checks += 1; groups[group] = groups.get(group, 0) + 1

def lines(r): return [bytes(r.line[i]).split(b'\0', 1)[0].decode() for i in range(r.count)]
def lab(t, text, expected=None, valid=True, tol=2e-11):
    r = Result(); ok = lib.aero_lab_run(t, text.encode(), C.byref(ctx), C.byref(s), C.byref(r))
    assert bool(ok) == valid and bool(r.ok) == valid, (t, text, ok, lines(r))
    assert 0 < r.count <= 48, (t, text, r.count)
    if expected is not None: assert math.isclose(r.value, float(expected), rel_tol=tol, abs_tol=tol), (text, r.value, expected, lines(r))
    checked('laboratory'); return r

def calculate(text, x=Fraction(2), exact=None):
    ctx.x = Value(float(x), 0, x.numerator, x.denominator, 1) if isinstance(x, Fraction) else Value(float(x), 0, 0, 1, 0)
    v = Value(); e = Error()
    assert lib.aero_calculate(text.encode(), C.byref(s), C.byref(ctx), C.byref(v), C.byref(e)), (text, e.code, e.pos)
    if exact is not None: assert v.exact and Fraction(v.num, v.den) == exact, (text, v.num, v.den, exact)
    checked('calculation'); return v.re

# Saved functions: substitution, nesting, dependencies, recursion and old memory F.
e = Error()
assert lib.aero_define(C.byref(s), 0, b'x^2+1', C.byref(e))
assert lib.aero_define(C.byref(s), 1, b'f(x+1)/2', C.byref(e))
assert lib.aero_define(C.byref(s), 2, b'g(x)+f(2*x)', C.byref(e))
calculate('f(3)', exact=Fraction(10)); calculate('g(2)', exact=Fraction(5)); calculate('h(2)', exact=Fraction(22))
calculate('f(f(2))', exact=Fraction(26)); calculate('g(f(x))', Fraction(1), Fraction(5))
ctx.mem[5] = Value(7, 0, 7, 1, 1)
calculate('F+f(2)', exact=Fraction(12))
for slot, body in [(0, b'f(x)'), (0, b'h(x)'), (0, b''), (1, b'g(x)'), (2, b'unknown(x)')]:
    before = bytes(s); assert not lib.aero_define(C.byref(s), slot, body, C.byref(e)); assert bytes(s) == before
    checked('function_rejection')
lib.aero_defaults(C.byref(s))

# Exact algebra compared symbolically, then evaluated against Fraction at rational x.
x = sp.Symbol('x')
for _ in range(140):
    a, b, c, d = [rng.randint(-7, 7) for _ in range(4)]; den = rng.randint(1, 9); power = rng.randint(1, 4)
    expression = f'(({a})*x+({b}))^{power}/({den})-(({c})*x+({d}))^2'
    original = sp.sympify(expression.replace('^', '**'))
    for diff in [0, 1]:
        out = C.create_string_buffer(512); error = C.create_string_buffer(160)
        assert lib.aero_symbolic(expression.encode(), C.byref(ctx), diff, out, error, 160), (expression, error.value)
        actual = sp.sympify(out.value.decode().replace('^', '**'))
        expected = sp.diff(original, x) if diff else sp.expand(original)
        assert sp.simplify(actual - expected) == 0, (expression, out.value, expected)
        checked('polynomial_identity')
        point = Fraction(rng.randint(-3, 3), rng.randint(1, 5))
        target = Fraction(expected.subs(x, sp.Rational(point.numerator, point.denominator)))
        calculate(out.value.decode(), point, target)

for bad in ['x^9', 'sin(x)', '1/x', 'x^0', 'pi*x']:
    out = C.create_string_buffer(512); error = C.create_string_buffer(160)
    assert not lib.aero_symbolic(bad.encode(), C.byref(ctx), 0, out, error, 160)
    assert error.value; checked('bounded_CAS')
for angle in range(3):
    ctx.angle = angle
    scale = mp.pi / 180 if angle == 0 else mp.pi / 200 if angle == 2 else mp.mpf(1)
    expressions = [('sin(x^2)', lambda z: 2*z*scale*mp.cos(z*z*scale)), ('cos(x)', lambda z: -scale*mp.sin(z*scale)), ('tan(x)', lambda z: scale/mp.cos(z*scale)**2), ('asin(x)', lambda z: 1/(scale*mp.sqrt(1-z*z))), ('acos(x)', lambda z: -1/(scale*mp.sqrt(1-z*z))), ('atan(x)', lambda z: 1/(scale*(1+z*z))), ('ln(x)', lambda z: 1/z), ('log2(x)', lambda z: 1/(z*mp.log(2))), ('log(x;3)', lambda z: 1/(z*mp.log(3))), ('sqrt(x)', lambda z: 1/(2*mp.sqrt(z))), ('cbrt(x)', lambda z: 1/(3*mp.root(z,3)**2)), ('exp(x)', mp.exp), ('sinh(x)', mp.cosh), ('cosh(x)', mp.sinh), ('tanh(x)', lambda z: 1/mp.cosh(z)**2), ('x/(x+1)', lambda z: 1/(z+1)**2), ('x^2*sin(x)', lambda z: 2*z*mp.sin(z*scale)+z*z*scale*mp.cos(z*scale)), ('x^(-2)', lambda z: -2/z**3)]
    for expression, reference in expressions:
        out = C.create_string_buffer(512); error = C.create_string_buffer(160)
        assert lib.aero_symbolic(expression.encode(), C.byref(ctx), 1, out, error, 160), (expression, error.value)
        for _ in range(5):
            point = Fraction(rng.randint(1, 7), 10)
            actual = calculate(out.value.decode(), point)
            target = float(reference(mp.mpf(point.numerator)/point.denominator))
            assert math.isclose(actual, target, rel_tol=3e-12, abs_tol=3e-12), (angle, expression, out.value, actual, target)
            checked('chain_rule')
ctx.angle = 0

# Probability references preserve very small nonzero tails.
for _ in range(170):
    n = rng.choice([0, 1, 5, 20, 100, 500, 1000, 5000]); p = rng.choice([.001, .01, .1, .3, .5, .9, .99, .999]); k = rng.randrange(n+1)
    q = mp.mpf(p)
    pmf = mp.binomial(n, k)*q**k*(1-q)**(n-k)
    cdf = mp.fsum(mp.mpf(math.comb(n,j))*q**j*(1-q)**(n-j) for j in range(k+1)) if k < n*p else 1-mp.fsum(mp.mpf(math.comb(n,j))*q**j*(1-q)**(n-j) for j in range(k+1,n+1))
    for fn, target in [(lib.aero_binomial_pmf, pmf), (lib.aero_binomial_cdf, cdf)]:
        actual = fn(n, k, p); target = float(target)
        assert math.isclose(actual, target, rel_tol=8e-11, abs_tol=2e-300), (n, k, p, actual, target)
        checked('binomial')
for p in [0., 1.]:
    for n in [0, 1, 5000]:
        for k in [-1, 0, n, n+1]:
            pmf = lib.aero_binomial_pmf(n,k,p); cdf = lib.aero_binomial_cdf(n,k,p)
            assert pmf == (1 if k == (0 if p == 0 else n) else 0)
            assert cdf == (1 if k >= (0 if p == 0 else n) else 0); checked('probability_edges')
for lo, hi in [(-1,1), (-8,-7), (7,8), (10,11), (-20,-19), (19,20), (0,0), (-1e-8,1e-8)]:
    target = (mp.erfc(mp.mpf(lo)/mp.sqrt(2))-mp.erfc(mp.mpf(hi)/mp.sqrt(2)))/2 if lo >= 0 else (mp.erfc(-mp.mpf(hi)/mp.sqrt(2))-mp.erfc(-mp.mpf(lo)/mp.sqrt(2)))/2 if hi <= 0 else mp.erf(mp.mpf(hi)/mp.sqrt(2))/2-mp.erf(mp.mpf(lo)/mp.sqrt(2))/2
    actual = lib.aero_normal_interval(0,1,lo,hi)
    assert math.isclose(actual,float(target), rel_tol=2e-8 if abs(lo)<1e-7 else 2e-13,abs_tol=1e-300), (lo,hi,actual,target)
    checked('normal')
assert math.isclose(lib.aero_normal_interval(-1e308,1e308,1e308,1.5e308), float((mp.erfc(2/mp.sqrt(2))-mp.erfc(mp.mpf('2.5')/mp.sqrt(2)))/2),rel_tol=1e-13)
checked('normal_overflow_safe')
for args in [(0,-1,0,1),(0,1,1,0),(math.nan,1,0,1)]:
    assert math.isnan(lib.aero_normal_interval(*args)); checked('probability_rejection')

for expression, target, dims in [('60 km / (2 h)|km/h',30,[1,0,-1,0,0,0,0,0]), ('2 N * 3 m|J',6,[2,1,-2,0,0,0,0,0]), ('1 MiB + 1 KiB|B',1049600,[0,0,0,0,0,0,0,1]), ('1 L|m^3',.001,[3,0,0,0,0,0,0,0]), ('1 ft + 2 in|cm',35.56,[1,0,0,0,0,0,0,0]), ('2 kg * (3 m/s)^2 / 2|J',9,[2,1,-2,0,0,0,0,0]), ('90 deg|rad',math.pi/2,[0]*8)]:
    val = C.c_double(); dimension = (C.c_int*8)(); unit = C.create_string_buffer(160); error = C.create_string_buffer(160)
    assert lib.aero_dimensions(expression.encode(),C.byref(val),dimension,unit,160,error,160), (expression,error.value)
    assert math.isclose(val.value,target,rel_tol=2e-13,abs_tol=1e-13) and list(dimension)==dims, (expression,val.value,list(dimension))
    checked('dimensions')
for invalid in ['1 m + 1 s', '2 m|s', '1/0 m', '1 degC', '1 m^1.5', '1e300 m * 1e300 m', '1e-200*1e-200 m', '1 m^100']:
    val=C.c_double(); dimension=(C.c_int*8)(); unit=C.create_string_buffer(160); error=C.create_string_buffer(160)
    assert not lib.aero_dimensions(invalid.encode(),C.byref(val),dimension,unit,160,error,160), invalid
    assert error.value; checked('dimension_rejection')

# Matrix results checked against independent NumPy operations on sizes 1..4.
for n in range(1,5):
    s.matrix_n=n
    for _ in range(18):
        a=np.array([[rng.randint(-8,8) for j in range(n)] for i in range(n)],dtype=float)
        b=np.array([[rng.randint(-8,8) for j in range(n)] for i in range(n)],dtype=float)
        for i in range(n):
            for j in range(n): s.matrix[0][i*4+j]=a[i,j]; s.matrix[1][i*4+j]=b[i,j]
        for op,target in [(0,a+b),(1,a-b),(2,a@b),(3,a.T)]:
            r=Result(); assert lib.aero_matrix_run(C.byref(s),op,C.byref(r)), lines(r)
            actual=np.array([[float(v) for v in row.split(';')] for row in lines(r)[1:]])
            assert np.allclose(actual,target,rtol=2e-11,atol=1e-11),(op,actual,target); checked('matrix_arithmetic')
        r=Result(); assert lib.aero_matrix_run(C.byref(s),4,C.byref(r)) and r.value==np.linalg.matrix_rank(a); checked('matrix_rank')
        assert lib.aero_matrix_run(C.byref(s),5,C.byref(r)) and math.isclose(r.value,float(np.linalg.det(a)),rel_tol=1e-10,abs_tol=1e-9); checked('matrix_determinant')
        if np.linalg.matrix_rank(a)==n:
            assert lib.aero_matrix_run(C.byref(s),6,C.byref(r)); text=lines(r)
            # Legacy inverse report prints indexed cells; reconstruction is verified below.
            actual=np.array([[float(v) for v in row.split()] for row in text[:n]])
            assert actual.shape == (n,n) and np.allclose(actual,np.linalg.inv(a),rtol=6e-9,atol=1e-11), (a,actual,np.linalg.inv(a))
            checked('matrix_inverse')
lib.aero_defaults(C.byref(s))
for _ in range(90):
    n=rng.choice([2,3]); a=[rng.randint(-8,8) for _ in range(n)]; b=[rng.randint(-8,8) for _ in range(n)]
    text=';'.join(map(str,a))+'|'+';'.join(map(str,b)); r=lab(3,text,np.dot(a,b))
    rows=lines(r)
    assert math.isclose(float(rows[0].split('~=')[1]),math.sqrt(sum(v*v for v in a)),rel_tol=1e-13,abs_tol=1e-13)
    checked('vector')
for _ in range(80):
    xs=np.array([rng.uniform(-8,8) for _ in range(5)])
    ys=2.3-1.8*xs+np.array([rng.uniform(-.2,.2) for _ in range(5)])
    pairs=';'.join(f'{q:.14g}' for xy in zip(xs,ys) for q in xy)
    r=lab(4,'linear|'+pairs+'|3',np.polyfit(xs,ys,1)[0],tol=1e-10)
    rows=lines(r); expected=np.corrcoef(xs,ys)[0,1]
    assert math.isclose(float(rows[3].split('~=')[1]),expected,rel_tol=1e-12,abs_tol=1e-12); checked('regression')
for mode,data,a,b in [('exp','0;2;1;4;2;8',2,math.log(2)),('log','1;2;e;5;e^2;8',2,3),('power','1;2;2;8;3;18',2,2)]:
    r=lab(4,mode+'|'+data,b); assert math.isclose(float(lines(r)[1].split('~=')[1]),a,rel_tol=1e-12); checked('nonlinear_regression')
for bad in ['linear|1;2;1;3', 'exp|0;0;1;2', 'power|-1;2;2;3', 'linear|1;2;3']:
    lab(4,bad,valid=False)

for t,text,target in [(0,'1/3+1/6',.5),(1,'2;-3',1.5),(2,'1;-3;2',2),(8,'10;.5;3',.171875),(9,'0;1;-1;1',.6826894921370859),(13,'x^2-1|0|-2|2',2)]: lab(t,text,target)
r=lab(13,'1/(x-0.3)|0|1',0); assert not any(v.startswith('Root:') for v in lines(r)); checked('pole_rejection')
r=lab(13,'x^2|1|-2|2',1); assert sum('Intersection:' in v for v in lines(r))==2 and any(v.startswith('min:') for v in lines(r)); checked('graph_analysis')
for text,target in [('0|3;?',9*math.pi),('0|?;4*pi',2),('1|3;4;?',12),('2|2;?',32*math.pi/3),('3|3;4;?',5),('3|?;4;5',3),('4|2;3;?',6),('5|12;?;4',3),('6|12;2;?',24),('7|100;5;?',20),('8|2;3;?',9),('9|10;2;?',5),('10|0.02;?',50),('11|2;3;?',6*9.80665)]: lab(6,text,target)
for bad in ['1|?;0;0','3|?;5;3','0|-1;?','5|12;?;0','4|?;?;3']: lab(6,bad,valid=False)

for bits in [32,64]:
    for _ in range(65):
        value=rng.uniform(-1e5,1e5); text=f'{value:.15g}'; packed=struct.pack('<f' if bits==32 else '<d',float(text)); raw=int.from_bytes(packed,'little')
        r=lab(10,f'{bits}|{text}'); actual=int(lines(r)[1].split(':')[1],16); assert actual==raw; checked('IEEE_encoding')
    exponent_bits=8 if bits==32 else 11; fraction_bits=23 if bits==32 else 52
    for raw,category in [(0,'Zero'),(1,'Subnormal'),(1<<(bits-1),'Zero'),(((1<<exponent_bits)-1)<<fraction_bits,'Infinity'),((((1<<exponent_bits)-1)<<fraction_bits)|1,'NaN')]:
        r=lab(10,f'{bits}|0x{raw:X}'); assert category in lines(r)[0] and int(lines(r)[1].split(':')[1],16)==raw; checked('IEEE_special')

# Independently parse every generated study problem instead of trusting its answer.
for topic in range(6):
    s.topic=topic
    for _ in range(35):
        question=C.create_string_buffer(512); explanation=C.create_string_buffer(512); answer=Value()
        lib.aero_study_question(C.byref(s),question,C.byref(answer),explanation)
        q=question.value.decode(); nums=list(map(int,re.findall(r'-?\d+',q)))
        if topic==0: target=Fraction(nums[0],nums[1])+Fraction(nums[2],nums[3])
        elif topic==1: target=Fraction(-nums[1],nums[0])
        elif topic==2: target={0:Fraction(0),30:Fraction(1,2),90:Fraction(1)}[nums[0]]
        elif topic==3: target=Fraction(nums[0]*nums[1]**(nums[0]-1))
        elif topic==4: target=Fraction(nums[0]*nums[3]-nums[1]*nums[2])
        else: target=Fraction(1,2**nums[0])
        assert answer.exact and Fraction(answer.num,answer.den)==target,(topic,q,answer.num,answer.den,target)
        assert explanation.value; checked('study')
report=dict(status='PASS',checks=checks,groups=groups,seed=20261009,references=['Fraction exact rational','SymPy polynomial identities','NumPy matrices and regressions','mpmath 100 digits'],physical_psp_test=False)
(ROOT/'build/extensions-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report),flush=True)
