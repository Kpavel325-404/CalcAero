This is the exact static pthread-embedded archive selected by the build SDK.
The corresponding upstream source is ../pthread-embedded; LGPL notice is in
../../LICENSES/Pthreads-LGPL-2.1.txt. CalcAero source is supplied in full.
To relink with a changed pthread library, build it with the PSP toolchain,
replace libpthread.a here, then use: make clean; make LIBDIR=vendor/relink
and: make EBOOT-SIGNED.PBP LIBDIR=vendor/relink
The original executable uses pspdev v20261001 (PSP GCC 15.2.0).
