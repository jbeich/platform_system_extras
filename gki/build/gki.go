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
	"fmt"
	"path/filepath"
	"strings"

	"android/soong/android"
	"android/soong/apex"
	"android/soong/etc"
	"android/soong/genrule"
	"android/soong/phony"

	"github.com/google/blueprint/proptools"
)

type gkiApexProperties struct {
	// Path relative to $(PRODUCT_OUT) that points to the boot image. This is
	// passed to the generated makefile_goal.
	Product_out_path *string

	// Declared KMI version of the boot image. Example: "5.4-android12-0"
	Kmi_version *string

	// The certificate to sign the OTA payload.
	// The name of a certificate in the default certificate directory, blank to use the default product certificate,
	// or an android_app_certificate module name in the form ":module".
	Ota_payload_certificate *string
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
	kmiVersion, err := parseKmiVersion(proptools.String(g.properties.Kmi_version))
	if err != nil {
		mctx.PropertyErrorf("kmi_version", err.Error())
		return
	}
	if proptools.String(g.properties.Product_out_path) == "" {
		mctx.PropertyErrorf("product_out_path", "cannot be empty")
		return
	}

	// Check if modules should be generated at all according to board variables.
	if !g.bootImgHasRules(mctx) || !g.kmiVersionMatches(mctx) {
		return
	}

	// Import $(PRODUCT_OUT)/boot.img to Soong
	bootImage := g.apexModuleName() + "_bootimage"
	mctx.CreateModule(android.MakefileGoalFactory, &makefileGoalProperties{
		Name:             proptools.StringPtr(bootImage),
		Product_out_path: g.properties.Product_out_path,
	})
	// boot.img -> payload.bin and payload_properties.txt
	otaPayloadGen := g.apexModuleName() + "_ota_payload_gen"
	mctx.CreateModule(rawImageOtaFactory, &nameProperties{
		Name: proptools.StringPtr(otaPayloadGen),
	}, &rawImageOtaProperties{
		Certificate: g.properties.Ota_payload_certificate,
		Image_goals: []string{"boot:" + bootImage},
	})
	// copy payload.bin to <apex>/etc/ota
	otaPayload := g.apexModuleName() + "_ota_payload"
	mctx.CreateModule(etc.PrebuiltEtcFactory, &prebuiltEtcProperties{
		Name:                  proptools.StringPtr(otaPayload),
		Src:                   proptools.StringPtr(":" + otaPayloadGen + "{" + payloadTag + "}"),
		Filename_from_src:     proptools.BoolPtr(true),
		Relative_install_path: proptools.StringPtr("ota"),
		Installable:           proptools.BoolPtr(false),
	})
	// copy payload_properties.txt to <apex>/etc/ota
	otaProperties := g.apexModuleName() + "_ota_payload_properties"
	mctx.CreateModule(etc.PrebuiltEtcFactory, &prebuiltEtcProperties{
		Name:                  proptools.StringPtr(otaProperties),
		Src:                   proptools.StringPtr(":" + otaPayloadGen + "{" + payloadPropertiesTag + "}"),
		Filename_from_src:     proptools.BoolPtr(true),
		Relative_install_path: proptools.StringPtr("ota"),
		Installable:           proptools.BoolPtr(false),
	})
	// boot.img -> kernel_release.txt
	kernelReleaseFile := g.apexModuleName() + "_bootimage_kernel_release_file"
	mctx.CreateModule(genrule.GenRuleFactory, &genRuleProperties{
		Name:     proptools.StringPtr(kernelReleaseFile),
		Defaults: []string{"extract_kernel_release_defaults"},
		Srcs:     []string{":" + bootImage},
	})
	// kernel_release.txt -> apex_manifest.json
	apexManifest := g.apexModuleName() + "_apex_manifest"
	apexManifestCmd := createApexManifestCmd()
	mctx.CreateModule(genrule.GenRuleFactory, &genRuleProperties{
		Name:  proptools.StringPtr(apexManifest),
		Tools: []string{"jsonmodify"},
		Out:   []string{"apex_manifest.json"},
		Srcs:  []string{":" + kernelReleaseFile},
		Cmd:   proptools.StringPtr(apexManifestCmd),
	})
	// The APEX module.
	// TODO(b/162973356): move APEX to the correct partition. It does not belong to system.
	apexModuleName := g.apexModuleName()
	apexName := "com.android.gki." + kmiVersion.toPackageSuffix()
	mctx.CreateModule(apex.BundleFactory, &apexProperties{
		Name:      proptools.StringPtr(apexModuleName),
		Apex_name: proptools.StringPtr(apexName),
		Manifest:  proptools.StringPtr(":" + apexManifest),
		Defaults:  []string{"com.android.gki_defaults"},
		Prebuilts: []string{
			otaPayload,
			otaProperties,
		},
	})
	// Check kernel_release.txt against boot image.
	checkKmiVersion := g.moduleName() + "_check_kmi_version"
	checkKmiVersionCmd := createKmiVersionCheckCommand(kmiVersion, proptools.String(g.properties.Product_out_path))
	mctx.CreateModule(genrule.GenRuleFactory, &genRuleProperties{
		Name: proptools.StringPtr(checkKmiVersion),
		Out:  []string{checkKmiVersion + "-timestamp"},
		Srcs: []string{":" + kernelReleaseFile},
		Cmd:  proptools.StringPtr(checkKmiVersionCmd),
	})
	// Reserve the original module name for future use.
	// Required the APEX to be built and KMI version to be checked.
	mctx.CreateModule(phony.PhonyFactory, &phonyProperties{
		Name: proptools.StringPtr(g.moduleName()),
		Required: []string{
			apexModuleName,
			checkKmiVersion,
		},
	})
}

