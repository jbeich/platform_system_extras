size_t getPssKb() {
  FILE* smaps = fopen("/proc/self/smaps", "r");
  if (smaps == NULL) {
    perror("");
    exit(1);
  }
  char buffer[4096];
  size_t total = 0;
  while (fgets(buffer, sizeof(buffer), smaps) != NULL) {
    if (strncmp(buffer, "Pss:", 4) == 0) {
      total += atol(buffer + 4);
    }
  }
  fclose(smaps);
  return total;
}
