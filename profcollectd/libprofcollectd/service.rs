//
// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//! ProfCollect Binder service implementation.

use binder::{public_api::Result, Status};
use profcollectd_aidl_interface::aidl::com::android::server::profcollect::IProfCollectd::IProfCollectd;
use std::sync::{Mutex, MutexGuard};

use crate::config::Config;
use crate::scheduler::Scheduler;

fn err_msg_to_binder_status(_msg: &str) -> Status {
    // TODO: Forward the error message.
    Status::new_service_specific_error(1, None)
}

pub struct ProfcollectdBinderService {
    lock: Mutex<Lock>,
}

struct Lock {
    config: Config,
    scheduler: Scheduler,
}

impl binder::Interface for ProfcollectdBinderService {}

impl IProfCollectd for ProfcollectdBinderService {
    fn read_config(&self) -> Result<()> {
        self.lock().config = Config::read_config();
        Ok(())
    }
    fn schedule(&self) -> Result<()> {
        let lock = &mut *self.lock();
        lock.scheduler.schedule_periodic(&lock.config).map_err(err_msg_to_binder_status)
    }
    fn terminate(&self) -> Result<()> {
        self.lock().scheduler.terminate_periodic().map_err(err_msg_to_binder_status)
    }
    fn trace_once(&self, tag: &str) -> Result<()> {
        let lock = &mut *self.lock();
        lock.scheduler.one_shot(&lock.config, tag).map_err(err_msg_to_binder_status)
    }
    fn process(&self) -> Result<()> {
        let lock = &mut *self.lock();
        lock.scheduler.process(&lock.config);
        Ok(())
    }
    fn report(&self) -> Result<()> {
        Ok(())
    }
    fn get_supported_provider(&self) -> Result<String> {
        // TODO
        Ok(("None").to_string())
    }
}

impl ProfcollectdBinderService {
    pub fn new() -> std::result::Result<Self, &'static str> {
        let new_scheduler = Scheduler::new()?;
        Ok(ProfcollectdBinderService {
            lock: Mutex::new(Lock { scheduler: new_scheduler, config: Config::read_config() }),
        })
    }

    fn lock(&self) -> MutexGuard<Lock> {
        self.lock.lock().unwrap()
    }
}
