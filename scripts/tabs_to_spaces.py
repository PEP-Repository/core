#!/usr/bin/env python3

import io
import os.path
import sys
import argparse
import subprocess
import shutil

def main():
    parser = argparse.ArgumentParser(
            description="Chances the leading tabs in the "
                        "specified file to two spaces.")
    parser.add_argument("file")

    args = parser.parse_args()
    
    if not os.path.isfile(args.file):
        fatal(f"file '{args.file}' does not exist") 

    with open(args.file, "rb") as clean_file:
        with open(args.file + ".tmp", "bw") as output_file:
            while True:
                clean_line = clean_file.readline()

                if not clean_line:
                    break
                
                i = 0 # index of first non-tab 
                while clean_line[i]==9:
                    i += 1

                output_file.write(b"  " * i + clean_line[i:])

    shutil.copymode(args.file, args.file + ".tmp")

    os.replace(src=args.file+".tmp", dst=args.file)
        

def fatal(text):
    print("ERROR: " + text, file=sys.stderr)
    sys.exit(1)

main()
