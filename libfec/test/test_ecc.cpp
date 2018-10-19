/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fec/io.h>

#include <iostream>
#include <memory>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " input" << std::endl;
    return 1;
  }

  fec::io input(argv[1]);
  if (!input) {
    return 1;
  }

  fec_ecc_metadata data;
  std::cout << "get_ecc_metadata: " << input.get_ecc_metadata(data)
            << std::endl;
  std::cout << "has_ecc: " << input.has_ecc() << std::endl;
  std::cout << "has_verity: " << input.has_verity() << std::endl;
  return 0;
}
