"""Independent reference checks using Python Fraction and mpmath at 100 decimal digits."""
import ctypes as C
import random
import json
import math
import sys
from pathlib import Path
from fractions import Fraction
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'build/python'))
import mpmath as mp
mp.mp.dps = 100
class Value(C.Structure):
    _fields_ = [('re',C.c_double),('im',C.c_double),('num',C.c_int64),('den',C.c_int64),('exact',C.c_int)]
class Context(C.Structure):
    _fields_ = [('angle',C.c_int),('complex_mode',C.c_int),('ans',Value),('mem',Value*6),('x',Value)]
class Error(C.Structure):
    _fields_ = [('code',C.c_int),('pos',C.c_int)]
class Result(C.Structure):
    _fields_ = [('line',(C.c_char*160)*48),('count',C.c_int),('ok',C.c_int),('approx',C.c_int),('value',C.c_double),('error',C.c_double)]
root = Path(__file__).resolve().parents[1]
lib = C.CDLL(str(root/'build/libcalc.so'))
lib.cl_context_init.argtypes=[C.POINTER(Context)]
lib.cl_calculate.argtypes=[C.c_char_p,C.POINTER(Context),C.POINTER(Value),C.POINTER(Error)]
lib.cl_tool_run.argtypes=[C.c_int,C.c_char_p,C.POINTER(Context),C.POINTER(Result)]
lib.cl_convert_unit.argtypes=[C.c_int,C.c_int,C.c_int,C.c_double,C.POINTER(C.c_double)]
lib.cl_programmer.argtypes=[C.c_char_p,C.c_int,C.c_int,C.POINTER(C.c_uint64),C.c_char_p,C.c_size_t]
ctx=Context();lib.cl_context_init(C.byref(ctx))
for i in range(6): ctx.mem[i]=Value(i+3,0,i+3,1,1)
ctx.ans=Value(9,0,9,1,1)
rng=random.Random(20261004);checks=0;worst=0;groups={}
def calc(expr,expected=None,angle=0,complex_mode=0,exact=None,error=None,tol=2e-12,group='core'):
    global checks,worst
    ctx.angle=angle;ctx.complex_mode=complex_mode
    v=Value();e=Error();ok=lib.cl_calculate(expr.encode(),C.byref(ctx),C.byref(v),C.byref(e))
    checks+=1;groups[group]=groups.get(group,0)+1
    if error is not None:
        assert not ok and e.code in error, (expr,'expected error',error,ok,e.code,v.re,v.im)
        return
    assert ok,(expr,'error',e.code,e.pos)
    if exact is not None:
        assert v.exact and Fraction(v.num,v.den)==exact,(expr,'exact',v.exact,v.num,v.den,exact)
    if expected is not None:
        expected=complex(expected);err=abs(complex(v.re,v.im)-expected)/max(1,abs(expected));worst=max(worst,err)
        assert err<=tol,(expr,v.re,v.im,expected,err,tol)
    return v
for expr,num in {'-2^2':-4,'(-2)^2':4,'2^3^2':512,'2^-3':Fraction(1,8),
    '2(3+4)':14,'200*10%':20,'0!':1,'20!':math.factorial(20),'sqrt(4/9)':Fraction(2,3),
    '0.1+0.2':Fraction(3,10),'1,25+0,75':2,'dms(-30;15;0)':Fraction(-121,4),
    'round(-2.5)':-3,'floor(-2/3)':-1,'ceil(-2/3)':0,'frac(-5/3)':Fraction(-2,3),
    'A+B+C+D+E+F':33,'e':math.e,'Ans+1':10,'ncr(52;5)':2598960,'npr(10;3)':720,
    'gcd(-48;18)':6,'lcm(21;6)':42,'mod(-7;3)':-1,'sin(90)':1,'cos(180)':-1}.items():
    calc(expr,num,exact=Fraction(num) if isinstance(num,(int,Fraction)) else None)
