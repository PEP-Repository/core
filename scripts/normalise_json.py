#!/usr/bin/env python3

import argparse
import os.path
import json

def main():
    parser = argparse.ArgumentParser(
            description="Formats given json file")
    parser.add_argument("file")

    args = parser.parse_args()
    
    if not os.path.isfile(args.file):
        fatal(f"file '{args.file}' does not exist") 

    data = None
    with open(args.file, "rb") as inf:
        with open(args.file + ".tmp", "w") as outf:
            json.dump(json.load(inf), outf, indent=2)
    os.replace(src=args.file+".tmp", dst=args.file)

def fatal(text):
    print("ERROR: " + text, file=sys.stderr)
    sys.exit(1)

main()
