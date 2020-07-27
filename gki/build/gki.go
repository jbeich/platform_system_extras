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
	"path/filepath"
	"strings"

	"android/soong/android"
	"android/soong/apex"
	"android/soong/etc"
	"android/soong/genrule"

	"github.com/google/blueprint/proptools"
)

// The APEX version for a factory GKI APEX package.
const factoryVersion = 1

type gkiApexProperties struct {
	// Whether this is a factory APEX. A factory APEX has version 0 and no OTA payload.
	// Exactly one of [factory, product_out_path] must be set.
	Factory *bool

	// Path relative to $(PRODUCT_OUT) that points to the boot image. This is
	// passed to the generated makefile_goal.
	// Exactly one of [factory, product_out_path] must be set.
	Product_out_path *string

	// Declared KMI version of the boot image. Example: "5.4-android12-0"
	Kmi_version *string

	// The certificate to sign the OTA payload.
	// The name of a certificate in the default certificate directory, blank to use the default product certificate,
	// or an android_app_certificate module name in the form ":module".
	Ota_payload_certificate *string

	// Whether modules should be enabled according to board variables.
	ModulesEnabled bool `blueprint:"mutated"`
    // APEX package name that will be declared in the APEX manifest.
    // e.g. com.android.gki.kmi_5_4_android12_0
	ApexName *string `blueprint:"mutated"`
}

type gkiApex struct {
	android.ModuleBase
	properties gkiApexProperties
}

func init() {
	android.RegisterModuleType("gki_apex", gkiApexFactory)
}

// Declare a GKI APEX. Generate a set of modules to define an apex with name
// "com.android.gki" + sanitized(kmi_version).
func gkiApexFactory() android.Module {
	g := &gkiApex{}
	g.AddProperties(&g.properties)
	android.InitAndroidModule(g)
	android.AddLoadHook(g, func(ctx android.LoadHookContext) { gkiApexMutator(ctx, g) })
	return g
}

func gkiApexMutator(mctx android.LoadHookContext, g *gkiApex) {
	g.validateAndSetMutableProperties(mctx)
	if proptools.Bool(g.properties.Factory) {
		g.createModulesForFactoryApex(mctx)
	} else {
		g.createModulesRealApex(mctx)
	}
}

func (g *gkiApex) validateAndSetMutableProperties(mctx android.LoadHookContext) {
	// Check consistency between factory and product_out_path properties.
	declaredFactory := proptools.Bool(g.properties.Factory)
	declaredBootImage := proptools.String(g.properties.Product_out_path) != ""
	if declaredFactory == declaredBootImage {
		mctx.ModuleErrorf("Must set exactly one of factory or product_out_path")
		return
	}

	// Parse kmi_version property to find APEX name.
	apexName, err := kmiVersionToApexName(proptools.String(g.properties.Kmi_version))
	if err != nil {
		mctx.PropertyErrorf("kmi_version", err.Error())
		return
	}

	// For factory APEX package, check that name aligns with apexName calculated from kmi_version.
	if declaredFactory && g.moduleName() != apexName {
		mctx.PropertyErrorf("name", "Name of factory GKI APEX for KMI version %s must be %s",
			proptools.String(g.properties.Kmi_version), apexName)
		return
	}

	// Set mutable properties.
	g.properties.ModulesEnabled = (declaredFactory || g.bootImgHasRules(mctx)) && g.boardDefinesKmiVersion(mctx)
	g.properties.ApexName = proptools.StringPtr(apexName)
}

// Create modules for a factory APEX package that does not contain the OTA payload.
// It has version |factoryVersion|. It cannot be installed via updates because the preinstall hook
// requires the OTA payload to exist.
func (g *gkiApex) createModulesForFactoryApex(mctx android.LoadHookContext) {
	// kmi_version property -> apex_manifest.json
	apexManifest := g.moduleName() + "_apex_manifest"
	mctx.CreateModule(genrule.GenRuleFactory, &moduleCommonProperties{
		Name:    proptools.StringPtr(apexManifest),
		Enabled: proptools.BoolPtr(g.properties.ModulesEnabled),
	}, &genRuleProperties{
		Tools: []string{"build_gki_apex_manifest"},
		Out:   []string{"apex_manifest.json"},
		Cmd:   proptools.StringPtr(g.createApexManifestCmd()),
	})
	// The APEX module. Installed to system. It does not contain any OTA payload.
	mctx.CreateModule(apex.BundleFactory, &moduleCommonProperties{
		Name:    proptools.StringPtr(g.moduleName()),
		Enabled: proptools.BoolPtr(g.properties.ModulesEnabled),
	}, &apexProperties{
		// Explicitly set APEX name to suppress key name checks.
		Apex_name:   g.properties.ApexName,
		Manifest:    proptools.StringPtr(":" + apexManifest),
		Defaults:    []string{"com.android.gki_defaults"},
		Installable: proptools.BoolPtr(true),
	})
}

