#![allow(dead_code)]

// bindgen generates FFI declarations for libprofcollectd.hpp
include!(concat!(env!("OUT_DIR"), "/libprofcollectd_ffi.rs"));

// Add safe Rust abstractions over the FFI functions.
pub fn init_service(start: bool) {
    unsafe {
        InitService(start);
    }
}

pub fn schedule_collection() {
    unsafe {
        ScheduleCollection();
    }
}

pub fn terminate_collection() {
    unsafe {
        TerminateCollection();
    }
}

pub fn trace_once() {
    unsafe {
        TraceOnce();
    }
}

pub fn process() {
    unsafe {
        Process();
    }
}

pub fn read_config() {
    unsafe {
        ReadConfig();
    }
}
