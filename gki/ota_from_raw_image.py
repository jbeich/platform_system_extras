#!/usr/bin/env python3
import sys
from zipfile import ZipFile

def main():
  # FIXME actually build it
  if len(sys.argv) < 3:
    sys.exit(1)
  with ZipFile(sys.argv[2], 'w') as zip:
    zip.write(sys.argv[1], arcname="payload.bin")
    zip.writestr("payload_properties.txt", "foo=bar\n")

if __name__ == '__main__':
  main();
