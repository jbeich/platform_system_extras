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
	"android/soong/android"
	"android/soong/apex"
	"android/soong/etc"
	"android/soong/genrule"
	"android/soong/phony"

	"github.com/google/blueprint"
	"github.com/google/blueprint/proptools"
)

type dependencyTag struct {
	blueprint.BaseDependencyTag
	name string
}

var (
	gkiApexSuffix    = "_gki"
	kernelReleaseTag = dependencyTag{name: "kernel_release"}

	pctx = android.NewPackageContext("android/gki")

	checkKmiVersionRule = pctx.StaticRule("checkKmiVersionRule", blueprint.RuleParams{
		Command: `[[ $$(cat ${in}) =~ '^${version}[.]${patchLevel}[.][0-9]+-${androidRelease}-${kmiGeneration}([^0-9].*)?$$' ]] &&` +
			`touch ${out} # ${in}`,
		Description: "check KMI version is ${version}.${patchLevel}-${androidRelease}-${kmiGeneration} in ${in}",
	}, "version", "patchLevel", "androidRelease", "kmiGeneration")
)

type gkiApexProperties struct {
	// Path relative to $(PRODUCT_OUT) that points to the boot image. This is
	// passed to the generated makefile_goal.
	Product_out_path *string

	// Declared KMI version of the boot image. Example: "5.4-android12-0"
	Kmi_version *string

	Overrides []string
}

type gkiApexRule struct {
	android.ModuleBase
	properties gkiApexProperties

	kmiVersion *kmiVersion
}

func init() {
	android.RegisterModuleType("gki_apex", gkiApexFactory)
}

// Declare a GKI APEX. Generate a set of modules to define an apex with name
// "com.android.gki" + sanitized(kmi_version).
func gkiApexFactory() android.Module {
	g := &gkiApexRule{}
	g.AddProperties(&g.properties)
	android.InitAndroidModule(g)
	android.AddLoadHook(g, func(ctx android.LoadHookContext) { gkiApexMutator(ctx, g) })
	return g
}

func gkiApexMutator(mctx android.LoadHookContext, g *gkiApexRule) {
	if err := g.computeKmiVersion(); err != nil {
		mctx.PropertyErrorf("kmi_version", err.Error())
		return
	}
	// Import $(PRODUCT_OUT)/%boot%.img to Soong
	mctx.CreateModule(android.MakefileGoalFactory, &makefileGoalProperties{
		Name:             proptools.StringPtr(g.apexModuleName() + "_bootimage"),
		Product_out_path: g.properties.Product_out_path,
	})
	// %boot%.img -> payload.bin
	mctx.CreateModule(genrule.GenRuleFactory, &genRuleProperties{
		Name:     proptools.StringPtr(g.apexModuleName() + "_ota_payload_gen"),
		Defaults: []string{"boot_img_to_ota_payload_defaults"},
		Srcs:     []string{":" + g.apexModuleName() + "_bootimage"},
	})
	// copy payload.bin to <apex>/etc/ota
	mctx.CreateModule(etc.PrebuiltEtcFactory, &prebuiltEtcProperties{
		Name:                  proptools.StringPtr(g.apexModuleName() + "_ota_payload"),
		Src:                   proptools.StringPtr(":" + g.apexModuleName() + "_ota_payload_gen"),
		Filename_from_src:     proptools.BoolPtr(true),
		Relative_install_path: proptools.StringPtr("ota"),
	})
	// payload.bin -> payload_properties.txt
	mctx.CreateModule(genrule.GenRuleFactory, &genRuleProperties{
		Name:     proptools.StringPtr(g.apexModuleName() + "_payload_properties_gen"),
		Defaults: []string{"ota_payload_to_properties_defaults"},
		Srcs:     []string{":" + g.apexModuleName() + "_ota_payload_gen"},
	})
	// copy payload_properties.txt to <apex>/etc/ota
	mctx.CreateModule(etc.PrebuiltEtcFactory, &prebuiltEtcProperties{
		Name:                  proptools.StringPtr(g.apexModuleName() + "_ota_payload_properties"),
		Src:                   proptools.StringPtr(":" + g.apexModuleName() + "_payload_properties_gen"),
		Filename_from_src:     proptools.BoolPtr(true),
		Relative_install_path: proptools.StringPtr("ota"),
	})
	// %boot%.img -> kernel_release.txt
	mctx.CreateModule(genrule.GenRuleFactory, &genRuleProperties{
		Name:     proptools.StringPtr(g.apexModuleName() + "_bootimage_kernel_release_file"),
		Defaults: []string{"extract_kernel_release_defaults"},
		Srcs:     []string{":" + g.apexModuleName() + "_bootimage"},
	})
	// kernel_release.txt -> apex_manifest.json
	mctx.CreateModule(genrule.GenRuleFactory, &genRuleProperties{
		Name:     proptools.StringPtr(g.apexModuleName() + "_apex_manifest"),
		Defaults: []string{"com.android.gki_apex_manifest_defaults"},
		Srcs:     []string{":" + g.apexModuleName() + "_bootimage_kernel_release_file"},
	})
	// The APEX module.
	apexName := "com.android.gki." + g.kmiVersion.toPackageSuffix()
	mctx.CreateModule(apex.BundleFactory, &apexProperties{
		Name:      proptools.StringPtr(g.apexModuleName()),
		Apex_name: proptools.StringPtr(apexName),
		Manifest:  proptools.StringPtr(":" + g.apexModuleName() + "_apex_manifest"),
		Defaults:  []string{"com.android.gki_defaults"},
		Prebuilts: []string{
			g.apexModuleName() + "_ota_payload",
			g.apexModuleName() + "_ota_payload_properties",
		},
		Overrides: addSuffix(g.properties.Overrides, gkiApexSuffix),
	})
	// Reserve the original module name for future use. Required the APEX and module named Name() to be built.
	mctx.CreateModule(phony.PhonyFactory, &phonyProperties{
		Name: proptools.StringPtr(g.moduleName()),
		Required: []string{
			g.apexModuleName(),
			g.Name(),
		},
	})
}

