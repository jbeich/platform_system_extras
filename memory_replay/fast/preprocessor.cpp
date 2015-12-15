#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "MemoryReplay.h"
#include "ReplayParser.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    printf("usage: %s <input file> <output file>\n", argv[0]);
    return 1;
  }

  ReplayParser parser;
  int infd = open(argv[1], O_RDONLY);
  if (infd < 0) {
    err(1, "failed to open input file '%s'", argv[1]);
  }

  printf("Preprocessing memory replay '%s'\n", argv[1]);

  MemoryReplay replay = ReplayParser::Parse(infd, 100000);
  close(infd);

  int outfd = open(argv[2], O_RDWR | O_TRUNC | O_CREAT, 0755);
  if (outfd < 0) {
    err(1, "failed to open output file '%s'", argv[2]);
  }

  printf("Writing memory dump to '%s'\n", argv[2]);
  replay.WriteDump(outfd);
  close(outfd);

  printf("Successfully preprocessed %s\n", argv[1]);
}