// Create modules for a real APEX package that contains an OTA payload.
func (g *gkiApex) createModulesRealApex(mctx android.LoadHookContext) {
	// Import $(PRODUCT_OUT)/boot.img to Soong
	bootImage := g.moduleName() + "_bootimage"
	mctx.CreateModule(android.MakefileGoalFactory, &moduleCommonProperties{
		Name:    proptools.StringPtr(bootImage),
		Enabled: proptools.BoolPtr(g.properties.ModulesEnabled),
	}, &makefileGoalProperties{
		Product_out_path: g.properties.Product_out_path,
	})
	// boot.img -> kernel_release.txt
	kernelReleaseFile := g.moduleName() + "_bootimage_kernel_release_file"
	mctx.CreateModule(genrule.GenRuleFactory, &moduleCommonProperties{
		Name:    proptools.StringPtr(kernelReleaseFile),
		Enabled: proptools.BoolPtr(g.properties.ModulesEnabled),
	}, &genRuleProperties{
		Defaults: []string{"extract_kernel_release_defaults"},
		Srcs:     []string{":" + bootImage},
	})
	// boot.img + kernel_release.txt -> payload.bin and payload_properties.txt
	otaPayloadGen := g.moduleName() + "_ota_payload_gen"
	mctx.CreateModule(rawImageOtaFactory, &moduleCommonProperties{
		Name:    proptools.StringPtr(otaPayloadGen),
		Enabled: proptools.BoolPtr(g.properties.ModulesEnabled),
	}, &rawImageOtaProperties{
		Certificate:    g.properties.Ota_payload_certificate,
		Image_goals:    []string{"boot:" + bootImage},
		Kernel_release: proptools.StringPtr(":" + kernelReleaseFile),
	})
	// copy payload.bin to <apex>/etc/ota
	otaPayload := g.moduleName() + "_ota_payload"
	mctx.CreateModule(etc.PrebuiltEtcFactory, &moduleCommonProperties{
		Name:    proptools.StringPtr(otaPayload),
		Enabled: proptools.BoolPtr(g.properties.ModulesEnabled),
	}, &prebuiltEtcProperties{
		Src:                   proptools.StringPtr(":" + otaPayloadGen + "{" + payloadTag + "}"),
		Filename_from_src:     proptools.BoolPtr(true),
		Relative_install_path: proptools.StringPtr("ota"),
		Installable:           proptools.BoolPtr(false),
	})
	// copy payload_properties.txt to <apex>/etc/ota
	otaProperties := g.moduleName() + "_ota_payload_properties"
	mctx.CreateModule(etc.PrebuiltEtcFactory, &moduleCommonProperties{
		Name:    proptools.StringPtr(otaProperties),
		Enabled: proptools.BoolPtr(g.properties.ModulesEnabled),
	}, &prebuiltEtcProperties{
		Src:                   proptools.StringPtr(":" + otaPayloadGen + "{" + payloadPropertiesTag + "}"),
		Filename_from_src:     proptools.BoolPtr(true),
		Relative_install_path: proptools.StringPtr("ota"),
		Installable:           proptools.BoolPtr(false),
	})
	// Check kmi_version property against kernel_release.txt, then
	// kernel_release.txt -> apex_manifest.json.
	apexManifest := g.moduleName() + "_apex_manifest"
	mctx.CreateModule(genrule.GenRuleFactory, &moduleCommonProperties{
		Name:    proptools.StringPtr(apexManifest),
		Enabled: proptools.BoolPtr(g.properties.ModulesEnabled),
	}, &genRuleProperties{
		Tools: []string{"build_gki_apex_manifest"},
		Out:   []string{"apex_manifest.json"},
		Srcs:  []string{":" + kernelReleaseFile},
		Cmd:   proptools.StringPtr(g.createApexManifestCmd()),
	})
	// The APEX module.
	mctx.CreateModule(apex.BundleFactory, &moduleCommonProperties{
		Name:    proptools.StringPtr(g.moduleName()),
		Enabled: proptools.BoolPtr(g.properties.ModulesEnabled),
	}, &apexProperties{
		Apex_name: g.properties.ApexName,
		Manifest:  proptools.StringPtr(":" + apexManifest),
		Defaults:  []string{"com.android.gki_defaults"},
		// A real GKI APEX cannot be preinstalled to the device.
		// It can only be provided as an update.
		Installable: proptools.BoolPtr(false),
		Prebuilts: []string{
			otaPayload,
			otaProperties,
		},
	})
}

