// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package gki

import (
	"errors"
	"fmt"
	"regexp"
)

type kmiVersion struct {
	version        string
	patchLevel     string
	androidRelease string
	kmiGeneration  string
}

var digits = "([0-9]+)"
var reKmi = regexp.MustCompile("^([0-9]+)[.]([0-9]+)-(android[0-9]+)-([0-9]+)$")

// Parse the given string as
func parseKmiVersion(s string) (*kmiVersion, error) {
	matches := reKmi.FindAllStringSubmatch(s, 4)

	if matches == nil {
		return nil, errors.New("Poorly formed KMI version: '" + s + "' must match '" + reKmi.String() + "'")
	}

	ret := kmiVersion{
		version:        matches[0][1],
		patchLevel:     matches[0][2],
		androidRelease: matches[0][3],
		kmiGeneration:  matches[0][4],
	}
	return &ret, nil
}

// 5.4-42-android11-0
func (k *kmiVersion) toString() string {
	return fmt.Sprintf("%s.%s-%s-%s", k.version, k.patchLevel, k.androidRelease, k.kmiGeneration)
}

// Sanitized string to be used as a suffix of APEX package name
// kmi_5_4_42_android11_0
func (k *kmiVersion) toPackageSuffix() string {
	return fmt.Sprintf("kmi_%s_%s_%s_%s", k.version, k.patchLevel, k.androidRelease, k.kmiGeneration)
}
