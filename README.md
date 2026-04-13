# GNU Backgammon UBGI Fork

This repository is a focused GNU Backgammon fork with native `--ubgi` support.

## Build (headless)

```bash
./autogen.sh
./configure --with-gtk=no --with-gtk3=no --with-board3d=no --with-python=no
make -j4
```

## macOS quick fix

macOS ships old `/usr/bin/bison` (2.3), which fails on this repo.

If `make` fails with `sgf_y.y` / `external_y.y` syntax errors, use this:

```bash
brew install bison flex
export PATH="$(brew --prefix bison)/bin:$(brew --prefix flex)/bin:$PATH"
bison --version
./autogen.sh
./configure --with-gtk=no --with-gtk3=no --with-board3d=no --with-python=no
make -j4
```

If `bison --version` still shows `2.3`, your shell is still using `/usr/bin/bison`.

## Run UBGI mode

- Protocol reference: https://github.com/oysteijo/Universal-Backgammon-Interface

```bash
./gnubg --ubgi --pkgdatadir . --datadir .
```

## Runtime Data Lookup

This fork adds XDG-friendly data lookup for runtime files (weights, bearoff DBs, MET XML).

Recommended location:

- `~/.local/share/gnubg`

Typical files:

- `gnubg.weights`, `gnubg.wd`, `gnubg_ts0.bd`, `met/Kazaross-XG2.xml`

If `gnubg.wd` is missing but `gnubg.weights` is present, generate it with:

```bash
./makeweights < gnubg.weights > gnubg.wd
```

## Lineage

- Upstream GNU Backgammon source and history: Savannah `gnubg` (based from `release-1_08_003`)
- macOS compatibility fork work by Ken Riley (`kenr@nodots.com`)
- UBGI protocol work in this fork

## Notes

- `README` from upstream is still present for original GNUbg details.
- This fork currently focuses on CLI/protocol use rather than GUI packaging.

## References

- GNU Backgammon project page: https://www.gnu.org/software/gnubg/
- GNU Backgammon Savannah project: https://savannah.gnu.org/projects/gnubg
- GNU Backgammon Savannah Git: https://git.savannah.gnu.org/cgit/gnubg.git
- Ken Riley macOS fork: https://github.com/nodots/gnubg-macos
- UBGI protocol: https://github.com/oysteijo/Universal-Backgammon-Interface
