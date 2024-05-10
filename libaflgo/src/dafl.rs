use std::{fmt, marker::PhantomData};

use libafl::{
    impl_serdeany,
    prelude::{
        Corpus, Event, EventFirer, ExitKind, Feedback, Named, Observer, ObserversTuple, Testcase,
        UserStats, UsesInput,
    },
    schedulers::{testcase_score::CorpusWeightTestcaseScore, TestcaseScore, WeightedScheduler},
    stages::PowerMutationalStage,
    state::{HasClientPerfMonitor, HasCorpus, HasMetadata},
    Error,
};

use serde::{Deserialize, Serialize};

pub trait DAFLObserver<S>: Observer<S>
where
    S: UsesInput,
{
    #[must_use]
    fn relevance(&self) -> u64;
}

#[derive(Debug)]
pub struct DAFLFeedback<O, S> {
    name: String,
    relevance: Option<u64>,

    phantom: PhantomData<(O, S)>,
}

impl<S: UsesInput, O: DAFLObserver<S>> DAFLFeedback<O, S> {
    #[must_use]
    pub fn new(name: String) -> Self {
        Self {
            name,
            relevance: None,
            phantom: PhantomData,
        }
    }

    #[must_use]
    pub fn with_observer(observer: &O) -> Self {
        Self {
            name: observer.name().to_string(),
            relevance: None,
            phantom: PhantomData,
        }
    }
}

impl<
        S: UsesInput + HasClientPerfMonitor + HasMetadata + HasCorpus + fmt::Debug,
        O: DAFLObserver<S>,
    > Feedback<S> for DAFLFeedback<O, S>
{
    fn init_state(&mut self, state: &mut S) -> Result<(), Error> {
        state.add_metadata(DAFLMetadata::default());
        Ok(())
    }

    fn is_interesting<EM, OT>(
        &mut self,
        state: &mut S,
        manager: &mut EM,
        _input: &S::Input,
        observers: &OT,
        _exit_kind: &ExitKind,
    ) -> Result<bool, Error>
    where
        EM: EventFirer<State = S>,
        OT: ObserversTuple<S>,
    {
        let observer = observers
            .match_name::<O>(self.name())
            .ok_or_else(|| libafl::Error::key_not_found("DAFLObserver not found".to_string()))?;
        let cur_relevance = observer.relevance();
        self.relevance = Some(cur_relevance);

        let metadata = state.metadata_mut::<DAFLMetadata>().unwrap();
        if !metadata.is_interesting(cur_relevance) {
            return Ok(false);
        }

        // This reports the existing range, without including the current test
        // case since it is not yet known if it will be inserted in the queue.

        if let (Some(avg), Some(min), Some(max)) = (
            metadata.avg_relevance(),
            metadata.min_relevance(),
            metadata.max_relevance(),
        ) {
            manager.fire(
                state,
                Event::UpdateUserStats {
                    name: self.name.clone() + "_avg",
                    value: UserStats::Float(avg),
                    phantom: PhantomData,
                },
            )?;

            manager.fire(
                state,
                Event::UpdateUserStats {
                    name: self.name.clone() + "_min",
                    value: UserStats::Number(min),
                    phantom: PhantomData,
                },
            )?;

            manager.fire(
                state,
                Event::UpdateUserStats {
                    name: self.name.clone() + "_max",
                    value: UserStats::Number(max),
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
    ) -> Result<(), Error>
    where
        OT: ObserversTuple<S>,
    {
        let cur = self
            .relevance
            .ok_or_else(|| Error::empty_optional("relevance was not set".to_string()))?;
        testcase.add_metadata(DAFLTestcaseMetadata::new(cur));

        let qlen = state.corpus().count();

        let metadata = state.metadata_mut::<DAFLMetadata>().unwrap();
        metadata.update(cur, qlen);

        self.relevance = None;
        Ok(())
    }

    fn discard_metadata(&mut self, _state: &mut S, _input: &S::Input) -> Result<(), Error> {
        self.relevance = None;
        Ok(())
    }
}

impl<O, S> Named for DAFLFeedback<O, S> {
    fn name(&self) -> &str {
        &self.name
    }
}

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct DAFLMetadata {
    avg_relevance: Option<f64>,
    min_relevance: Option<u64>,
    max_relevance: Option<u64>,
}

impl DAFLMetadata {
    fn is_interesting(&self, cur_relevance: u64) -> bool {
        if cur_relevance == 0 {
            return false;
        }

        if let Some(min_relevance) = self.min_relevance {
            cur_relevance > min_relevance
        } else {
            true
        }
    }

    fn min_relevance(&self) -> Option<u64> {
        self.min_relevance
    }

    fn max_relevance(&self) -> Option<u64> {
        self.max_relevance
    }

    fn avg_relevance(&self) -> Option<f64> {
        self.avg_relevance
    }

    fn update(&mut self, cur: u64, qlen: usize) {
        self.avg_relevance = self
            .avg_relevance
            .map(|avg| avg + ((cur as f64 - avg) / qlen as f64))
            .or(Some(cur as f64));
        self.min_relevance = Some(self.min_relevance.unwrap_or(cur).min(cur));
        self.max_relevance = Some(self.max_relevance.unwrap_or(cur).max(cur));
    }
}

impl_serdeany!(DAFLMetadata);

#[derive(Debug, Serialize, Deserialize)]
pub struct DAFLTestcaseMetadata {
    relevance: u64,
}

impl DAFLTestcaseMetadata {
    #[must_use]
    pub fn new(relevance: u64) -> Self {
        Self { relevance }
    }

    #[must_use]
    pub fn relevance(&self) -> u64 {
        self.relevance
    }
}

impl_serdeany!(DAFLTestcaseMetadata);

pub struct DAFLWeightTestcaseScore;

impl<S> TestcaseScore<S> for DAFLWeightTestcaseScore
where
    S: HasCorpus + HasMetadata,
{
    fn compute(_state: &S, entry: &mut Testcase<S::Input>) -> Result<f64, Error> {
        let tc_metadata = entry.metadata::<DAFLTestcaseMetadata>()?;
        Ok(tc_metadata.relevance() as f64)
    }
}

pub type DAFLWeightedScheduler<O, S> = WeightedScheduler<DAFLWeightTestcaseScore, O, S>;

pub struct DAFLPowerTestcaseScore;

impl<S> TestcaseScore<S> for DAFLPowerTestcaseScore
where
    S: HasCorpus + HasMetadata,
{
    fn compute(state: &S, entry: &mut Testcase<S::Input>) -> Result<f64, Error> {
        let mut weight = CorpusWeightTestcaseScore::compute(state, entry)?;
        let tc_metadata = entry.metadata::<DAFLTestcaseMetadata>()?;
        let metadata = state.metadata::<DAFLMetadata>()?;
        weight *= tc_metadata.relevance() as f64
            / metadata
                .avg_relevance()
                .ok_or_else(|| Error::illegal_state("avg relevance should have been set"))?;
        Ok(weight)
    }
}

pub type DAFLPowerMutationalStage<E, EM, I, M, Z> =
    PowerMutationalStage<E, DAFLPowerTestcaseScore, EM, I, M, Z>;
