// Copyright 2020 Google Inc. All rights reserved.
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

// makefile_goal imports a Makefile goal to Soong by copying the file built by
// the goal to a path visible to Soong.

package etc

import (
	"fmt"
	"io"
	"path/filepath"

	"github.com/google/blueprint/proptools"

	"android/soong/android"
)

var pctx = android.NewPackageContext("android/soong/makefile")

func init() {
	android.RegisterModuleType("makefile_goal", makefileGoalFactory)
}

type makefileGoalProperties struct {
	// Sources.
	// Makefile goal output file path, relative to PRODUCT_OUT.
	Product_out_path *string
}

type makefileGoal struct {
	android.ModuleBase

	properties makefileGoalProperties

	// Destination. Output file path of this module.
	outputFilePath android.OutputPath
}

var _ android.AndroidMkEntriesProvider = (*makefileGoal)(nil)
var _ android.OutputFileProducer = (*makefileGoal)(nil)

// Input file of this makefile_goal module. Nil if none specified. May use variable names in makefiles.
func (p *makefileGoal) inputPath() *string {
	if p.properties.Product_out_path != nil {
		return proptools.StringPtr(filepath.Join("$(PRODUCT_OUT)", proptools.String(p.properties.Product_out_path)))
	}
	return nil
}

// OutputFileProducer
func (p *makefileGoal) OutputFiles(tag string) (android.Paths, error) {
	if tag != "" {
		return nil, fmt.Errorf("unsupported tag %q", tag)
	}
	return android.Paths{p.outputFilePath}, nil
}

// AndroidMkEntriesProvider
func (p *makefileGoal) DepsMutator(ctx android.BottomUpMutatorContext) {
	if p.inputPath() == nil {
		ctx.PropertyErrorf("product_out_path", "Path relative to PRODUCT_OUT required")
	}
}

func (p *makefileGoal) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	filename := filepath.Base(proptools.String(p.inputPath()))
	p.outputFilePath = android.PathForModuleOut(ctx, filename).OutputPath

	ctx.InstallFile(android.PathForModuleInstall(ctx, "etc"), ctx.ModuleName(), p.outputFilePath)
}

func (p *makefileGoal) AndroidMkEntries() []android.AndroidMkEntries {
	return []android.AndroidMkEntries{android.AndroidMkEntries{
		Class:      "ETC",
		OutputFile: android.OptionalPathForPath(p.outputFilePath),
		ExtraFooters: []android.AndroidMkExtraFootersFunc{
			func(w io.Writer, name, prefix, moduleDir string, entries *android.AndroidMkEntries) {
				// Can't use android.Cp because inputPath() is not a valid android.Path.
				fmt.Fprintf(w, "$(eval $(call copy-one-file,%s,%s))\n", proptools.String(p.inputPath()), p.outputFilePath)
			},
		},
	}}
}

func makefileGoalFactory() android.Module {
	module := &makefileGoal{}
	module.AddProperties(&module.properties)
	// This module is device-only
	android.InitAndroidArchModule(module, android.DeviceSupported, android.MultilibFirst)
	return module
}
