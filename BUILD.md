# Build CalcAero 1.3

Use pspdev v20261001 with PSPSDK, HarfBuzz, FreeType, libpng, zlib, bzip2 and C++
runtime libraries. `assets` already contains the catalogs and font subsets.

```sh
export PSPDEV=/path/to/pspdev
export PATH="$PSPDEV/bin:$PATH"
make -j2
make EBOOT-SIGNED.PBP
```

Install EBOOT-SIGNED.PBP as PSP/GAME/CalcAero/EBOOT.PBP and copy the entire assets
folder beside it. `ebootsign` comes from int-0/ebootsigner. The unsigned EBOOT.PBP
is kept for reproducible comparison and other signing tools.

User module attributes=0, main USER|VFPU, user callbacks, 256 KiB main stack,
4096 KiB heap, MEMSIZE=0, PSP_SYSTEM_VER=6.60. The memory budget is recorded in
VERIFY/psp-audit.json. No 64 MiB expansion or CPU overclock is required.

Host regression suite: `bash tests/run-host.sh`. Requires C compiler, ASan/UBSan,
Python Pillow/NumPy/SymPy/mpmath and FreeType/HarfBuzz development headers and
libraries. Adapt HOST_FONT_INCLUDE/HOST_FONT_LIBS to the host installation.
`tests/preview.c` rasterizes the real C renderer, including its font atlas.
`tests/build_locale.py` optionally regenerates catalogs/font subsets using
fontTools and the upstream fonts. PSP never uses Python or a translation model.

`tests/audit_psp.py` inspects ELF/SFO/PBP; pass PSP_TOOLS if the SDK has a different
location. Native tests use PPSSPP 1.20.4's controller and read-only VRAM debugger
protocol, not debugger memory writes. final_native.py tests the signed file and
starts a second PSP process. finish_native.py records the completed prefix of an
interrupted run, checks that its compiled program payload matches the release,
then verifies the release pictures, final assets, first start and a fresh process.
The resulting JSON distinguishes the earlier EBOOT from the release EBOOT.

The statically linked pthread-embedded dependency retains its LGPL license.
Its upstream source is included under vendor/pthread-embedded. CalcAero's full
source, libraries selected by Makefile, and the SDK allow rebuilding/relinking
with a modified library. THIRD-PARTY.md lists provenance and license notices.
