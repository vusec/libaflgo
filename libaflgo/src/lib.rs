use std::{
    marker::PhantomData,
    time::{Duration, Instant},
};

use libaflgo_targets::{compute_test_case_distance, reset_distance_stats};

use libafl::{
    impl_serdeany,
    prelude::{
        Event, EventFirer, ExitKind, Feedback, Named, Observer, ObserversTuple, Testcase,
        UserStats, UsesInput,
    },
    schedulers::{testcase_score::CorpusPowerTestcaseScore, TestcaseScore},
    stages::PowerMutationalStage,
    state::{HasClientPerfMonitor, HasCorpus, HasMetadata, HasNamedMetadata},
    Error,
};
use serde::{Deserialize, Serialize};

#[derive(Debug, Serialize, Deserialize)]
pub struct DistanceObserver {
    name: String,
    distance: Option<f64>,
}

impl DistanceObserver {
    #[must_use]
    pub fn new(name: String) -> Self {
        Self {
            name,
            distance: None,
        }
    }

    pub fn distance(&self) -> Option<f64> {
        self.distance
    }
}

impl Named for DistanceObserver {
    fn name(&self) -> &str {
        &self.name
    }
}

impl<S: UsesInput> Observer<S> for DistanceObserver {
    fn pre_exec(&mut self, _state: &mut S, _input: &S::Input) -> Result<(), libafl::Error> {
        reset_distance_stats();
        self.distance = None;
        Ok(())
    }

    fn post_exec(
        &mut self,
        _state: &mut S,
        _input: &S::Input,
        _exit_kind: &ExitKind,
    ) -> Result<(), libafl::Error> {
        self.distance = Some(compute_test_case_distance());
        Ok(())
    }
}

#[derive(Debug)]
pub struct DistanceFeedback {
    name: String,
    distance: Option<f64>,

    schedule: Option<CoolingSchedule>,
    time_to_exploit: Duration,
}

impl DistanceFeedback {
    #[must_use]
    pub fn new(name: String, schedule: Option<CoolingSchedule>, time_to_exploit: Duration) -> Self {
        Self {
            name,
            distance: None,
            schedule,
            time_to_exploit,
        }
    }

    #[must_use]
    pub fn with_observer(
        observer: &DistanceObserver,
        schedule: Option<CoolingSchedule>,
        time_to_exploit: Duration,
    ) -> Self {
        Self {
            name: observer.name().to_string(),
            distance: None,
            schedule,
            time_to_exploit,
        }
    }
}

impl<S: UsesInput + HasClientPerfMonitor + HasMetadata> Feedback<S> for DistanceFeedback {
    fn init_state(&mut self, state: &mut S) -> Result<(), libafl::Error> {
        state.add_metadata(DistanceMetadata::with_schedule(
            self.schedule,
            self.time_to_exploit,
        ));
        Ok(())
    }

    fn is_interesting<EM, OT>(
        &mut self,
        state: &mut S,
        manager: &mut EM,
        _input: &S::Input,
        observers: &OT,
        _exit_kind: &ExitKind,
    ) -> Result<bool, libafl::Error>
    where
        EM: EventFirer<State = S>,
        OT: ObserversTuple<S>,
    {
        let distance_observer = observers
            .match_name::<DistanceObserver>(self.name())
            .ok_or_else(|| {
                libafl::Error::key_not_found("DistanceObserver not found".to_string())
            })?;
        let cur_distance = distance_observer
            .distance()
            .ok_or_else(|| Error::empty_optional("distance was not set in observer".to_string()))?;
        self.distance = Some(cur_distance);

        let distance_metadata = state.metadata_mut::<DistanceMetadata>().unwrap();
        distance_metadata.update_runtime();
        if !distance_metadata.is_interesting(cur_distance) {
            return Ok(false);
        }

        // This reports the existing distance range, without including the
        // current test case since it is not yet known if it will be
        // inserted in the queue.

        let min_distance = distance_metadata.min_distance();
        let max_distance = distance_metadata.max_distance();

        if let (Some(min_distance), Some(max_distance)) = (min_distance, max_distance) {
            manager.fire(
                state,
                Event::UpdateUserStats {
                    name: self.name.clone() + "_min",
                    value: UserStats::Float(min_distance),
                    phantom: PhantomData,
                },
            )?;

            manager.fire(
                state,
                Event::UpdateUserStats {
                    name: self.name.clone() + "_max",
                    value: UserStats::Float(max_distance),
                    phantom: PhantomData,
                },
            )?;
        }

        Ok(true)
    }

    fn append_metadata<OT>(
        &mut self,
        state: &mut S,
        _observers: &OT,
        testcase: &mut Testcase<S::Input>,
    ) -> Result<(), libafl::Error>
    where
        OT: ObserversTuple<S>,
    {
        let cur_distance = self
            .distance
            .ok_or_else(|| Error::empty_optional("distance was not set".to_string()))?;
        testcase.add_metadata(DistanceTestcaseMetadata::new(cur_distance));

        let distance_metadata = state.metadata_mut::<DistanceMetadata>().unwrap();
        distance_metadata.update_range(cur_distance);

        self.distance = None;
        Ok(())
    }

    fn discard_metadata(&mut self, _state: &mut S, _input: &S::Input) -> Result<(), libafl::Error> {
        self.distance = None;
        Ok(())
    }
}

impl Named for DistanceFeedback {
    fn name(&self) -> &str {
        &self.name
    }
}