for _ in range(10000):
    a,b,c,d=[rng.randint(-1000000,1000000) for j in range(4)];b=b or 1;d=d or 1
    op=rng.choice(['+','-','*','/'])
    if op=='/' and c==0:c=1
    x,y=Fraction(a,b),Fraction(c,d)
    target={'+':lambda:x+y,'-':lambda:x-y,'*':lambda:x*y,'/':lambda:x/y}[op]()
    expr=f'({a}/{b}){op}({c}/{d})'
    calc(expr,float(target),exact=target if abs(target.numerator)<=2**63-1 and target.denominator<=2**63-1 else None,group='rational')
for angle in range(3):
    for _ in range(250):
        x=rng.uniform(-1000,1000);s=f'{x:.15g}';q=mp.mpf(s)*(mp.pi/180 if angle==0 else mp.pi/200 if angle==2 else 1)
        for name,fn in [('sin',mp.sin),('cos',mp.cos),('tan',mp.tan)]:
            calc(f'{name}({s})',float(fn(q)),angle=angle,tol=4e-12,group='angles')
for _ in range(200):
    x=rng.uniform(-.99,.99);s=f'{x:.15g}'
    for angle in range(3):
        for name,fn in [('asin',mp.asin),('acos',mp.acos),('atan',mp.atan)]:
            factor=180/mp.pi if angle==0 else 200/mp.pi if angle==2 else 1
            calc(f'{name}({s})',float(fn(mp.mpf(s))*factor),angle=angle,group='inverse_angles')
for _ in range(500):
    x=10**rng.uniform(-250,250);s=f'{x:.15g}'
    for name,fn in [('ln',mp.log),('log',mp.log10),('log2',lambda x:mp.log(x,2)),('sqrt',mp.sqrt),('cbrt',lambda x:mp.root(x,3))]:
        calc(f'{name}({s})',float(fn(mp.mpf(s))),group='transcendental')
for _ in range(200):
    x=rng.uniform(-100,100);s=f'{x:.15g}'
    for name,fn in [('exp',mp.exp),('sinh',mp.sinh),('cosh',mp.cosh),('tanh',mp.tanh),('asinh',mp.asinh)]:
        calc(f'{name}({s})',float(fn(mp.mpf(s))),group='hyperbolic')
for _ in range(100):
    x,y=rng.uniform(-2,2),rng.uniform(-2,2);s=f'({x:.14g}+({y:.14g})*i)';z=mp.mpc(f'{x:.14g}',f'{y:.14g}')
    for name,fn in [('sqrt',mp.sqrt),('ln',mp.log),('exp',mp.exp),('sin',mp.sin),('cos',mp.cos),('sinh',mp.sinh),('cosh',mp.cosh)]:
        calc(f'{name}({s})',complex(fn(z)),angle=1,complex_mode=1,group='complex')
for expr,target in [('sqrt(-1)',1j),('(1+i)/(1-i)',1j),('abs(3+4i)',5),('re(2+3i)',2),('im(2+3i)',3),('conj(2+3i)',2-3j),('polar(2;90)',2j)]:
    calc(expr,target,complex_mode=1,group='complex')
for expr in ['',')','(', '1+', '1 2', '2..3','sin()','sin(1;2)','unknown(1)','log(1;1)',
    'sqrt(-1)','ln(0)','1/0','0^0','0^-1','tan(90)','(-1)!','171!','ncr(4;5)',
    'sin(1e15)','exp(-1000)','1e-200*1e-200','root(1;0)','i']:
    calc(expr,error=set(range(1,10)),group='errors')
