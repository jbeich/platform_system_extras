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

type makefileGoalProperties struct {
	Name             *string
	Product_out_path *string
}

type genRuleProperties struct {
	Name     *string
	Defaults []string
	Srcs     []string
}

type prebuiltEtcProperties struct {
	Name                  *string
	Src                   *string
	Filename_from_src     *bool
	Relative_install_path *string
	Installable           *bool
}

type apexProperties struct {
	Name      *string
	Apex_name *string
	Manifest  *string
	Defaults  []string
	Prebuilts []string
	Overrides []string
}
type phonyProperties struct {
	Name     *string
	Required []string
}
