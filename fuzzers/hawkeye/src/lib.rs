//! A singlethreaded libfuzzer-like fuzzer that can auto-restart.
use mimalloc::MiMalloc;
#[global_allocator]
static GLOBAL: MiMalloc = MiMalloc;

use std::{
    cell::RefCell,
    env,
    fs::{self, File, OpenOptions},
    io::{self, Read, Write},
    os::fd::AsRawFd,
    path::{Path, PathBuf},
    process::{self, exit},
    time::Duration,
};

use clap::{Parser, ValueEnum};
use libafl::{
    bolts::{
        current_nanos, current_time,
        os::dup2,
        rands::StdRand,
        shmem::{ShMemProvider, StdShMemProvider},
        tuples::{tuple_list, Merge},
        AsSlice,
    },
    corpus::{Corpus, OnDiskCorpus},
    events::SimpleRestartingEventManager,
    executors::{inprocess::InProcessExecutor, ExitKind, TimeoutExecutor},
    feedback_or,
    feedbacks::{CrashFeedback, MaxMapFeedback, TimeFeedback},
    fuzzer::{Fuzzer, StdFuzzer},
    inputs::{BytesInput, HasTargetBytes},
    monitors::SimpleMonitor,
    mutators::{
        scheduled::havoc_mutations, token_mutations::I2SRandReplace, tokens_mutations,
        StdMOptMutator, StdScheduledMutator, Tokens,
    },
    observers::{HitcountsMapObserver, TimeObserver},
    schedulers::IndexesLenTimeMinimizerScheduler,
    stages::{calibrate::CalibrationStage, StdMutationalStage, TracingStage},
    state::{HasCorpus, HasMetadata, StdState},
    Error,
};
#[cfg(any(target_os = "linux", target_vendor = "apple"))]
use libafl_targets::autotokens;
use libafl_targets::{
    libfuzzer_initialize, libfuzzer_test_one_input, std_edges_map_observer, CmpLogObserver,
};

use libaflgo::{
    CoolingSchedule, DistanceFeedback, DistancePowerMutationalStage, DistanceWeightedScheduler,
    SimilarityFeedback,
};
use libaflgo_targets::{
    distance::InProcessDistanceObserver, similarity::InProcessSimilarityObserver,
    target::get_targets_map_observer,
};

/// LibAFL-based in-process reimplementation of AFLGo
#[derive(Parser, Debug)]
#[command(author, version, about)]
struct Args {
    /// The directory to place finds in ('corpus')
    #[arg(short, long)]
    output: PathBuf,

    /// The directory to read initial inputs from ('seeds')
    #[arg(short, long)]
    input: PathBuf,

    /// A file to read tokens from, to be used during fuzzing
    #[arg(short = 'x', long)]
    tokens: Option<PathBuf>,

    /// Duplicates all output to this file
    #[arg(short, long, default_value = "libafl.log")]
    logfile: PathBuf,

    /// Timeout for each individual execution, in milliseconds
    #[arg(short, long, default_value = "1200")]
    timeout: u64,

    /// Do not redirect stdout and stderr to /dev/null
    #[arg(short = 'D', long)]
    show_target_output: bool,

    /// Cooling schedule used for directed fuzzing
    #[arg(short = 'z', long, default_value = "exp")]
    cooling_schedule: CoolingScheduleClap,

    /// Cut-off time for cooling schedule, in minutes
    #[arg(short = 'c', long, default_value = "10")]
    time_to_exploit: u64,
}

#[derive(Clone, Debug)]
struct CoolingScheduleClap(CoolingSchedule);

impl ValueEnum for CoolingScheduleClap {
    fn value_variants<'a>() -> &'a [Self] {
        &[
            CoolingScheduleClap(CoolingSchedule::Exponential),
            CoolingScheduleClap(CoolingSchedule::Linear),
            CoolingScheduleClap(CoolingSchedule::Logarithmic),
            CoolingScheduleClap(CoolingSchedule::Quadratic),
        ]
    }

    fn to_possible_value(&self) -> Option<clap::builder::PossibleValue> {
        match self {
            CoolingScheduleClap(CoolingSchedule::Exponential) => Some("exp".into()),
            CoolingScheduleClap(CoolingSchedule::Linear) => Some("lin".into()),
            CoolingScheduleClap(CoolingSchedule::Logarithmic) => Some("log".into()),
            CoolingScheduleClap(CoolingSchedule::Quadratic) => Some("quad".into()),
        }
    }
}

#[derive(Parser, Debug)]
struct FallbackArgs {
    test_cases: Vec<PathBuf>,
}

