# Requirements

  1. `libmata` installed using `sudo make install` (see [Mata homepage](https://github.com/VeriFIT/mata))

Alternatively, `nix develop` drops you into a shell with `libmata` and the
Python dependencies already available, and `nix build` / `nix run` produce
the C++ and Python interpreters directly (see `flake.nix`).
