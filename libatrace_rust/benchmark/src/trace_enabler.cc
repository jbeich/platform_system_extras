/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "trace_enabler.h"

#include <cutils/properties.h>
#include <cutils/trace.h>
#include <cstdlib>

void disable_app_atrace() {
    if (property_set("debug.atrace.app_number", "") < 0) {
        // TODO write error
        std::exit(1);
    }
    atrace_update_tags();
}

void enable_atrace_for_single_app(const char* name) {
    // TODO touch tracing_marker

    if (property_set("debug.atrace.app_number", "1") < 0) {
        // TODO write error
        std::exit(2);
    }
    if (property_set("debug.atrace.app_0", name) < 0) {
        // TODO write error
        std::exit(3);
    }
    atrace_update_tags();
}
