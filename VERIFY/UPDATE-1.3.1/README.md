# CalcAero 1.3.1 verification

The final signed EBOOT SHA-256 is `14c6ce3392b1326f80d99b4fad7ee6c6a3188597d41cf10add1100a8738d54bb`.
These are fresh records for the Portuguese/French update, with 28 catalogs and
535 messages per catalog. Older verification and physical reports remain in
their original directories and still describe their original release.

- Numerical regression: 22,537 + 2,878 = **25,415 checks**, PASS.
- Host ASan/UBSan: malformed input, save CRC/rollback, legacy language IDs,
  Portuguese/French configuration restore and controller navigation, PASS.
- UI: **1748 states**; 17316 / 32768 vertices;
  geometry within 480×272 and no missing required glyphs.
- ELF/SFO/container audit: User Mode, MEMSIZE=0, 4 MiB heap, 256 KiB stack,
  OFW 6.60 target, no CPU clock change, int-0/ebootsigner.
- Exact signed EBOOT executed in PPSSPP: PSP-1000, 32 MiB, 222 MHz,
  HLE 6.60/6.61; English first boot, 28 choices, Portuguese/French selection,
  immediate config.json writes, exact fractions, real TXT/BMP exports and
  fresh-process restore. All pages/help were covered by the host UI suite.

Native screenshots in `screens` are read from PSP VRAM. Each scene has both
VRAM buffers; one may contain the previous frame. The public English language
picker was checked visually. `exports` contains files produced by the signed
application itself, including Portuguese and French UTF-8 history.
Host images and an XMB background are produced by the actual C renderer.

The first controller run completed English/PT and the French selection/export.
Its last assertion incorrectly expected recalculating an identical formula to
add a duplicate history item. CalcAero deliberately deduplicates that item.
The corrected final check verifies a fresh autosave and the unchanged exact
answer/history. It was rerun in another fresh process after validating the
retained prefix, EBOOT, assets, exports and saved-state CRC. Both the original
trace and the successful final trace are included; the JSON records this scope.

Physical OFW/CFW tests for **1.3.1 are not recorded yet**. The owner's confirmed
OFW 6.61 and ARK-5 5.1.0 cIPL tests describe **1.3**. HLE is not Sony's loader.
The two new translations were prepared with AI assistance; native-speaker review
is welcome. English owner screenshots from 1.3 have not been altered.
