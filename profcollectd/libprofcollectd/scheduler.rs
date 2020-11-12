//
// Copyright (C) 2022 The Android Open Source Project
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

//! ProfCollect tracing scheduler.

use std::sync::mpsc::{SyncSender, sync_channel};
use std::thread;
use std::sync::Arc;

use crate::config::Config;
use crate::trace_provider::{self, TraceProvider};

pub struct Scheduler {
    termination_ch : Option<SyncSender<()>>,
    trace_provider: Arc<dyn TraceProvider + Send + Sync>,
}

impl Scheduler {
    pub fn new() -> Result<Self, &'static str> {
        let new_trace_provider = trace_provider::get_trace_provider()?;
        Ok(Scheduler {
            termination_ch: None,
            trace_provider: new_trace_provider,
        })
    }

    fn is_scheduled(&self) -> bool {
        self.termination_ch.is_some()
    }

    pub fn schedule_periodic(&mut self, config : &Config) -> Result<(), &'static str> {
        if self.is_scheduled() {
            return Err("Already scheduled.")
        }

        let (sender, receiver) = sync_channel(1);
        self.termination_ch = Some(sender);

        // Copy value of config.
        let config = config.clone();
        let trace_provider = self.trace_provider.clone();

        thread::spawn(move || {
            loop {
                match receiver.recv_timeout(config.collection_interval) {
                    Ok(()) => break,
                    Err(_) => {
                        // Did not receive termination signal, initiate trace event.
                        println!("TRACE!");
                        trace_provider.trace(&config.trace_output_dir, "periodic", &config.sampling_period);
                    },
                }
            }
        });

        Ok(())
    }

    pub fn terminate_periodic(&mut self) -> Result<(), &str> {
        if !self.is_scheduled() {
            return Err("Not scheduled.")
        }

        self.termination_ch.as_ref().unwrap().send(()).map_err(|_| "Scheduler worker disappeared.")?;
        self.termination_ch = None;
        Ok(())
    }

    pub fn one_shot(&self, config : &Config, tag : &str) -> Result<(), &str> {
        let trace_provider = self.trace_provider.clone();
        trace_provider.trace(&config.trace_output_dir, tag, &config.sampling_period);
        Ok(())
    }
}