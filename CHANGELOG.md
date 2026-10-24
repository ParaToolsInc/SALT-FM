# Changelog

All notable changes to SALT-FM are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

- `saltfm` stops with an error, and writes no output, when
  `--tau_output` comes with more than one source, instead of writing
  only the last source to the output
  ([#111](https://github.com/ParaToolsInc/SALT-FM/issues/111)).
- Before `--`, `-o <file>`, `-o=<file>`, `--o <file>`, and
  `--o=<file>` name the output file, as `--tau_output` does, and
  `-o<file>` is an error. C and C++ refused `-o`, and
  Fortran ignored it or took `<file>` as the source. After `--`, `-o`,
  `--o`, or `--output` and its value are build options, not a source.
  Giving the output file more than once, with `-o` or `--tau_output`,
  is an error ([#111](https://github.com/ParaToolsInc/SALT-FM/issues/111)).
- `saltfm` puts each TAU call of a Fortran source on its line of the
  source when the source has `#line` lines, such as `#line 100
  "orig.f90"` or the `# 100 "orig.f90"` line markers of `cpp` and fypp.
  Before, it used the line numbers of the `#line` lines, so it wrote no
  TAU calls or put them on wrong lines, with exit status 0, or Flang
  aborted, as with an instrumented output as the input. The timer
  names give the file and lines of the `#line` lines, as compiler
  messages do. When the instrumentation points are out of line order,
  which instrumented code in an included file can cause, `saltfm` stops
  with an error and exit status 1 and writes no output, instead of
  aborting ([#112](https://github.com/ParaToolsInc/SALT-FM/issues/112)).
- The new `--tau_build_uses_salt_flang` option of `saltfm` and
  `fparse-llvm`, or the `SALT_BUILD_USES_SALT_FLANG` environment
  variable set to any value except empty, `0`, `false`, `no`, or `off`,
  in any case, is for a build that compiles with the same LLVM Flang
  version that SALT-FM parses with. The Fortran parse then reads and
  writes `.mod` files instead of `.saltmod` files, so it finds the
  modules of the build and of its libraries, such as `use mpi`, and the
  intrinsic modules of the Flang that runs. When Flang refuses a module
  file, `fparse-llvm` adds a note that the build and its libraries must
  use SALT-FM's Flang version
  ([#40](https://github.com/ParaToolsInc/SALT-FM/issues/40)).
- `fparse-llvm` passes `-fdefault-real-8`, `-fdefault-integer-8`, and
  `-fdefault-double-8` to Flang instead of removing them, so a generic
  call resolves to the same specific procedure as in the build. It
  still removes `-fdefault-double-8` without `-fdefault-real-8`, which
  Flang refuses and gfortran accepts
  ([#40](https://github.com/ParaToolsInc/SALT-FM/issues/40)).
- `SALT_FORTRAN_VERBOSE` follows the rule of the other on/off
  environment variables: `false`, `no`, and `off`, in any case, now
  turn the Flang plugin's messages off, as empty and `0` do.
- The Flang plugin builds and runs with LLVM and Flang 23: it follows
  the renamed OpenMP parse-tree nodes, finds the intrinsic modules in
  the Clang resource directory, and passes them to the frontend as
  intrinsic module paths
  ([#70](https://github.com/ParaToolsInc/SALT-FM/issues/70)).
- CI builds and tests against LLVM 23 in
  `ghcr.io/paratoolsinc/salt-dev:1.8`, and the README lists that image
  in its supported-versions table
  ([#70](https://github.com/ParaToolsInc/SALT-FM/issues/70)).
- With LLVM 19 to 22, SALT-FM now instruments Fortran sources that use
  `use, intrinsic :: <module>` or `use omp_lib`, and SALT-FM's copies
  of the compiler modules no longer hide a user module of the same
  name, such as a user `iso_fortran_env`
  ([#74](https://github.com/ParaToolsInc/SALT-FM/issues/74)).
- `fparse-llvm` runs the Flang driver that SALT-FM was configured with,
  not the first `flang-new` on `PATH`, and stops with an error when the
  driver's LLVM major version differs from the build's. If that driver
  is missing, it warns and uses `flang` or `flang-new` from `PATH`. The
  new `--flang_exe=<path>` option (also accepted by `saltfm`) and the
  `SALT_FLANG_EXE` environment variable select another driver
  ([#75](https://github.com/ParaToolsInc/SALT-FM/issues/75)).
- The tests write their outputs under `test-output/` in the build
  directory instead of the build root. The new `clean-test-artifacts`
  target removes `test-output/`, and `clean` now also removes
  `test-output/` and CTest's `Testing/` directory. A narrow
  `ctest -R <test>` also runs the setup and producer tests that it needs
  ([#78](https://github.com/ParaToolsInc/SALT-FM/issues/78)).
- `saltfm` exits with an error that names the file when it cannot open
  or write its output, for example a missing directory, an output path
  that is a directory, a read-only file or directory, or a full disk.
  Before, C and C++ instrumentation reported success and left the old
  output in place, and Fortran crashed when it could not open the file
  and failed without naming the file when a write failed.
- For C, C++, and Fortran, `saltfm` writes each output to a temporary
  file in the output's directory and renames it to the output only when
  the run succeeds. A run that fails, for example on a full disk, no
  longer leaves a partial output for TAU's compiler wrapper to compile.
  The output's directory must be writable. An output that is not a
  regular file, such as `/dev/null`, is still written directly
  ([#95](https://github.com/ParaToolsInc/SALT-FM/issues/95)).
- With more than one C or C++ source and no `--tau_output`, `saltfm`
  keeps its outputs only when every source succeeds, so an error in a
  later source, such as a bad configuration file, no longer leaves the
  output of an earlier one
  ([#104](https://github.com/ParaToolsInc/SALT-FM/issues/104)).
- When Clang reports errors in a C or C++ source, `saltfm` now exits
  with a non-zero status and writes no instrumented output. Before, it
  exited with status 0 and wrote output instrumented from the partial
  parse tree that Clang builds after errors, which can miss timers.
  With `--tau_fabricate_unknown_types` (or
  `SALT_FABRICATE_UNKNOWN_TYPES`), for which Clang still reports
  errors, `saltfm` keeps the old behavior
  ([#85](https://github.com/ParaToolsInc/SALT-FM/issues/85)).
- For C, C++, and Fortran, `saltfm` now removes a regular file already
  at the `--tau_output` path before instrumentation starts, so a
  failed run never leaves a stale one for TAU's compiler wrapper to
  compile. It does not remove a read-only file; the run then fails to
  open it for writing and exits with an error. It stops with an error
  when it cannot remove a writable file
  ([#85](https://github.com/ParaToolsInc/SALT-FM/issues/85)).
- When the `--tau_output` path is an input source, the configuration
  file, or the selective instrumentation file, `saltfm` now stops with
  an error before it writes anything, for C, C++, and Fortran; before,
  it overwrote or removed that file
  ([#85](https://github.com/ParaToolsInc/SALT-FM/issues/85)).
- New C and C++ option `--tau_allow_parse_errors` (or
  `SALT_ALLOW_PARSE_ERRORS=1`), off by default, makes `saltfm` warn
  about Clang errors, write output instrumented from the partial parse
  tree that Clang builds, and exit with status 0, as before. If you run
  `saltfm` outside your build, without the project's include paths,
  most sources have Clang errors, so `saltfm` now fails on them; set
  `SALT_ALLOW_PARSE_ERRORS=1` to keep the old behavior.
  `--tau_fabricate_unknown_types` also allows the errors and now prints
  the same warning
  ([#66](https://github.com/ParaToolsInc/SALT-FM/issues/66)).
- When a C or C++ source does not exist, `saltfm` leaves no file at
  the output path, even with `--tau_allow_parse_errors` or
  `--tau_fabricate_unknown_types`; before, it left an empty file.
- `fparse-llvm` matches an argument to the flags it knows only as a
  whole. Before, it also forwarded to Flang an argument that only
  contained one, such as a file name like `my-Stuff.f95` (`-S`) or
  flags such as `-fno-wrapv` (`-w`) and `-fopenmp-simd` (`-fopenmp`),
  which it now drops as unknown flags. It also silently dropped an
  argument that contained `-W` and a letter other than `l`, such as
  the include or module path `-I/path/proj-Wave/inc`, which it now
  forwards.
- When testing is on, configuring stops with an error if the source or
  build directory path contains a space, since TAU's compiler wrappers
  and the test checks cannot handle such paths and the tests would fail.
  Configure with `-DBUILD_TESTING=OFF` to build in such a path.
- On macOS, the Flang plugin builds and runs with Homebrew `flang`,
  which now ships a CMake package. SALT-FM copies the contents of
  Homebrew's intrinsic module files, which are symbolic links; before,
  it copied the links, which did not resolve in the build tree, and
  `use, intrinsic` failed.
- When the file lists of a selective instrumentation file exclude a C or
  C++ source file, `saltfm` writes the source to the output file after a
  `#line 1 "<source>"` line, as the Fortran instrumentor does, so TAU's
  compiler wrappers compile it without timers. Before, it wrote no
  output file, so the wrappers compiled the original source instead.
  C and C++ file list entries now match the source path as given to
  `saltfm`, as the Fortran instrumentor does, and the `#line` lines of
  the output name that path; before, both used Clang's absolute path
  ([#71](https://github.com/ParaToolsInc/SALT-FM/issues/71)).
- The selective instrumentation file parser removes the whitespace at
  the two ends of each line, as TAU does. Before, it removed only the
  leading whitespace, so a `BEGIN_` or `END_` line with trailing spaces,
  tabs, or a CRLF line ending was not recognized, and an entry kept its
  trailing whitespace and did not match. Lines longer than 64 KiB no
  longer stop the parse, and the `BEGIN_INSTRUMENT_SECTION` warning goes
  to stderr on its own line
  ([#71](https://github.com/ParaToolsInc/SALT-FM/issues/71)).
- The C/C++ instrumentor and the Fortran plugin match selective
  instrumentation entries with one wildcard matcher that follows TAU:
  `#` (routines) or `*` (files) matches zero or more characters, `?`
  matches one character, and every other character is literal. Before,
  C and C++ built regular expressions, so `.`, `[`, and `$` in an entry
  had their regular expression meanings, `?` was literal in routines and
  optional in files, and results varied with the C++ library. Fortran
  treated `?` as literal and matched file entries only against the
  basename, so an entry with a `/` never matched; such an entry now
  matches the whole source path
  ([#71](https://github.com/ParaToolsInc/SALT-FM/issues/71)).
- Fortran routine names in selective instrumentation lists ignore case,
  so `COMPUTE` selects subroutine `compute`. File entries and C and C++
  routine entries stay case-sensitive. TAU matches Fortran names with
  case, so this differs from TAU
  ([#71](https://github.com/ParaToolsInc/SALT-FM/issues/71)).
- The C/C++ instrumentor honors a selective instrumentation include list
  (`BEGIN_INCLUDE_LIST`): when it is not empty, only the routines it
  matches get timers, as in TAU and the Fortran plugin. Before, it only
  opted inline functions in to instrumentation
  ([#46](https://github.com/ParaToolsInc/SALT-FM/issues/46)).
- When a selective instrumentation file excludes the main program, an
  include list leaves it out, or, with
  `--tau_excluded_main_setup=file`, the file lists exclude its file,
  SALT-FM removes only main's timer and, unless
  `--tau_excluded_main_setup=none`, keeps its setup code, which for TAU
  is `TAU_INIT` and `TAU_PROFILE_SET_NODE(0)`, or `TAU_PROFILE_INIT` and
  `TAU_PROFILE_SET_NODE(0)` in Fortran, and for perfstubs and ITT
  `PERFSTUBS_INITIALIZE()`. Before, C and C++ removed the setup code
  too, so a program run without MPI or `tau_exec` wrote
  `profile.-1.0.0` and a threaded program could fail with timer errors;
  Fortran ignored the routine lists for the main program. SALT-FM warns
  when an include list that is not empty leaves out the main program
  and no exclude list entry names it, unless
  `--tau_excluded_main_setup=none`
  ([#71](https://github.com/ParaToolsInc/SALT-FM/issues/71)).
- Configuration files give main's setup code apart from its timer: the
  new optional `main_setup_insert` is written at the start of `main`
  whether or not main is instrumented, and `main_insert` and
  `main_insert_scope` now hold only main's timer, written after the
  setup code. The new optional `main_setup_insert_noargs` replaces
  `main_setup_insert` for a `main` without parameters; it takes the
  place of SALT-FM's built-in replacement of `TAU_INIT(&argc, &argv);`
  with a comment, so a custom file that writes that call needs it. For
  Fortran, the new optional `program_setup_insert` likewise takes the
  setup code and the declarations out of `program_insert`. A custom
  configuration file whose `main_insert` or `program_insert` still
  holds setup code keeps working for an instrumented main, but that
  code is not written when the selective instrumentation lists leave
  main out; move it to the new entries. In TAU's output, main's timer
  now comes after `TAU_INIT` and `TAU_PROFILE_SET_NODE(0)`
  ([#71](https://github.com/ParaToolsInc/SALT-FM/issues/71)).
- Quotes in a selective instrumentation file list entry are part of the
  pattern, as in TAU, so `"*.c"` matches only a file name that has the
  quotes. SALT-FM warns about each file entry that starts with `"`,
  naming the line. Before, it removed the quotes, as it still does in
  routine lists
  ([#71](https://github.com/ParaToolsInc/SALT-FM/issues/71)).
- On macOS, the first configure of a new build directory applies the
  deployment target raised to match the LLVM library. Before, only a
  second configure did, so the first build linked for macOS 11.0 and ld
  printed hundreds of "built for newer 'macOS' version" warnings.
- When a selective instrumentation file leaves out a C or C++ routine,
  including main, its `return` lines stay as written. Before, a `return`
  that shared its line with other code, as in `int f(void) { return 1; }`
  or `if (n) return 1;`, was written as the code before it followed by
  the whole line again, which nested a function definition or repeated
  the condition, and with `--tau_use_cxx_api` the rest of a one-line
  routine was dropped
  ([#71](https://github.com/ParaToolsInc/SALT-FM/issues/71)).
- By default, SALT-FM no longer parses a source that the file lists of
  a selective instrumentation file exclude: it copies the source after
  a `#line 1` line, so a Clang or Flang error in the source no longer
  makes `saltfm` fail, and excluding a file works around a source that
  SALT-FM cannot parse. The main program in such a source gets no
  setup code, so a program run without MPI or `tau_exec` writes
  `profile.-1.0.0`; SALT-FM warns when an excluded source seems to
  contain the main program (in Fortran, a `program` statement). The
  new option `--tau_excluded_main_setup=<none|routine|file>` (or
  `SALT_EXCLUDED_MAIN_SETUP`; default `routine`) selects which excluded
  main programs get setup code: `file` parses all sources and gives
  such a main its setup code, and `none` also gives no setup code to a
  main that the routine lists leave out
  ([#88](https://github.com/ParaToolsInc/SALT-FM/issues/88)).
- Unless `--tau_excluded_main_setup=file`, SALT-FM still parses an
  excluded Fortran source that seems to define a module, or that holds
  an INCLUDE line or an `#include`, which can bring one in, so that
  Flang writes the module files that other sources need; if Flang
  fails, it warns and copies the source. The new option
  `--tau_parse_excluded_modules=<off|modules-only|submodules-and-modules>`
  (or `SALT_PARSE_EXCLUDED_MODULES`; default `modules-only`) turns this
  parse off, or also parses a source that seems to define a submodule,
  whose module file only its own child submodules need. SALT-FM installs
  a new helper program, `salt-selectfile`, that `fparse-llvm` runs to
  apply the file lists before Flang runs
  ([#88](https://github.com/ParaToolsInc/SALT-FM/issues/88)).
- `saltfm` and `fparse-llvm` find their own directory without the
  `dirname` program, and also when the path they are run by uses `\`
  separators, as under MSYS
  ([#88](https://github.com/ParaToolsInc/SALT-FM/issues/88)).
- In C and C++ output, the file name of the `#line` lines escapes `\`
  and `"`, and writes a line break as `\n`, so that compiler messages,
  `__FILE__`, and debug information name a source whose path holds
  them. Before, a `"` broke the `#line` line, and the compiler read a
  `\` as the start of an escape. Fortran output keeps the name as
  written, since Flang and ifx read it that way; gfortran reads
  escapes, and Flang rejects a `"`
  ([#88](https://github.com/ParaToolsInc/SALT-FM/issues/88),
  [#97](https://github.com/ParaToolsInc/SALT-FM/issues/97)).
- `${full_timer_name}` is written for its string literal. In C and C++,
  a `\` comes before each `\` and `"` of the file path and the
  signature, such as `operator""_km` or `S<'\n'>`, and a line break is
  written as `\n`. In Fortran, each `"` is doubled, and the split name
  never breaks the pair across lines.
  Before, such a path or signature broke the inserted code or changed
  the timer name, in Fortran a `$&` or `$1` in the path was replaced as
  a regex format escape, and in C and C++ a `${full_timer_name}` in
  the path or signature made `saltfm` hang
  ([#88](https://github.com/ParaToolsInc/SALT-FM/issues/88)).
- `saltfm` and `fparse-llvm` take their options only from their
  arguments and the documented `SALT_*` environment variables. Before,
  an environment variable with the name of one of their shell
  variables, such as `show`, `tool`, or `output_file`, changed what
  they did; with `output_file` and no `--tau_output`, `fparse-llvm`
  removed and overwrote the file that it named. Without an input file,
  `fparse-llvm` now stops with `ERROR: no input file`, and `--show`
  prints the command; before, both stopped on an unset shell variable.
- For C, C++, and Fortran, `saltfm` stops with an error, exit status 1,
  and no output when a source needs the configuration file, and the
  file is missing, cannot be read, does not parse, lacks a key that the
  run needs, or has a value of the wrong type. The error names the
  file, and the bad key or the parse error position. Before, `saltfm`
  crashed, or wrote broken output and exited with status 0. Fortran
  exited with status 253 for a missing file or key, and 252 for an
  unreadable selective instrumentation file
  ([#51](https://github.com/ParaToolsInc/SALT-FM/issues/51),
  [#54](https://github.com/ParaToolsInc/SALT-FM/issues/54),
  [#83](https://github.com/ParaToolsInc/SALT-FM/issues/83),
  [#90](https://github.com/ParaToolsInc/SALT-FM/issues/90)).
- The configuration file can use YAML aliases, such as `include: *x`,
  and can start with the YAML document marker `---`.
- C and C++ no longer need the `instrumentation` key in the
  configuration file, which only names the instrumentation in the log.
- Without `--config_file`, Fortran also reads the configuration file
  that `SALT_CONFIG_FILE` names, as C and C++ do. Before, Fortran
  ignored it and read the installed `config.yaml`. An empty
  `SALT_CONFIG_FILE` selects the installed file, as an unset one does;
  before, C and C++ stopped with an error. Fortran stops with an error
  for an empty `--config_file=` or a `--config_file` without a value, as
  C and C++ do; before, it read `config_files/tau_config.yaml` in the
  working directory, or the default file.
- `saltfm` stops with an error and writes no output when
  `--tau_output` or `--tau_select_file` has an empty value, or ends
  the arguments without one. Before, it wrote the default output file
  or instrumented every routine; C and C++ stopped only for a missing
  value. When an option error stops C or C++, `saltfm` now also
  removes a stale file at the `--tau_output` path.
- SALT-FM copies a Fortran source that the file lists exclude without
  reading the configuration file, so a bad one does not stop the copy
  ([#90](https://github.com/ParaToolsInc/SALT-FM/issues/90)).
- For C and C++, `saltfm` stops with an error and writes no output when
  it cannot read the selective instrumentation file. Before, it
  instrumented every routine and exited with status 0. Fortran now
  reports this error in one line, as C and C++ do. A directory given as
  the selective instrumentation file is now this error in C, C++, and
  Fortran; before, all three instrumented every routine.
- `saltfm` and `fparse-llvm` read a source as Fortran if its extension
  is `.f`, `.for`, `.ftn`, `.f77`, `.fpp`, `.f90`, `.f95`, `.f03`,
  `.f08`, or `.f18`, in lowercase or uppercase (but not `.F77`). Before,
  they knew only `.f`, `.f90`, and `.f03`, and their uppercase forms.
  Without such a source, `fparse-llvm` takes the first argument that is
  not a flag as the input file; before, it could take a flag. A flag
  that ends in a Fortran extension, such as `-DSRC=util.f90`, is no
  longer taken as the input file; before, Flang read standard input,
  and `saltfm` could leave an empty output. Without
  `--tau_output`, the output name of a fixed-form source ends in
  `.inst.F`, and that of a free-form source in `.inst.F90`
  ([#102](https://github.com/ParaToolsInc/SALT-FM/issues/102)).
- `saltfm` takes the language only from a source, not from an option
  value such as `--tau_output=out.inst.F90` or
  `--tau_output out.inst.F90`.
- C and C++ ignore `--flang_exe`; before, they stopped with an
  unknown-argument error.
- The README instructions work again: the example runs
  `build/bin/saltfm` and TAU's compiler wrappers, and the TAU
  configuration uses `-dwarf=download`. Before, the example used a
  wrong path and defines that `CMakeLists.txt` no longer has, and TAU's
  `configure` rejected `-libdwarf=download`.

## [0.5.0] - 2026-10-09

- New C/C++ option `--tau_fabricate_unknown_types` (or
  `SALT_FABRICATE_UNKNOWN_TYPES=1`), off by default, fabricates
  declarations for unknown namespaces, class templates, and qualified
  types, so functions whose signatures use types the parser cannot
  resolve are still instrumented. Timer names use the spelled types
  when a type is invalid, so unknown types no longer show up as `int`
  ([#67](https://github.com/ParaToolsInc/SALT-FM/pull/67) by @nchaimov).
- `--tau_instrument_inline` now takes effect; it was stored in a
  per-translation-unit `static` in a header, so the instrumentor never
  saw it
  ([#69](https://github.com/ParaToolsInc/SALT-FM/pull/69) by @nchaimov):
  - [#68](https://github.com/ParaToolsInc/SALT-FM/issues/68) - C++
    methods defined inside the class body can now be instrumented with
    `--tau_instrument_inline` (reported by @giltirn).
- Selective instrumentation files keep the TAU/PDT syntax: a leading `#`
  inside `BEGIN_`/`END_` list blocks still starts a comment, and a
  pattern starting with the `#` wildcard must be quoted (`"#foo#"`).
  SALT-FM now warns when a comment line in a routine include or exclude
  list could be an unquoted pattern (`#` followed directly by anything
  but whitespace or one of a few punctuation characters used in rulers,
  such as `=`, `/` or `|`) and suggests quoting it; comments such as
  `# note` or `#=====` stay silent. Suspicious quoting in any list
  block, FILE lists included (missing closing quote, non-blank text after
  the closing quote, empty quoted pattern), also emits a warning instead
  of silently altering the entry
  ([#65](https://github.com/ParaToolsInc/SALT-FM/pull/65) by @zbeekman):
  - [#64](https://github.com/ParaToolsInc/SALT-FM/issues/64) - exclude
    list entries starting with an unquoted wildcard were silently
    dropped as comments (reported by @giltirn).
- `spack.yaml` lists its compilers as package externals, since Spack's
  `develop` branch (the upcoming 1.3), which CI uses through
  `setup-spack`, removed the `compilers` section; released Spack 0.23
  and later already read compiler externals
  ([#65](https://github.com/ParaToolsInc/SALT-FM/pull/65) by @zbeekman).

## [0.4.1] - 2026-05-12

- macOS build robustness: configure-time auto-detection of
  `CMAKE_OSX_SYSROOT`, `CMAKE_PREFIX_PATH`, and `-stdlib=libc++`;
  deployment target aligned with the LLVM dylib (cached with `FORCE`
  so the raised value persists across re-configures and is visible to
  cmake-gui / ccmake); sysroot and resource-dir injected for
  `cparse-llvm` LibTooling; fail-fast `<ryml_all.hpp>` configure-time
  probe so misconfigured toolchains surface at configure rather than
  mid-build
  ([#61](https://github.com/ParaToolsInc/salt/pull/61) by @zbeekman):
  - [#60](https://github.com/ParaToolsInc/salt/issues/60) - macOS
    toolchain auto-detect plus fail-fast on misconfigured SDK / libc++
    setups.
- CMake test refactor: `compile_instrumented` split into focused
  helpers; `${CMAKE_BINARY_DIR}/{GCC,LLVM}` namespacing applied to TAU
  test directories; profile-dir and run-check fixtures shared across
  C/C++ and Fortran (Fortran TAU tests gain the `rm_old` / `mkdir`
  fixtures they were missing); ctest labels added for grouping;
  GCC/LLVM compiler-classifier regexes anchored so `clang++` no
  longer matches the `g++` alternative and gets misrouted into the
  GCC build subdir
  ([#61](https://github.com/ParaToolsInc/salt/pull/61) by @zbeekman):
  - [#28](https://github.com/ParaToolsInc/salt/issues/28) - CMake test
    logic cleanup (folding C/C++ and Fortran into a single helper was
    dropped as too broad; flag handling diverges).
- New `macos-latest` GitHub Actions job using Homebrew LLVM and Flang
  (no TAU on this leg)
  ([#61](https://github.com/ParaToolsInc/salt/pull/61) by @zbeekman).
- TAU-on-macOS dispatch fixes: `-optShared` forced for TAU tests;
  `tau_exec -T` tags derived per TAU variant; `TAU_ROOT` cache-shadow
  bug fixed; TAU's `bin/` directory prepended to ctest `PATH` so
  `tau_exec` / `tau_cc.sh` / `tau_f90.sh` can resolve their sibling
  helpers (`tau-config`, `tau_compiler.sh`) in in-tree TAU installs,
  which on macOS is the common layout
  ([#61](https://github.com/ParaToolsInc/salt/pull/61) by @zbeekman).
- `build_and_test.sh` hardened on macOS: honors caller-provided
  `CC` / `CXX`, only auto-selects Homebrew `llvm`+`flang` on Darwin
  when both formulae are installed and only fills in the env vars that
  were not set explicitly; passes `--no-tests=error` so an empty test
  set is a failure. Bash 3.2 empty-array `set -u` forwarding fixed in
  the `saltfm` and `fparse-llvm` wrappers
  ([#61](https://github.com/ParaToolsInc/salt/pull/61) by @zbeekman).
- User-facing strings in README, CMake status / error messages, and
  the `saltfm` / `fparse-llvm` wrapper banners normalized to the
  canonical `SALT-FM` spelling
  ([#61](https://github.com/ParaToolsInc/salt/pull/61) by @zbeekman,
  refs [#48](https://github.com/ParaToolsInc/salt/pull/48)).

## [0.4.0] - 2026-05-08

- macOS build support and LLVM 22 added to the CI test matrix; bumps
  `.VERSION` to 0.4.0 and makes `cparse-llvm --version` report the SALT-FM
  version (previously only the LLVM banner)
  ([#48](https://github.com/ParaToolsInc/salt/pull/48) by @zbeekman).
- F2018 separate-module-procedure (R1538) support in the Flang plugin,
  including pure/elemental skipping for the body form whose attributes
  live on the parent module's interface
  ([#48](https://github.com/ParaToolsInc/salt/pull/48) by @zbeekman):
  - [#33](https://github.com/ParaToolsInc/salt/issues/33) - regression
    coverage confirming Fortran 2008 submodule procedures are
    instrumented in submodule bodies and that the parent module's
    bodyless interface declarations remain untouched.
  - [#58](https://github.com/ParaToolsInc/salt/issues/58) - pure and
    elemental detection on separate-module-procedure bodies
    (`module procedure foo ... end procedure foo`) via the resolved
    Symbol, including the cross-`.mod`-file case. Closes the body-form
    branch of #36; calling-side handling still TODO.
- Fortran instrumentor robustness fixes
  ([#48](https://github.com/ParaToolsInc/salt/pull/48) by @zbeekman):
  - [#31](https://github.com/ParaToolsInc/salt/issues/31) - no longer
    aborts when a labeled CONTINUE terminates a label-DO at the end of
    a procedure body; the body's end line is now captured from the
    wrapping End statement instead of the final construct.
  - [#39](https://github.com/ParaToolsInc/salt/issues/39) - handles
    multi-line `&` continuations inside `if (...) return` statements.
  - Instruments `return <expr>` (the obsolescent F2018 Annex B.3
    alternate-return form) so existing TAU timer starts get matching
    stops. Refs #32.
  - Numeric statement labels on `if (cond) return` are preserved on
    the synthesized `if (cond) then` block (F2018 R601/R863), so any
    `goto N` references in the surrounding procedure continue to
    resolve.
  - Implicit main programs (no `program` statement; F2018 R1101 makes
    the program-stmt optional) get a synthesized timer name combining
    the input file's basename with a fixed `._implicit_main_` marker,
    and a real start line from the first executable statement,
    instead of the previous empty name and `{0,...}` source range.
- TAU timer source-range field now emits real start/end columns for
  both fparse-llvm (PDT-style inclusive last-char convention) and
  cparse-llvm (signature start to body close brace), instead of the
  previously hardcoded `,1}` placeholders
  ([#48](https://github.com/ParaToolsInc/salt/pull/48) by @zbeekman,
  closes [#49](https://github.com/ParaToolsInc/salt/issues/49)
  and [#50](https://github.com/ParaToolsInc/salt/issues/50)).
- Regression coverage for cparse-llvm's bodyless `FunctionDecl` handling
  (forward declarations, pure-virtual methods); paired with a stub TU
  so the instrumented binary also links and runs end-to-end
  ([#48](https://github.com/ParaToolsInc/salt/pull/48) by @zbeekman,
  refs [#53](https://github.com/ParaToolsInc/salt/issues/53)).
- LLVM 20, 21, and 22 build compatibility while preserving LLVM 19 support,
  with the CI matrix expanded against `salt-dev:1.3` (LLVM 19), `:1.4`
  (LLVM 20), and `:1.5` (LLVM 21), and a `spack concretize` smoketest
  ([#45](https://github.com/ParaToolsInc/salt/pull/45) by @zbeekman):
  - [#34](https://github.com/ParaToolsInc/salt/issues/34) - Fortran build
    support for LLVM 20.
  - [#30](https://github.com/ParaToolsInc/salt/issues/30) - `-Werror`
    restricted to Debug builds so release configurations are not blocked
    by new compiler warnings.
- Selective Instrumentation File (SIF) matcher correctness and coverage
  ([#45](https://github.com/ParaToolsInc/salt/pull/45) by @zbeekman):
  - [#47](https://github.com/ParaToolsInc/salt/issues/47) - Fortran SIF
    matcher now requires a full-string match instead of a substring search,
    so routines and files with overlapping names are no longer incorrectly
    included or excluded.
  - [#29](https://github.com/ParaToolsInc/salt/issues/29) - added tests
    covering SIF routine whitelist/blacklist, file-level include/exclude,
    combined INCLUDE+EXCLUDE semantics, and glob handling.
  - Preserves `*` when converting SIF globs to regular expressions in the
    Flang plugin (regression caught while fixing #47).
- Test-suite resilience: TAU-dependent tests are skipped instead of failing
  the build when the required TAU Makefiles are missing
  ([#45](https://github.com/ParaToolsInc/salt/pull/45) by @zbeekman, closes
  [#44](https://github.com/ParaToolsInc/salt/issues/44) reported by
  @nchaimov).
- Project version is now derived from a committed `.VERSION` file plus
  `git describe` build metadata, replacing hardcoded version constants
  that had drifted to `0.2` after v0.3.0 shipped. A vendored `.githooks/`
  directory (auto-installed by `cmake` configure) and a tag-push CI
  workflow guard against `.VERSION`/tag drift
  ([#45](https://github.com/ParaToolsInc/salt/pull/45) by @zbeekman).
- C++ instrumentation API now reads entries from the configuration file,
  with new scoped variants `main_insert_scope` and
  `function_begin_insert_scope`; minimum required CMake version raised to
  3.23.0 for `FILE_SET`
  ([#43](https://github.com/ParaToolsInc/salt/pull/43) by @nchaimov):
  - [#41](https://github.com/ParaToolsInc/salt/issues/41) - C++ API
    instrumentation now emits `TAU_PROFILE_SET_NODE`.

## [0.3.0] - 2025-02-04

- Fortran module files are handled correctly during instrumentation
  ([#38](https://github.com/ParaToolsInc/salt/pull/38) by @zbeekman).
- Timer names are correct for internal procedures inside Fortran main
  programs ([#37](https://github.com/ParaToolsInc/salt/pull/37) by
  @zbeekman).

## [0.2.0] - 2025-01-28

First public SALT-FM milestone: Fortran instrumentation via the Flang
plugin in addition to the existing C/C++ tooling.

- SALT-FM Fortran instrumentation: Flang plugin, `fparse-llvm` driver, and
  a `saltfm` wrapper script that selects the C/C++ or Fortran instrumentor
  based on source language. Includes selective instrumentation
  (`FILE_INCLUDE_LIST` / `FILE_EXCLUDE_LIST`, routine-level include and
  exclude), `#line` directives in instrumented Fortran output, and
  instrumentation for `if (...) return` statements
  ([#25](https://github.com/ParaToolsInc/salt/pull/25) by @zbeekman, with
  major Fortran-side work by @nchaimov).
- TAU is now optional when building SALT-FM
  ([#25](https://github.com/ParaToolsInc/salt/pull/25) by @zbeekman).
- Funding acknowledgement section added to the README
  ([#27](https://github.com/ParaToolsInc/salt/pull/27) by @nchaimov).
- Project LICENSE ([#26](https://github.com/ParaToolsInc/salt/pull/26) by
  @zbeekman).

## Pre-tag history

Initial public development of the C/C++ instrumentor before any release
tag, ending with the v0.2.0 SALT-FM milestone.

- Refactor of TAU's instrumentor into a standalone repository with a
  frontend / instrumentor API
  ([#7](https://github.com/ParaToolsInc/salt/pull/7) by @vikram8128):
  - [#6](https://github.com/ParaToolsInc/salt/issues/6) - frontend-related
    functions split into a separate source file.
- Configurable instrumentation driven by a YAML configuration file,
  replacing hard-coded instrumentation
  ([#9](https://github.com/ParaToolsInc/salt/pull/9) by @PlatinumCD):
  - [#1](https://github.com/ParaToolsInc/salt/issues/1) - replaced
    hard-coded instrumentation with config-file-driven instrumentation.
- CMake-based build system replacing the previous custom configure script
  ([#10](https://github.com/ParaToolsInc/salt/pull/10) by @zbeekman):
  - [#4](https://github.com/ParaToolsInc/salt/issues/4) - switched build
    system to CMake.
- `--config_file` flag added to `cparse-llvm` so the configuration file
  can be supplied explicitly
  ([#12](https://github.com/ParaToolsInc/salt/pull/12) by @zbeekman):
  - [#11](https://github.com/ParaToolsInc/salt/issues/11) - config file
    can now be passed via flag instead of being implicit.
- `-DTAU_ROOT=<dir>` option for tests to point at an alternate TAU
  installation ([#14](https://github.com/ParaToolsInc/salt/pull/14) by
  @wspear).
- `TAU_PROFILE_SET_NODE` correctly emitted from `main` for C and C++
  instrumentation
  ([#22](https://github.com/ParaToolsInc/salt/pull/22) by @zbeekman):
  - [#21](https://github.com/ParaToolsInc/salt/issues/21) - C/C++ SALT-FM now
    injects instrumentation correctly into `main`.
- Segfault fixed when the trailing `--` argument was omitted from
  `cparse-llvm` invocations
  ([#5](https://github.com/ParaToolsInc/salt/pull/5) by @vikram8128).
- Build fixed under Clang 15 with stricter pedantic diagnostics
  ([#13](https://github.com/ParaToolsInc/salt/pull/13) by @khuck).

[Unreleased]: https://github.com/ParaToolsInc/SALT-FM/compare/v0.5.0...HEAD
[0.5.0]: https://github.com/ParaToolsInc/SALT-FM/compare/v0.4.1...v0.5.0
[0.4.1]: https://github.com/ParaToolsInc/SALT-FM/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/ParaToolsInc/SALT-FM/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/ParaToolsInc/SALT-FM/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/ParaToolsInc/SALT-FM/releases/tag/v0.2.0
