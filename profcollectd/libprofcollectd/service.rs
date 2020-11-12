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
    // FIXME: This is inefficient however putting them in a single Mutex hits issue in schedule(),
    // since scheduler is mut borrow while config is not.
    config: Mutex<Config>,
    scheduler: Mutex<Scheduler>,
}

impl binder::Interface for ProfcollectdBinderService {}

impl IProfCollectd for ProfcollectdBinderService {
    fn read_config(&self) -> Result<()> {
        *self.lock_config() = Config::read_config();
        Ok(())
    }
    fn schedule(&self) -> Result<()> {
        self.lock_scheduler()
            .schedule_periodic(&self.lock_config())
            .map_err(err_msg_to_binder_status)
    }
    fn terminate(&self) -> Result<()> {
        self.lock_scheduler().terminate_periodic().map_err(err_msg_to_binder_status)
    }
    fn trace_once(&self, tag: &str) -> Result<()> {
        self.lock_scheduler().one_shot(&self.lock_config(), tag).map_err(err_msg_to_binder_status)
    }
    fn process(&self) -> Result<()> {
        self.lock_scheduler().process(&self.lock_config());
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
            scheduler: Mutex::new(new_scheduler),
            config: Mutex::new(Config::read_config()),
        })
    }

    fn lock_scheduler(&self) -> MutexGuard<Scheduler> {
        self.scheduler.lock().unwrap()
    }

    fn lock_config(&self) -> MutexGuard<Config> {
        self.config.lock().unwrap()
    }
}
