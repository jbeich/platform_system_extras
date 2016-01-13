/*
 * Copyright (C) 2014 The Android Open Source Project
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

#define _LARGEFILE64_SOURCE

#include <stdio.h>

#include <dlfcn.h>
#include <assert.h>

#include <f2fs_fs.h>
#include <f2fs_format_utils.h>
#if defined(__linux__)
#define F2FS_DYN_LIB "libf2fs_fmt_host_dyn.so"
#elif defined(__APPLE__) && defined(__MACH__)
#define F2FS_DYN_LIB "libf2fs_fmt_host_dyn.dylib"
#else
#ifdef USE_MINGW
#define F2FS_DYN_LIB "libf2fs_fmt_host_dyn.dll"
#else
#error Unsupported platform
#endif
#endif

int (*f2fs_format_device_dl)(void);
void (*f2fs_init_configuration_dl)(struct f2fs_configuration *);
// int (*f2fs_assign_config_ptr_dl)(struct f2fs_configuration* );
struct f2fs_configuration* (*f2fs_assign_config_ptr_dl)();
void (*flush_sparse_buffs_dl)(void);
int (*assign_f2fs_sparse_file_ptr_dl)(void* );

int f2fs_format_device(void) {
	assert(f2fs_format_device_dl);
	return f2fs_format_device_dl();
}
void f2fs_init_configuration(struct f2fs_configuration *config) {
	assert(f2fs_init_configuration_dl);
	f2fs_init_configuration_dl(config);
}
/* int f2fs_assign_config_ptr(struct f2fs_configuration *config) {
	assert(f2fs_assign_config_ptr_dl);
	return f2fs_assign_config_ptr_dl(config);
} */
struct f2fs_configuration* f2fs_assign_config_ptr() {
	assert(f2fs_assign_config_ptr_dl);
	return f2fs_assign_config_ptr_dl();
}

void flush_sparse_buffs(){
	assert(flush_sparse_buffs_dl);
	flush_sparse_buffs_dl();
}
int assign_f2fs_sparse_file_ptr(void* sparse_file_ptr) {
	assert(assign_f2fs_sparse_file_ptr_dl);
	return assign_f2fs_sparse_file_ptr_dl(sparse_file_ptr);
}

void* dlopenf2fs() {
	void* f2fs_lib = NULL;

	f2fs_lib = dlopen(F2FS_DYN_LIB, RTLD_NOW | RTLD_LOCAL);
	if (!f2fs_lib) {
		printf("f2fs_lib is NULL \n");
		return NULL;
	}
	f2fs_format_device_dl = dlsym(f2fs_lib, "f2fs_format_device");
	f2fs_init_configuration_dl = dlsym(f2fs_lib, "f2fs_init_configuration");
	f2fs_assign_config_ptr_dl = dlsym(f2fs_lib, "f2fs_assign_config_ptr");
	flush_sparse_buffs_dl = dlsym(f2fs_lib, "flush_sparse_buffs");
	assign_f2fs_sparse_file_ptr_dl = dlsym(f2fs_lib, "assign_f2fs_sparse_file_ptr");
	if (
		!f2fs_format_device_dl || 
		!f2fs_init_configuration_dl || 
		!f2fs_assign_config_ptr_dl ||
		!flush_sparse_buffs_dl ||
		!assign_f2fs_sparse_file_ptr_dl
		) {
		printf("One of the dynamic lib fcn pointers is NULL \n");
		return NULL;
	}
	return f2fs_lib;
}

int dlclosef2fs(void* h){
	if(!h){
		printf("cannot close dl handle. h is NULL \n");
		return -1;
	}
	return dlclose(h);
}
