#![allow(dead_code)]

// bindgen generates FFI declarations for libprofcollectd.hpp
extern crate profcollectd_bindgen;

// Add safe Rust abstractions over the FFI functions.
pub fn init_service(start: bool) {
    unsafe {
        profcollectd_bindgen::InitService(start);
    }
}

pub fn schedule_collection() {
    unsafe {
        profcollectd_bindgen::ScheduleCollection();
    }
}

pub fn terminate_collection() {
    unsafe {
        profcollectd_bindgen::TerminateCollection();
    }
}

pub fn trace_once() {
    unsafe {
        profcollectd_bindgen::TraceOnce();
    }
}

pub fn process() {
    unsafe {
        profcollectd_bindgen::Process();
    }
}

pub fn read_config() {
    unsafe {
        profcollectd_bindgen::ReadConfig();
    }
}
