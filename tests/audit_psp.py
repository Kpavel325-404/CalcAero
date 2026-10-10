"""Audit exact ELF user attributes, import libraries, SFO memory and signed PBP."""
from pathlib import Path
import struct,json,hashlib,subprocess,re,os,shutil
ROOT=Path(__file__).resolve().parent.parent
SDK=Path(os.environ.get('PSP_TOOLS') or Path(shutil.which('psp-nm') or 'psp-nm').resolve().parent)
def u32(b,at=0):return struct.unpack_from('<I',b,at)[0]
def elf_sections(b):
 at=u32(b,32);size,count,names=struct.unpack_from('<HHH',b,46);headers=[struct.unpack_from('<10I',b,at+i*size) for i in range(count)]
 string=headers[names];strings=b[string[4]:string[4]+string[5]];out={}
 for h in headers:
  name=strings[h[0]:].split(b'\0')[0].decode();out[name]=dict(type=h[1],addr=h[3],offset=h[4],size=h[5],data=b[h[4]:h[4]+h[5]])
 return out
b=(ROOT/'calcaero.elf').read_bytes();s=elf_sections(b)
def address(addr,n):
 for sec in s.values():
  if sec['type']!=8 and sec['addr']<=addr and addr+n<=sec['addr']+sec['size']:
   off=sec['offset']+addr-sec['addr'];return b[off:off+n]
 raise AssertionError(hex(addr))
def cstring(addr):
 out=bytearray()
 for i in range(100):
  c=address(addr+i,1)[0]
  if not c:return out.decode()
  out.append(c)
 raise AssertionError('import name')
allowed={'sceUtility','scePower','sceCtrl','sceDisplay','sceGe_user','sceNetInet','IoFileMgrForUser','Kernel_Library','ModuleMgrForUser','StdioForUser','SysMemUserForUser','ThreadManForUser','UtilsForUser','LoadExecForUser'}
lib=s['.lib.stub']['data'];imports=[];at=0
while at<len(lib):
 name,version,attr,length,vcount,fcount,nids,stubs=struct.unpack_from('<IHHBBHII',lib,at)
 name=cstring(name);assert name in allowed and not name.endswith(('_driver','ForKernel'))
 imports.append(dict(library=name,version=version,attributes=hex(attr),functions=fcount,nids=[hex(u32(address(nids+i*4,4))) for i in range(fcount)]));at+=length*4
module=s['.rodata.sceModuleInfo']['data'];assert struct.unpack_from('<H',module)[0]==0
nm=subprocess.check_output([str(SDK/'psp-nm'),str(ROOT/'calcaero.elf')],text=True)
symbols={name:int(addr,16) for addr,typ,name in (line.split() for line in nm.splitlines() if len(line.split())==3) if re.fullmatch('[0-9a-f]+',addr)}
thread=u32(address(symbols['sce_newlib_attribute'],4));assert thread&0x80000000 and not thread&0x1000
stack=u32(address(symbols['sce_newlib_stack_kb_size'],4));heap=u32(address(symbols['sce_newlib_heap_kb_size'],4));assert stack==256 and heap==4096
sfo=(ROOT/'PARAM.SFO').read_bytes();keyat,datat,count=struct.unpack_from('<III',sfo,8);fields={}
for i in range(count):
 key,fmt,length,maxlen,off=struct.unpack_from('<HHIII',sfo,20+i*16);name=sfo[keyat+key:].split(b'\0')[0].decode();raw=sfo[datat+off:datat+off+length]
 fields[name]=u32(raw) if fmt==0x404 else raw.rstrip(b'\0').decode()
assert fields['MEMSIZE']==0 and fields['PSP_SYSTEM_VER']=='6.60' and fields['TITLE']=='CalcAero 1.3'
signed=(ROOT/'EBOOT-SIGNED.PBP').read_bytes();unsigned=(ROOT/'EBOOT.PBP').read_bytes()
a=struct.unpack_from('<8I',signed,8);u=struct.unpack_from('<8I',unsigned,8);assert signed[:4]==b'\0PBP' and signed[a[6]:a[6]+4]==b'~PSP' and unsigned[u[6]:u[6]+4]==b'\x7fELF'
assert signed[a[0]:a[6]]==unsigned[u[0]:u[6]]
text_data_bss=sum(sec['size'] for name,sec in s.items() if name in ['.text','.init','.fini','.sceStub.text','.rodata','.data','.sdata','.sbss','.bss'])
result=dict(status='PASS',module_attributes='0x0 (user)',main_thread_attributes=hex(thread),main_stack_kib=stack,heap_kib=heap,static_image_bytes=text_data_bss,reserved_image_heap_stack_bytes=text_data_bss+(heap+stack)*1024,imports=imports,sfo=fields,psp1000_large_memory=False,cpu_clock_changes=False,signer='int-0/ebootsigner (pspdev v20261001)',signed_psp_magic='~PSP',unsigned_eboot_sha256=hashlib.sha256(unsigned).hexdigest(),signed_eboot_sha256=hashlib.sha256(signed).hexdigest(),physical_ofw_660_test=False,physical_ofw_661_test=False)
(ROOT/'build/psp-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k not in ['imports']},indent=2))