func addSuffix(list []string, suffix string) []string {
	for i := range list {
		list[i] = list[i] + suffix
	}
	return list
}

// Original module name as specified by the "Name" property,
// e.g. boot_5.4-android12-0
func (g *gkiApexRule) moduleName() string {
	return g.ModuleBase.Name()
}

// Name of the generated APEX module. This is also the prefix of names of all generated modules
// except the phony module.
// e.g. boot_5.4-android12-0_gki
func (g *gkiApexRule) apexModuleName() string {
	return g.moduleName() + gkiApexSuffix
}

// The appeared name of this gkiApexRule object. Named "_check" because it does additional checks;
// see GenerateAndroidBuildActions.
// e.g. boot_5.4-android12-0_gki_check
func (g *gkiApexRule) Name() string {
	return g.apexModuleName() + "_check"
}

func (g *gkiApexRule) computeKmiVersion() error {
	if g.kmiVersion != nil {
		return nil
	}
	kmiVersion, err := parseKmiVersion(proptools.String(g.properties.Kmi_version))
	if err != nil {
		return err
	}
	g.kmiVersion = kmiVersion
	return nil
}

func (g *gkiApexRule) DepsMutator(ctx android.BottomUpMutatorContext) {
	ctx.AddDependency(ctx.Module(), kernelReleaseTag, g.apexModuleName()+"_bootimage_kernel_release_file")
}

func (g *gkiApexRule) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	if err := g.computeKmiVersion(); err != nil {
		ctx.PropertyErrorf("kmi_version", err.Error())
		return
	}
	ctx.VisitDirectDepsWithTag(kernelReleaseTag, func(module android.Module) {
		genrule, ok := module.(*genrule.Module)
		if !ok {
			panic("Generated " + module.Name() + " is not a genrule!")
		}
		if len(genrule.GeneratedSourceFiles()) != 1 {
			panic("Generated " + module.Name() + " generates more than one file!")
		}

		checkKmiVersionTimestamp := android.PathForModuleGen(ctx, module.Name()+"-timestamp")
		ctx.Build(pctx, android.BuildParams{
			Rule:   checkKmiVersionRule,
			Output: checkKmiVersionTimestamp,
			Input:  genrule.GeneratedSourceFiles()[0],
			Args: map[string]string{
				"version":        g.kmiVersion.version,
				"patchLevel":     g.kmiVersion.patchLevel,
				"androidRelease": g.kmiVersion.androidRelease,
				"kmiGeneration":  g.kmiVersion.kmiGeneration,
			},
		})
	})
}
