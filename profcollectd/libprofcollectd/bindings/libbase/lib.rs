//
// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//! This module implements safe wrappers for GetProperty method from libbase.

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("properties.hpp");
        fn GetProperty(key: &str, default_value: &str) -> String;
        fn SetProperty(key: &str, value: &str);
    }
}

/// Returns the current value of the system property `key`,
/// or `default_value` if the property is empty or doesn't exist.
pub fn get_property(key: &str, default_value: &str) -> String {
    ffi::GetProperty(key, default_value)
}

/// Sets the system property `key` to `value`.
pub fn set_property(key: &str, value: &str) {
    ffi::SetProperty(key, value);
}