/// The fuzzer main (as `no_mangle` C function)
#[no_mangle]
pub fn libafl_main() {
    let args = Args::try_parse().unwrap_or_else(|error| {
        // Attempting a dry run
        let fallback = FallbackArgs::try_parse();
        if let Ok(fallback) = fallback {
            if !fallback.test_cases.is_empty() {
                run_testcases(&fallback.test_cases);
                exit(0);
            }
        }

        error.exit();
    });

    println!("Workdir: {}", env::current_dir().unwrap().display());

    // For fuzzbench, crashes and finds are inside the same `corpus` directory, in the "queue" and "crashes" subdir.
    let out_dir = args.output;
    if let Err(error) = fs::create_dir(&out_dir) {
        if error.kind() == io::ErrorKind::AlreadyExists {
            eprintln!("Output directory already exists: {}", out_dir.display());
        } else {
            eprintln!("Could not create output dir: {}", out_dir.display());
            return;
        }
    }

    let in_dir = args.input;
    if !in_dir.is_dir() {
        eprintln!(
            "Input directory is not a valid directory: {}",
            in_dir.display()
        );
        return;
    }

    if let Err(error) = fuzz(
        out_dir.join("queue"),
        out_dir.join("crashes"),
        in_dir,
        args.tokens,
        args.logfile,
        Duration::from_millis(args.timeout),
        args.show_target_output,
        args.cooling_schedule.0,
        Duration::from_secs(args.time_to_exploit * 60),
    ) {
        panic!("An error occurred while fuzzing: {error}");
    }
}

fn run_testcases(filenames: &[impl AsRef<Path>]) {
    // The actual target run starts here.
    // Call LLVMFUzzerInitialize() if present.
    let args: Vec<String> = env::args().collect();
    if libfuzzer_initialize(&args) == -1 {
        println!("Warning: LLVMFuzzerInitialize failed with -1");
    }

    println!(
        "You are not fuzzing, just executing {} testcases",
        filenames.len()
    );
    for fname in filenames {
        println!("Executing {}", fname.as_ref().display());

        let mut file = File::open(fname).expect("No file found");
        let mut buffer = vec![];
        file.read_to_end(&mut buffer).expect("Buffer overflow");

        libfuzzer_test_one_input(&buffer);
    }
}

