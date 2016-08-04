constexpr int LOOP_COUNT = 1000000000;

void Function1() {
  for (volatile int i = 0; i < LOOP_COUNT; ++i) {
  }
}

int main() {
  Function1();
  return 0;
}
