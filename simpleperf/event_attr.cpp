/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "event_attr.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "event.h"

static const char* sample_format_names[64];  // perf_event_attr.sample_type has 64 bits.

static int Bit(uint64_t value) {
  uint64_t bit_value = 1;
  for (int i = 0; i < 64; ++i) {
    if (value == bit_value) {
      return i;
    }
    bit_value <<= 1;
  }
  return -1;
}

__attribute__((constructor)) static void init_sample_format_names() {
  sample_format_names[Bit(PERF_SAMPLE_IP)] = "ip";
  sample_format_names[Bit(PERF_SAMPLE_TID)] = "tid";
  sample_format_names[Bit(PERF_SAMPLE_TIME)] = "time";
  sample_format_names[Bit(PERF_SAMPLE_ADDR)] = "addr";
  sample_format_names[Bit(PERF_SAMPLE_READ)] = "read";
  sample_format_names[Bit(PERF_SAMPLE_CALLCHAIN)] = "callchain";
  sample_format_names[Bit(PERF_SAMPLE_ID)] = "id";
  sample_format_names[Bit(PERF_SAMPLE_CPU)] = "cpu";
  sample_format_names[Bit(PERF_SAMPLE_PERIOD)] = "period";
  sample_format_names[Bit(PERF_SAMPLE_STREAM_ID)] = "stream_id";
  sample_format_names[Bit(PERF_SAMPLE_RAW)] = "raw";
}

void EventAttr::Init(uint32_t type, uint64_t config, bool enabled) {
  memset(&attr, 0, sizeof(attr));
  attr.size = sizeof(perf_event_attr);
  attr.type = type;
  attr.config = config;
  attr.disabled = (enabled) ? 0 : 1;
  attr.mmap = 1;
  attr.comm = 1;
  attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_ID;
  attr.sample_type |= PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_PERIOD;
}

EventAttr::EventAttr(const Event* event, bool enabled) : event(event) {
  Init(event->Type(), event->Config(), enabled);
}

EventAttr::EventAttr(const perf_event_attr* attr) : attr(*attr) {
  event = Event::FindEventByConfig(attr->type, attr->config);
  if (event == nullptr) {
    fprintf(stderr, "Can't find event with type %u, config %llu\n", attr->type, attr->config);
  }
}

void EventAttr::Print(int space) const {
  printf("%*sevent_attr: %s\n", space, "", event->Name());
  printf("%*stype %u, size %u, config %llu\n", space + 2, "", attr.type, attr.size, attr.config);
  if (attr.freq) {
    printf("%*ssample_freq %llu\n", space + 2, "", attr.sample_freq);
  } else {
    printf("%*ssample_period %llu\n", space + 2, "", attr.sample_period);
  }

  std::string sample_format;
  for (size_t bit = 0; bit < sizeof(attr.sample_type) * 8; ++bit) {
    if ((attr.sample_type & (1 << bit)) != 0 && sample_format_names[bit] != nullptr) {
      if (sample_format.size() != 0) {
        sample_format += "|";
      }
      sample_format += sample_format_names[bit];
    }
  }
  printf("%*ssample_type (0x%llx) %s\n", space + 2, "", attr.sample_type, sample_format.c_str());
  printf("%*sread_format (0x%llx)\n", space + 2, "", attr.read_format);

  printf("%*sdisabled %llu, inherit %llu, pinned %llu, exclusive %llu\n",
         space + 2, "", attr.disabled, attr.inherit, attr.pinned, attr.exclusive);
  printf("%*sexclude_user %llu, exclude_kernel %llu, exclude_hv %llu\n",
         space + 2, "", attr.exclude_user, attr.exclude_kernel, attr.exclude_hv);
  printf("%*sexclude_idle %llu, mmap %llu, comm %llu, freq %llu\n",
         space + 2, "", attr.exclude_idle, attr.mmap, attr.comm, attr.freq);
  printf("%*sinherit_stat %llu, enable_on_exec %llu, task %llu\n",
         space + 2, "", attr.inherit_stat, attr.enable_on_exec, attr.task);
  printf("%*swatermark %llu, precise_ip %llu, mmap_data %llu\n",
         space + 2, "", attr.watermark, attr.precise_ip, attr.mmap_data);
  printf("%*ssample_id_all %llu, exclude_host %llu, exclude_guest %llu\n",
         space + 2, "", attr.sample_id_all, attr.exclude_host, attr.exclude_guest);
}
