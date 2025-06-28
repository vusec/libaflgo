# LibAFLGo

<img src="logo.png", width="250", align="right" />

> This is the main repository for the paper _LibAFLGo: Evaluating and Advancing Directed Greybox
> Fuzzing_. We are in the process of upstreaming LibAFLGo to LibAFL.

LibAFLGo extends [LibAFL](https://github.com/AFLplusplus/LibAFL) for directed fuzzing. It
re-implements three directed fuzzing policies in a modular fashion: AFLGo, Hawkeye, and DAFL. The
LLVM-based compiler passes integrate with [SVF](https://github.com/SVF-tools/SVF), which provides a
plethora of analyses out-of-the-box.

The research artifacts are split across three repositories:

- this repository contains directed fuzzing extensions for LibAFL, re-implemented fuzzers
- [MAGMA-directed](https://github.com/vusec/magma-directed) extends MAGMA for directed fuzzing
- [LibAFL-directed](https://github.com/vusec/LibAFL-directed) contains minor patches to LibAFL

## Project Structure

```
.
├── fuzzers                                             <- contains re-implemented fuzzers
│   ├── aflgo
│   ├── dafl
│   └── hawkeye
├── include                                             <- header files for LLVM passes
│   ├── AFLGoCompiler                                   <-   compile-time plugin
│   │   └── TargetInjection.hpp                         <-     instruments target locations
│   ├── AFLGoLinker                                     <-   link-time plugin
│   │   ├── DAFL.hpp                                    <-     DAFL instrumentation
│   │   ├── DistanceInstrumentation.hpp                 <-     AFLGo distance instrumentation
│   │   ├── DuplicateTargetRemoval.hpp                  <-     supporting target instrumentation
│   │   ├── FunctionDistanceInstrumentation.hpp         <-     Hawkeye distance instrumentation
│   │   └── TargetInjectionFixup.hpp                    <-     supporting target instrumentation
│   └── Analysis                                        <-   analyses used by plugins
│       ├── BasicBlockDistance.hpp                      <-     AFLGo basic block distance analysis
│       ├── DAFL.hpp                                    <-     DAFL data-flow distance
│       ├── ExtendedCallGraph.hpp                       <-     enhance CFG with PTA
│       ├── FunctionDistance.hpp                        <-     Hawkeye function distance analysis
│       └── TargetDetection.hpp                         <-     supporting target instrumentation
├── libaflgo                                            <- LibAFL fuzzer components
├── libaflgo_targets                                    <- LibAFL target instrumentation components
├── passes                                              <- implementation of LLVM passes
├── test                                                <- tests for LLVM passes
├── wrapper                                             <- compiler wrapper libaflgo_cc
├── Cargo.lock
├── Cargo.toml
├── CMakeLists.txt                                      <- cmake build entrypoint
├── README.md
├── rust-toolchain.toml
├── SVF-1282.patch                                      <- patch SVF for ASan compatibility
```

## Building

You can use `cmake` to build LLVM passes and LibAFL components:

```bash
cmake --build build --config RelWithDebInfo
```

## MAGMA Integration

We extended [MAGMA](https://github.com/vusec/magma-directed) for directed fuzzing. The original
documentation applies also to our fork. The easiest way to get up and running to fuzz targets is to
look at the LibAFLGo integrations in the `fuzzers` folder in that repository.