calc('('*50+'1'+')'*50,error={7},group='bounds')
calc('+'.join(['1']*200),error={7},group='bounds')
def tool(t,s,expected=None,valid=True,tol=1e-9):
    global checks
    ctx.angle=0;ctx.complex_mode=0;r=Result();ok=lib.cl_tool_run(t,s.encode(),C.byref(ctx),C.byref(r));checks+=1;groups['tools']=groups.get('tools',0)+1
    assert bool(ok)==valid,(t,s,ok,[bytes(r.line[i]).split(b'\0')[0] for i in range(r.count)])
    if expected is not None:assert math.isclose(r.value,expected,rel_tol=tol,abs_tol=tol),(t,s,r.value,expected)
    return r
tool(0,'1;-3;2',2);tool(0,'1;100000000;1',-1e8,tol=1e-14)
tool(0,'1;0;1');tool(0,'0;2;-6',3);tool(1,'2;1;5;1;-1;1',2)
tool(2,'1;1;1;6;2;-1;1;3;1;2;-1;2',1)
tool(3,'2;1;2;3;4',-2);tool(4,'2;1;2;3;4',-2)
tool(3,'2;1;0;0;1e-15',1e-15,tol=1e-20)
tool(4,'2;1;2;2;4',valid=False);tool(5,'1;2;3;4;5',3)
tool(5,'1e300;1e300;1e300',1e300);tool(6,'x^2-2|0|2',math.sqrt(2))
tool(6,'1/x|-1|1',valid=False);tool(6,'1/(x-0.3)|0|1',valid=False)
tool(7,'sin(x)|30',math.pi/180*math.cos(math.pi/6));tool(7,'x^3|2',12)
tool(7,'abs(x)|0',valid=False);tool(8,'x^2|0|1',1/3);tool(8,'x^2|1|0',-1/3)
tool(8,'1/x|-1|1',valid=False)
for _ in range(100):
    a,b,c=[rng.randint(-50,50) for j in range(3)];x=rng.uniform(-3,3)
    tool(7,f'{a}*x^2+({b})*x+({c})|{x:.15g}',2*a*x+b,tol=2e-7)
    lo,hi=sorted([rng.uniform(-3,3),rng.uniform(-3,3)])
    target=a*(hi**3-lo**3)/3+b*(hi**2-lo**2)/2+c*(hi-lo)
    tool(8,f'{a}*x^2+({b})*x+({c})|{lo:.15g}|{hi:.15g}',target,tol=2e-9)
for g,fr,to,x,target in [(0,4,0,1,.0254),(3,0,1,0,32),(3,1,0,212,100),(3,2,0,273.15,0),(4,0,1,180,math.pi),(11,2,0,1,1000),(11,5,0,1,1024)]:
    v=C.c_double();assert lib.cl_convert_unit(g,fr,to,x,C.byref(v)) and math.isclose(v.value,target,abs_tol=1e-10,rel_tol=1e-12);checks+=1;groups['units']=groups.get('units',0)+1
for bits in [8,16,32,64]:
    mask=2**bits-1
    for _ in range(1000):
        a=rng.getrandbits(bits);b=rng.getrandbits(bits);op=rng.randrange(1,12)
        if op in (5,6):b=rng.randrange(bits)
        if op in (10,11) and b==0:b=1
        target={1:lambda:a&b,2:lambda:a|b,3:lambda:a^b,4:lambda:~a,5:lambda:a<<b,6:lambda:a>>b,
            7:lambda:a+b,8:lambda:a-b,9:lambda:a*b,10:lambda:a//b,11:lambda:a%b}[op]()&mask
        v=C.c_uint64();err=C.create_string_buffer(160);s=str(a) if op==4 else f'{a};{b}'
        assert lib.cl_programmer(s.encode(),bits,op,C.byref(v),err,160),(bits,s,op,err.value)
        assert v.value==target,(bits,s,op,v.value,target);checks+=1;groups['programmer']=groups.get('programmer',0)+1
report={'checks':checks,'reference':'Python Fraction + mpmath 100 digits','seed':20261004,'groups':groups,'max_scaled_error':worst,'status':'PASS'}
(root/'build/numeric-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