#[derive(Clone, Debug, Serialize, Deserialize, Copy)]
pub enum CoolingSchedule {
    Exponential,
    Logarithmic,
    Linear,
    Quadratic,
}

#[derive(Serialize, Deserialize, Clone, Debug, Default)]
pub struct DistanceMetadata {
    schedule: Option<CoolingSchedule>,

    // Incrementally increasing runtime is necessary to preserve correctness
    // across restarts.
    #[serde(skip)]
    last_update: Option<Instant>,
    runtime: Duration,
    time_to_exploit: Duration,

    min_distance: Option<f64>,
    max_distance: Option<f64>,
}

impl DistanceMetadata {
    #[must_use]
    pub fn new() -> Self {
        Self {
            last_update: Some(Instant::now()),
            ..Default::default()
        }
    }

    #[must_use]
    pub fn with_schedule(schedule: Option<CoolingSchedule>, time_to_exploit: Duration) -> Self {
        Self {
            schedule,
            last_update: Some(Instant::now()),
            runtime: Duration::ZERO,
            time_to_exploit,
            ..Default::default()
        }
    }

    pub fn update_range(&mut self, cur_distance: f64) {
        self.min_distance = Some(self.min_distance.unwrap_or(cur_distance).min(cur_distance));
        self.max_distance = Some(self.max_distance.unwrap_or(cur_distance).max(cur_distance));
    }

    pub fn is_interesting(&mut self, cur_distance: f64) -> bool {
        if !cur_distance.is_finite() {
            return false;
        }

        if let Some(min_distance) = self.min_distance {
            cur_distance < min_distance
        } else {
            true
        }
    }

    #[must_use]
    pub fn normalize(&self, cur_distance: f64) -> Option<f64> {
        let min_distance = self.min_distance?;
        let max_distance = self.max_distance?;
        if min_distance == max_distance {
            return None;
        }

        Some((cur_distance - min_distance) / (max_distance - min_distance))
    }

    #[must_use]
    pub fn schedule(&self) -> Option<&CoolingSchedule> {
        self.schedule.as_ref()
    }

    fn update_runtime(&mut self) {
        let time_since_last_update = if let Some(last_update) = self.last_update {
            last_update.elapsed()
        } else {
            // After a restart
            Duration::ZERO
        };
        self.runtime += time_since_last_update;
        self.last_update = Some(Instant::now());
    }

    #[must_use]
    pub fn progress_to_exploit(&self) -> f64 {
        self.time_to_exploit.as_secs_f64() / self.runtime.as_secs_f64()
    }

    pub fn min_distance(&self) -> Option<f64> {
        self.min_distance
    }

    pub fn max_distance(&self) -> Option<f64> {
        self.max_distance
    }
}

impl_serdeany!(DistanceMetadata);

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct DistanceTestcaseMetadata {
    distance: f64,
}

impl DistanceTestcaseMetadata {
    #[must_use]
    pub fn new(distance: f64) -> Self {
        Self { distance }
    }

    #[must_use]
    pub fn distance(&self) -> f64 {
        self.distance
    }
}

impl_serdeany!(DistanceTestcaseMetadata);

const HAVOC_MAX_MULT: f64 = 64.0; // From testcase_score.rs
const MAX_FACTOR: f64 = 32.0;

pub struct DistancePowerTestcaseScore;

impl<S> TestcaseScore<S> for DistancePowerTestcaseScore
where
    S: HasCorpus + HasMetadata + HasNamedMetadata,
{
    fn compute(entry: &mut Testcase<<S>::Input>, state: &S) -> Result<f64, libafl::Error> {
        let mut perf_score = CorpusPowerTestcaseScore::compute(entry, state)?;

        let test_case_metadata = entry.metadata::<DistanceTestcaseMetadata>()?;
        let test_case_distance = test_case_metadata.distance();
        if !test_case_distance.is_finite() {
            // If the distance from the target is unknown, fall back to AFL-like scheduling.
            return Ok(perf_score);
        }

        let distance_metadata = state.metadata::<DistanceMetadata>()?;

        let Some(schedule) = distance_metadata.schedule() else { return Ok(perf_score); };
        let progress_to_exploit = distance_metadata.progress_to_exploit();
        let t_schedule = match schedule {
            CoolingSchedule::Exponential => 1.0 / 20.0_f64.powf(progress_to_exploit),
            CoolingSchedule::Logarithmic => {
                let alpha = f64::exp(19.0 / 2.0);
                1.0 / (1.0 + 2.0 * f64::ln(1.0 + progress_to_exploit * alpha))
            }
            CoolingSchedule::Linear => 1.0 / (1.0 + 19.0 * progress_to_exploit),
            CoolingSchedule::Quadratic => 1.0 / (1.0 + 19.0 * progress_to_exploit.powf(2.0)),
        };

        let Some(norm_distance) = distance_metadata.normalize(test_case_distance) else { return Ok(perf_score); };

        let p = (1.0 - norm_distance) * (1.0 - t_schedule) + 0.5 * t_schedule;
        let power_factor = 2.0_f64.powf(2.0 * f64::log2(MAX_FACTOR) * (p - 0.5));
        perf_score *= power_factor;

        // Check upper bound again
        if perf_score > HAVOC_MAX_MULT * 100.0 {
            perf_score = HAVOC_MAX_MULT * 100.0;
        }

        assert!(perf_score.is_finite());
        Ok(perf_score)
    }
}

pub type DistancePowerMutationalStage<E, EM, I, M, Z> =
    PowerMutationalStage<E, DistancePowerTestcaseScore, EM, I, M, Z>;
