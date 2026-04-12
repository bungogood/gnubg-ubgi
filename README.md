# GNU Backgammon UBGI Fork

This repository is a focused GNU Backgammon fork with native `--ubgi` support.

## Lineage

- Upstream GNU Backgammon source and history: Savannah `gnubg` (based from `release-1_08_003`)
- macOS compatibility fork work by Ken Riley (`kenr@nodots.com`)
- UBGI protocol work in this fork

## UBGI

- Protocol reference: https://github.com/oysteijo/Universal-Backgammon-Interface
- Run engine mode:

```bash
./gnubg --ubgi --pkgdatadir . --datadir .
```

## Runtime Data Lookup

GNUbg needs runtime data files such as weights, bearoff databases, and MET XML files.

Lookup order for program data (`pkgdatadir`) is:

1. `--pkgdatadir <dir>` CLI option
2. `GNUBG_PKGDATADIR` environment variable
3. XDG user data dir: `$XDG_DATA_HOME/gnubg` (default `~/.local/share/gnubg`)
4. XDG system data dirs: each entry in `$XDG_DATA_DIRS` with `/gnubg` appended
5. Build-time default (`AC_PKGDATADIR`)

Recommended modern location:

- `~/.local/share/gnubg`

Typical files under that directory:

- `gnubg.weights`
- `gnubg.wd`
- `gnubg_ts0.bd`
- `met/Kazaross-XG2.xml`

If `gnubg.wd` is missing but `gnubg.weights` is present, generate it with:

```bash
./makeweights < gnubg.weights > gnubg.wd
```

## Build (headless example)

```bash
./configure --with-gtk=no --with-gtk3=no --with-board3d=no --with-python=no
make -j4
```

## Notes

- `README` from upstream is still present for original GNUbg details.
- This fork currently focuses on CLI/protocol use rather than GUI packaging.

## References

- GNU Backgammon project page: https://www.gnu.org/software/gnubg/
- GNU Backgammon Savannah project: https://savannah.gnu.org/projects/gnubg
- GNU Backgammon Savannah Git: https://git.savannah.gnu.org/cgit/gnubg.git
- Ken Riley macOS fork: https://github.com/nodots/gnubg-macos
- UBGI protocol: https://github.com/oysteijo/Universal-Backgammon-Interface