/// The actual fuzzer
#[allow(clippy::too_many_arguments)]
fn fuzz<P: AsRef<Path>>(
    corpus_dir: P,
    objective_dir: P,
    seed_dir: P,
    tokenfile: Option<P>,
    logfile: P,
    timeout: Duration,
    show_target_output: bool,
    cooling_schedule: CoolingSchedule,
    time_to_exploit: Duration,
) -> Result<(), Error> {
    let log = RefCell::new(
        OpenOptions::new()
            .append(true)
            .create(true)
            .open(logfile.as_ref())?,
    );

    #[cfg(unix)]
    let file_null = File::open("/dev/null")?;

    // While the monitor are state, they are usually used in the broker - which is likely never restarted
    let monitor = SimpleMonitor::with_user_monitor(
        |s| {
            println!("{s}");
            writeln!(log.borrow_mut(), "{:?} {s}", current_time()).unwrap();
        },
        true,
    );

    // We need a shared map to store our state before a crash.
    // This way, we are able to continue fuzzing afterwards.
    let mut shmem_provider = StdShMemProvider::new()?;

    let (state, mut mgr) = match SimpleRestartingEventManager::launch(monitor, &mut shmem_provider)
    {
        // The restarting state will spawn the same process again as child, then restarted it each time it crashes.
        Ok(res) => res,
        Err(err) => match err {
            Error::ShuttingDown => {
                return Ok(());
            }
            _ => {
                panic!("Failed to setup the restarter: {err}");
            }
        },
    };

    // Create an observation channel using the coverage map
    // We don't use the hitcounts (see the Cargo.toml, we use pcguard_edges)
    let edges_observer = HitcountsMapObserver::new(unsafe { std_edges_map_observer("edges") });

    // Create an observation channel to keep track of the execution time
    let time_observer = TimeObserver::new("time");

    // Create an observation channel to keep track of the distance of test cases
    let distance_observer = InProcessDistanceObserver::new(String::from("distance"));

    // Create an observation channel to keep track of the similarity of test cases
    let similarity_observer = InProcessSimilarityObserver::new(String::from("similarity"));

    let targets_observer =
        HitcountsMapObserver::new(unsafe { get_targets_map_observer("targets") });

    let cmplog_observer = CmpLogObserver::new("cmplog", true);

    let map_feedback = MaxMapFeedback::tracking(&edges_observer, true, false);

    let targets_feedback = MaxMapFeedback::new(&targets_observer);

    let calibration = CalibrationStage::new(&map_feedback);

    // Feedback to rate the interestingness of an input
    // This one is composed by two Feedbacks in OR
    let mut feedback = feedback_or!(
        // New maximization map feedback linked to the edges observer and the feedback state
        map_feedback,
        // Time feedback, this one does not need a feedback state
        TimeFeedback::with_observer(&time_observer),
        // Distance feedback, it adds the distace of a test case as metadata
        DistanceFeedback::with_observer(
            &distance_observer,
            Some(cooling_schedule),
            time_to_exploit
        ),
        // Similarity feedback, it adds the similarity of a test case as metadata
        SimilarityFeedback::with_observer(&similarity_observer),
        targets_feedback
    );

    // A feedback to choose if an input is a solution or not
    let mut objective = CrashFeedback::new();

    // If not restarting, create a State from scratch
    let mut state = state.unwrap_or_else(|| {
        StdState::new(
            // RNG
            StdRand::with_seed(current_nanos()),
            // Corpus that will be evolved, we keep it in memory for performance
            OnDiskCorpus::new(corpus_dir).unwrap(),
            // Corpus in which we store solutions (crashes in this example),
            // on disk so the user can get them after stopping the fuzzer
            OnDiskCorpus::new(objective_dir).unwrap(),
            // States of the feedbacks.
            // The feedbacks can report the data that should persist in the State.
            &mut feedback,
            // Same for objective feedbacks
            &mut objective,
        )
        .unwrap()
    });

    println!("Let's fuzz :)");

    // The actual target run starts here.
    // Call LLVMFUzzerInitialize() if present.
    let args: Vec<String> = env::args().collect();
    if libfuzzer_initialize(&args) == -1 {
        println!("Warning: LLVMFuzzerInitialize failed with -1");
    }

    // Setup a randomic Input2State stage
    let i2s = StdMutationalStage::new(StdScheduledMutator::new(tuple_list!(I2SRandReplace::new())));

    // Setup a MOPT mutator
    let mutator = StdMOptMutator::new(
        &mut state,
        havoc_mutations().merge(tokens_mutations()),
        7,
        5,
    )?;

    let power = DistancePowerMutationalStage::new(mutator);

    // A minimization+queue policy to get testcasess from the corpus
    let scheduler = IndexesLenTimeMinimizerScheduler::new(DistanceWeightedScheduler::new(
        &mut state,
        &edges_observer,
    ));

    // A fuzzer with feedbacks and a corpus scheduler
    let mut fuzzer = StdFuzzer::new(scheduler, feedback, objective);

    // The wrapped harness function, calling out to the LLVM-style harness
    let mut harness = |input: &BytesInput| {
        let target = input.target_bytes();
        let buf = target.as_slice();
        libfuzzer_test_one_input(buf);
        ExitKind::Ok
    };

    let mut tracing_harness = harness;

    // Create the executor for an in-process function with one observer for edge coverage and one for the execution time
    let mut executor = TimeoutExecutor::new(
        InProcessExecutor::new(
            &mut harness,
            tuple_list!(
                edges_observer,
                time_observer,
                distance_observer,
                similarity_observer,
                targets_observer
            ),
            &mut fuzzer,
            &mut state,
            &mut mgr,
        )?,
        timeout,
    );

    // Setup a tracing stage in which we log comparisons
    let tracing = TracingStage::new(TimeoutExecutor::new(
        InProcessExecutor::new(
            &mut tracing_harness,
            tuple_list!(cmplog_observer),
            &mut fuzzer,
            &mut state,
            &mut mgr,
        )?,
        // Give it more time!
        timeout * 10,
    ));

    // The order of the stages matter!
    let mut stages = tuple_list!(calibration, tracing, i2s, power);

    // Read tokens
    if state.metadata_map().get::<Tokens>().is_none() {
        let mut toks = Tokens::default();
        if let Some(tokenfile) = tokenfile {
            toks.add_from_file(tokenfile)?;
        }
        #[cfg(any(target_os = "linux", target_vendor = "apple"))]
        {
            toks += autotokens()?;
        }

        if !toks.is_empty() {
            state.add_metadata(toks);
        }
    }

    // In case the corpus is empty (on first run), reset
    if state.must_load_initial_inputs() {
        state
            .load_initial_inputs(
                &mut fuzzer,
                &mut executor,
                &mut mgr,
                &[seed_dir.as_ref().to_path_buf()],
            )
            .unwrap_or_else(|error| {
                println!(
                    "Failed to load initial corpus in {}: {}",
                    seed_dir.as_ref().display(),
                    error,
                );
                process::exit(0);
            });
        println!("We imported {} inputs from disk.", state.corpus().count());
    }

    // Remove target ouput (logs still survive)
    if !show_target_output {
        #[cfg(unix)]
        {
            let null_fd = file_null.as_raw_fd();
            dup2(null_fd, io::stdout().as_raw_fd())?;
            dup2(null_fd, io::stderr().as_raw_fd())?;
        }
    }
    // reopen file to make sure we're at the end
    log.replace(OpenOptions::new().append(true).create(true).open(logfile)?);

    fuzzer.fuzz_loop(&mut stages, &mut executor, &mut state, &mut mgr)?;

    // Never reached
    Ok(())
}
