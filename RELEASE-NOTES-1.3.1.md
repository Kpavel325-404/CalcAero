# CalcAero 1.3.1 — Portuguese and French update

- Added **Português** (`pt`) as language 27 and **Français** (`fr`) as language 28, with 535 messages each.
- Added the required accented glyphs to the compact base font.
- Reworked the picker into 14/14 rows; all 28 entries fit the 480×272 screen.
- Preserved the original 26 language codes/IDs and save formats.
- Kept English on first boot and immediate `config.json` persistence.
- Updated the signed User Mode EBOOT and localized export version headers.

Copy the `PSP` folder onto the existing installation and replace `EBOOT.PBP`
and the complete `assets` folder. Keep `config.json`, `state.dat`, `aero.dat`,
their backups and `reports/`. This package contains no user configuration.

Fresh build, catalog, host regression and PPSSPP results are recorded in
[VERIFY/UPDATE-1.3.1](VERIFY/UPDATE-1.3.1/README.md). Physical OFW 6.61 / ARK-5
confirmation belongs to 1.3; a separate physical test report for 1.3.1 is pending.
The Portuguese and French translations were prepared with AI assistance and have not yet
received a native-speaker review.
