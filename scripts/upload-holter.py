#!/usr/bin/env python3
# Shebang should also work on Windows: see https://docs.python.org/3/using/windows.html#shebang-lines

# This script uploads (pre-pseudonymized) Holter data to PEP: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1638

import sys, argparse, csv, os, errno, shutil, subprocess, time, uuid, codecs


# Reads command line parameters into a structure.
def readCommandLineParameters(argv):
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("dir", help="path to directory containing *.ecg files")
    parser.add_argument("csv", help="path to CSV file associating short pseudonyms with UUID file names") # Not an argparse.FileType('r') because of special processing (below)
    parser.add_argument("col", help="name of PEP column to store data into")
    parser.add_argument("prefix", help="prefix for short pseudonyms associated with data to store in the specified 'col'")
    parser.add_argument("--cli", default="./pepcli", help="path to pepcli executable") # Not an argparse.FileType('r') because of special processing (below)
    parser.add_argument("--oauth-token", help="path to OAuth token file to use for enrollment")
    parser.add_argument("--wet", action='store_true', help="perform actual upload") # Non-dry runs must be specified explicitly
    result = parser.parse_args()
    
    # Deal with (e.g. UTF-8) BOM: https://stackoverflow.com/a/13591421
    # The result.csv field is replaced by a file handle for the specified path
    bytes = min(32, os.path.getsize(result.csv))
    raw = open(result.csv, 'rb').read(bytes)
    if raw.startswith(codecs.BOM_UTF8):
        result.csv = open(result.csv, 'r', encoding='utf-8-sig')
    else:
        result.csv = open(result.csv, 'r')
    
    # Ensure we have a good path to pepcli
    cli = shutil.which(result.cli)
    if cli is None and os.name == 'nt': # If running on Windows: see https://stackoverflow.com/a/1325587
        cli = shutil.which(result.cli + ".exe") # Support specifying executable name without .exe extension
    if cli is None:
        raise FileNotFoundError(errno.ENOENT, os.strerror(errno.ENOENT), result.cli) # https://stackoverflow.com/a/36077407
    if not os.access(cli, os.X_OK):
        raise PermissionError(errno.EPERM, os.strerror(errno.EPERM) + " (not executable)", result.cli)
    result.cli = cli
    
    return result


# Uploads a single file to PEP.
def uploadHolterFile(params, sp, payload):
    # Construct command line
    sections = [params.cli, "--suppress-version-info"]
    sections += ["--loglevel", "warning"]
    if params.oauth_token is not None:
        sections += ["--oauth-token", params.oauth_token]
    if params.wet:
        sections.append("store")
        sections += ["--input-path", payload]
    else:
        sections.append("list")
    sections += ["-c", params.col]
    sections += ["--sp", sp]
    # Execute
    subprocess.run(sections, check=True, stdout=params.stdout)


# Uploads available files to PEP.
def uploadHolterFiles(params, spsByUuid):
    # Collect file(path)s in a list so they can be sorted
    files = list()
    # Iterating over files in a directory: https://stackoverflow.com/a/10378012
    for file in os.listdir(os.fsencode(params.dir)):
        filename = os.fsdecode(file)
        files.append(filename)
    
    # Process files in sorted order so that state can be corrected if errors are encountered
    files.sort()
    for filename in files:
        path = os.path.join(params.dir, filename)
        print("Uploading " + path, end='', flush=True)
        if not filename.endswith(".ecg"):
            print("... skipped (incorrect extension)")
        else:
            id = os.path.splitext(filename)[0]
            if not id in spsByUuid:
                raise ValueError("File " + filename + ": ID not found in lookup table")
            sp = spsByUuid[id]
            print(" to SP " + sp + "... ", end='', flush=True)
            if not sp.startswith(params.prefix):
                print("skipped (incorrect prefix)")
            else:
                uploadHolterFile(params, sp, path)
                if not params.wet:
                    print("skipped (dry run)")
                else:
                    print("done")


# Reads a .CSV file (associating ECG file UUIDs with SP values) into a dictionary.
# Lines must contain at least "SP-value;UUID" fields.
def readCsvFile(file):
    result = dict()
    reader = csv.reader(file, delimiter=';')
    for row in reader:
        if len(row) < 2:
            raise ValueError("Expected at least 2 fields in CSV line but got " + reader.dialect.delimiter.join(row))
        sp = row[0]
        id = str(uuid.UUID(row[1])).translate(str.maketrans('', '', '-')).lower() # Ensure UUID (string) is lowercase without dashes
        if id in result:
            if result[id] != sp:
                raise ValueError("CSV file binds UUID " + id + " to multiple short pseudonyms: " + result[id] + " and " + sp)
        elif sp in result.values():
            raise ValueError("CSV file binds short pseudonym " + sp + " to multiple UUIDs")
        else:
            result[id] = sp
    return result


# Main function.
def main(argv):
    params = readCommandLineParameters(argv)
    params.stdout = open("upload-holter." + str(time.time()) + ".log", "w")
    spsByUuid = readCsvFile(params.csv)
    uploadHolterFiles(params, spsByUuid)


# Invoke main function
if __name__ != "__main__":
    sys.exit("Please invoke this script directly from the command line or interpreter.")
main(sys.argv)