// Original module name as specified by the "name" property.
// This is also the APEX module name, i.e. the file name of the APEX file.
// This is also the prefix of names of all generated modules that the phony module depends on.
// e.g. com.android.gki.kmi_5_4_android12_0_boot
func (g *gkiApex) moduleName() string {
	return g.ModuleBase.Name()
}

// The appeared name of this gkiApex object. Exposed to Soong to avoid conflicting with
// the generated APEX module with name moduleName().
// e.g. com.android.gki.kmi_5_4_android12_0_boot_all
func (g *gkiApex) Name() string {
	return g.moduleName() + "_all"
}

// If the boot image pointed at product_out_path has no rule to be generated, do not generate any
// build rules for this gki_apex module. For example, if this gki_apex module is:
//     { name: "foo", product_out_path: "boot-bar.img" }
// But there is no rule to generate boot-bar.img, then
// - `m foo` fails with `unknown target 'foo'`
// - checkbuild is still successful. The module foo doesn't even exist, so there
//   is no dependency on boot-bar.img
//
// There is a rule to generate "boot-foo.img" if "kernel-foo" is in BOARD_KERNEL_BINARIES.
// As a special case, there is a rule to generate "boot.img" if BOARD_KERNEL_BINARIES is empty,
// or "kernel" is in BOARD_KERNEL_BINARIES.
func (g *gkiApex) bootImgHasRules(mctx android.EarlyModuleContext) bool {
	kernelNames := mctx.DeviceConfig().BoardKernelBinaries()
	if len(kernelNames) == 0 {
		return proptools.String(g.properties.Product_out_path) == "boot.img"
	}
	for _, kernelName := range kernelNames {
		validBootImagePath := strings.Replace(kernelName, "kernel", "boot", -1) + ".img"
		if proptools.String(g.properties.Product_out_path) == validBootImagePath {
			return true
		}
	}
	return false
}

// Only generate if this module's kmi_version property is in BOARD_KERNEL_MODULE_INTERFACE_VERSIONS.
// Otherwise, this board does not support GKI APEXes, so no modules are generated at all.
// This function also avoids building invalid modules in checkbuild. For example, if these
// gki_apex modules are defined:
//   gki_apex { name: "boot-kmi-1", kmi_version: "1", product_out_path: "boot.img" }
//   gki_apex { name: "boot-kmi-2", kmi_version: "2", product_out_path: "boot.img" }
// But a given device's $PRODUCT_OUT/boot.img can only support at most one KMI version.
// Disable some modules accordingly to make sure checkbuild still works.
func (g *gkiApex) boardDefinesKmiVersion(mctx android.EarlyModuleContext) bool {
	kmiVersions := mctx.DeviceConfig().BoardKernelModuleInterfaceVersions()
	return android.InList(proptools.String(g.properties.Kmi_version), kmiVersions)
}

// - For factory GKI APEX, write APEX manifest JSON to $(out) for factory APEX.
//   e.g. 5.4-android12-0 => name: "com.android.gki.kmi_5_4_android12_0", version: factory version.
//
// - For real GKI APEX, transform kernel release file in $(in) to KMI version + sublevel.
//   e.g. 5.4.42-android12-0 => name: "com.android.gki.kmi_5_4_android12_0", version: "42"
//
// Finally, write APEX manifest JSON to $(out).
func (g *gkiApex) createApexManifestCmd() string {
	ret := `$(location build_gki_apex_manifest) ` +
		`--kmi_version "` + proptools.String(g.properties.Kmi_version) + `" ` +
		`--apex_manifest $(out) `
	// For factory APEX, no boot image is provided, hence there is no kernel release file.
	// build_gki_apex_manifest uses kmi_version argument and infer factory APEX version.
	if proptools.Bool(g.properties.Factory) {
		ret += `--factory`
	} else {
		ret += `--kernel_release_file $(in)`
	}
	return ret
}

func (g *gkiApex) DepsMutator(ctx android.BottomUpMutatorContext) {
}

func (g *gkiApex) GenerateAndroidBuildActions(ctx android.ModuleContext) {
}

// OTA payload binary is signed with default_system_dev_certificate, which is equivalent to
// DefaultAppCertificate().
func getDefaultCertificate(ctx android.EarlyModuleContext) string {
	pem, _ := ctx.Config().DefaultAppCertificate(ctx)
	return strings.TrimSuffix(pem.String(), filepath.Ext(pem.String()))
}
