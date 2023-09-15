import sys
import os
import subprocess
import argparse

def main():
    parser = argparse.ArgumentParser(description="Find which library contains a given symbol.")
    parser.add_argument("symbol", type=str, help="The symbol to search for.")
    parser.add_argument("lib_dir", type=str, help="The directory containing the library files.")

    args = parser.parse_args()

    found_libs = find_symbol_in_libs(args.symbol, args.lib_dir)

    if found_libs:
        print(f"Symbol {args.symbol} found in: {', '.join(found_libs)}")
    else:
        print(f"Symbol {args.symbol} not found in any library in {args.lib_dir}.")

def find_symbol_in_libs(symbol, lib_dir):
    found_libs = []
    for root, _, files in os.walk(lib_dir):
        for file_name in files:
            if not file_name.endswith('.a'):
                continue

            lib_path = os.path.join(root, file_name)
            nm_output = os.popen(f'llvm-nm -g {lib_path} 2>/dev/null').read()
            if symbol in nm_output:
                found_libs.append(lib_path)
    return found_libs

if __name__ == "__main__":
    sys.exit(main())
