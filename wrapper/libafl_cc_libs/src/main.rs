use libafl_cc::LLVMPasses;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = std::env::args().collect::<Vec<_>>();
    if args.len() < 3 {
        eprintln!("Usage: {} <pass> <dst>", args[0]);
        std::process::exit(1);
    }

    let pass = match args[1].as_str() {
        "autotokens" => LLVMPasses::AutoTokens,
        "cmplog-rtn" => LLVMPasses::CmpLogRtn,
        _ => {
            eprintln!("Unknown pass");
            std::process::exit(1);
        }
    };

    let pass_path = pass.path();
    println!("{} -> {}", pass_path.display(), &args[2]);
    std::fs::copy(pass_path, &args[2])?;

    Ok(())
}