// Original module name as specified by the "name" property.
// This is also the prefix of names of all generated modules that the phony module depends on.
// e.g. com.android.gki.boot_5.4-android12-0
func (g *gkiApex) moduleName() string {
	return g.ModuleBase.Name()
}

// Name of the generated APEX module.
// This is also the prefix of names of all generated modules that the APEX module depends on.
// e.g. com.android.gki.boot_5.4-android12-0_apex
func (g *gkiApex) apexModuleName() string {
	return g.moduleName() + "_apex"
}

// The appeared name of this gkiApex object. Exposed to Soong to avoid conflicting with
// the generated phony module with name moduleName().
// e.g. com.android.gki.boot_5.4-android12-0_all
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

// Only generate if this module's kmi_version field is in BOARD_KERNEL_MODULE_INTERFACE_VERSIONS.
// Otherwise, this board does not support GKI APEXes, so no modules are generated at all.
// This function also avoids building invalid modules in checkbuild. For example, if these
// gki_apex modules are defined:
//   gki_apex { name: "boot-kmi-1", kmi_version: "1", product_out_path: "boot.img" }
//   gki_apex { name: "boot-kmi-2", kmi_version: "2", product_out_path: "boot.img" }
// But a given device's $PRODUCT_OUT/boot.img can only support at most one KMI version.
// Disable some modules accordingly to make sure checkbuild still works.
func (g *gkiApex) kmiVersionMatches(mctx android.EarlyModuleContext) bool {
	kmiVersions := mctx.DeviceConfig().BoardKernelModuleInterfaceVersions()
	return android.InList(proptools.String(g.properties.Kmi_version), kmiVersions)
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

// Create a command that checks KMI version from a kernel release file against kmiVersion.
func createKmiVersionCheckCommand(kmiVersion *kmiVersion, boot string) string {
	return fmt.Sprintf(
		`rm -rf $(out) && `+
			`if [[ $$(cat $(in)) =~ ^%[1]s[.]%[2]s[.][0-9]+-%[3]s-%[4]s([^0-9].*)?$$ ]]; then `+
			`  touch $(out); `+
			`else `+
			`  echo "ERROR: Kernel release of %[5]s is $$(cat $(in)), expect KMI version is %[1]s.%[2]s-%[3]s-%[4]s" && `+
			`  exit 1;`+
			`fi`, kmiVersion.version, kmiVersion.patchLevel, kmiVersion.androidRelease, kmiVersion.kmiGeneration, boot)
}

// Transform kernel release file in $(in) to KMI version + sublevel. Write APEX manifest JSON to
// $(out).
// e.g. 5.4.42-android11-0 => name: "com.android.gki.kmi_5_4_android11_0", version: "42"
// Note that the KMI version is read from the boot image, not from the statically defined
// value; this allows apexer to check the declared value in makefiles versus the actual value
// in boot image.
func createApexManifestCmd() string {
	return "echo '{}' | $(location jsonmodify) " +
		"-v name com.android.gki.kmi_$$(cat $(in) | sed -E 's/^([0-9]+[.][0-9]+)[.]([0-9]+)-(android[0-9]+)-([0-9]+).*$$/\\1_\\3_\\4/g') " +
		"-v version                  $$(cat $(in) | sed -E 's/^([0-9]+[.][0-9]+)[.]([0-9]+)-(android[0-9]+)-([0-9]+).*$$/\\2/g') " +
		"-v preInstallHook bin/com.android.gki.preinstall " +
		"-v postInstallHook bin/com.android.gki.postinstall " +
		"> $(out)"
}
