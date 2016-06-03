/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <inttypes.h>
#include <stdio.h>

#include <memory>

#include "system/extras/simpleperf/report_sample.pb.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

namespace proto = simpleperf_report_proto;

bool ReadProtobufReport(const std::string& filename) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  std::unique_ptr<FILE, decltype(&fclose)> fp(fopen(filename.c_str(), "rb"),
                                              fclose);
  if (fp == nullptr) {
    fprintf(stderr, "failed to open %s\n", filename.c_str());
    return false;
  }
  google::protobuf::io::FileInputStream protobuf_file_is(fileno(fp.get()));
  google::protobuf::io::CodedInputStream coded_is(&protobuf_file_is);
  while (true) {
    uint32_t size;
    if (!coded_is.ReadLittleEndian32(&size)) {
      fprintf(stderr, "failed to read %s\n", filename.c_str());
      return false;
    }
    if (size == 0) {
      break;
    }
    auto limit = coded_is.PushLimit(size);
    proto::Record proto_record;
    if (!proto_record.ParseFromCodedStream(&coded_is)) {
      fprintf(stderr, "failed to read %s\n", filename.c_str());
      return false;
    }
    coded_is.PopLimit(limit);
    if (proto_record.type() != proto::Record_Type_SAMPLE) {
      fprintf(stderr, "unexpected record type %d\n", proto_record.type());
      return false;
    }
    auto& sample = proto_record.sample();
    static size_t sample_count = 0;
    printf("sample %zu:\n", ++sample_count);
    printf("  time: %" PRIu64 "\n", sample.time());
    printf("  callchain:\n");
    for (int j = 0; j < sample.callchain_size(); ++j) {
      const proto::Sample_CallChainEntry& callchain = sample.callchain(j);
      printf("    ip: %" PRIx64 "\n", callchain.ip());
      printf("    dso: %s\n", callchain.file().c_str());
      printf("    symbol: %s\n", callchain.symbol().c_str());
    }
  }
  google::protobuf::ShutdownProtobufLibrary();
  return true;
}

#if defined(BUILD_EXECUTABLE)

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s report_protobuf_file\n", argv[0]);
    return -1;
  }
  if (!ReadProtobufReport(argv[1])) {
    return -1;
  }
  return 0;
}

#endif
