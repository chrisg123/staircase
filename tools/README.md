# Tools Directory

This directory contains utility scripts that aid in the development process.

## `occt_find_symbol`

A Bash wrapper around `find_symbol.py` that simplifies its usage for OpenCASCADE
symbols.

### Why This Tool?

When developing with OpenCASCADE, it can be challenging to determine which
library contains the symbols you're using. This can lead to linker errors that
are difficult to diagnose. This script automates the process of finding the
right library for a given symbol.


### Usage

```bash
./occt_find_symbol [symbol]
```

Simply replace `[symbol]` with the symbol you're looking for. The script will
search through the built OpenCASCADE libraries and return any matches.
