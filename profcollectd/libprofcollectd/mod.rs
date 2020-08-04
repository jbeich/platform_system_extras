#![allow(dead_code)]

extern "C" {
    pub fn InitService(_: bool);
    pub fn ScheduleCollection();
    pub fn TerminateCollection();
    pub fn TraceOnce();
    pub fn Process();
    pub fn ReadConfig();
}

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
