// Copyright 2023 Google LLC

//! Tracing-subscriber layer for libatrace_rust.

use atrace::AtraceTag;
use tracing::{Event, Id, Subscriber};
use tracing_subscriber::layer::{Context, Layer};
use tracing_subscriber::registry::LookupSpan;

use tracing::span::Attributes;
use tracing::span::Record;
use tracing_subscriber::field::Visit;

/// Subscriber layer that forwards events to ATrace.
pub struct AtraceSubscriber {
    tag: AtraceTag,
    should_record_fields: bool,
}

impl Default for AtraceSubscriber {
    fn default() -> Self {
        Self::new(AtraceTag::App)
    }
}

impl AtraceSubscriber {
    /// Makes a new subscriber with tag.
    pub fn new(tag: AtraceTag) -> AtraceSubscriber {
        AtraceSubscriber { tag, should_record_fields: false }
    }

    /// Enables recording of field values.
    pub fn with_fields(self) -> AtraceSubscriber {
        AtraceSubscriber { tag: self.tag, should_record_fields: true }
    }
}

struct FieldFormatter {
    is_event: bool,
    s: String,
}

impl FieldFormatter {
    fn for_event() -> FieldFormatter {
        FieldFormatter { is_event: true, s: String::new() }
    }
    fn for_span() -> FieldFormatter {
        FieldFormatter { is_event: false, s: String::new() }
    }
}

impl FieldFormatter {
    fn as_string(&self) -> &String {
        &self.s
    }
    fn is_empty(&self) -> bool {
        self.s.is_empty()
    }
}

impl Visit for FieldFormatter {
    fn record_debug(&mut self, field: &tracing::field::Field, value: &dyn std::fmt::Debug) {
        if !self.s.is_empty() {
            self.s.push_str(", ");
        }
        if self.is_event && field.name() == "message" {
            self.s.push_str(&format!("{:?}", value));
        } else {
            self.s.push_str(&format!("{} = {:?}", field.name(), value));
        }
    }
}

impl<S: Subscriber + for<'lookup> LookupSpan<'lookup>> Layer<S> for AtraceSubscriber {
    fn on_new_span(&self, attrs: &Attributes, id: &Id, ctx: Context<S>) {
        if self.should_record_fields {
            let mut formatter = FieldFormatter::for_span();
            attrs.record(&mut formatter);
            ctx.span(id).unwrap().extensions_mut().insert(formatter)
        }
    }

    fn on_record(&self, span: &Id, values: &Record, ctx: Context<S>) {
        if self.should_record_fields {
            values.record(
                ctx.span(span).unwrap().extensions_mut().get_mut::<FieldFormatter>().unwrap(),
            );
        }
    }

    fn on_enter(&self, id: &Id, ctx: Context<S>) {
        let span = ctx.span(id).unwrap();
        let mut span_str = String::from(span.metadata().name());
        if self.should_record_fields {
            let span_extensions = span.extensions();
            let formatter = span_extensions.get::<FieldFormatter>().unwrap();
            if !formatter.is_empty() {
                span_str.push_str(", ");
                span_str.push_str(formatter.as_string());
            }
        }
        atrace::atrace_begin(self.tag, &span_str);
    }

    fn on_exit(&self, _id: &Id, _ctx: Context<S>) {
        atrace::atrace_end(self.tag);
    }

    fn on_event(&self, event: &Event, _ctx: Context<S>) {
        let mut event_str = String::new();
        if self.should_record_fields {
            let mut formatter = FieldFormatter::for_event();
            event.record(&mut formatter);
            event_str = formatter.as_string().clone();
        } else {
            struct MessageVisitor<'a> {
                s: &'a mut String,
            }
            impl Visit for MessageVisitor<'_> {
                fn record_debug(
                    &mut self,
                    field: &tracing::field::Field,
                    value: &dyn std::fmt::Debug,
                ) {
                    if field.name() == "message" {
                        self.s.push_str(&format!("{:?}", value));
                    }
                }
            }
            event.record(&mut MessageVisitor { s: &mut event_str });
        }
        if event_str.is_empty() {
            event_str.push_str(&format!("{} event", event.metadata().level().as_str()));
        }
        atrace::atrace_instant(self.tag, &event_str);
    }
}
