[![CI](https://github.com/ParaToolsInc/SALT-FM/actions/workflows/CI.yaml/badge.svg)](https://github.com/ParaToolsInc/SALT-FM/actions/workflows/CI.yaml)

SALT-FM: An LLVM-based Source Analysis Toolkit for HPC
======================================================

The [TAU Performance System] is a powerful and versatile performance monitoring and analysis system.
TAU supports several mechanisms for instrumenting applications,
including source-based instrumentation.
Source-based instrumentation inserts instrumentation points around areas of interest
by parsing and modifying an application's source code.
However, source-based instrumentation can be difficult to use,
especially for large code-bases written in modern languages such as C++ and Fortran.
To improve the usability of TAU,
we introduce a next-generation source analysis toolkit called SALT-FM.
SALT-FM is built using LLVM compiler technology,
including the libTooling library, Clang's C and C++ parsers, and f18/Flang's Fortran parser.
SALT-FM supports the following LLVM/Clang versions, each tested in CI against a matching [salt-dev] container image.
The same images are also published to [Docker Hub][salt-dev Docker Hub] under the `paratools/salt-dev` repository.

| LLVM/Clang version | GitHub Container Registry           | Docker Hub                |
| ------------------ | ----------------------------------- | ------------------------- |
| 19                 | `ghcr.io/paratoolsinc/salt-dev:1.3` | `paratools/salt-dev:1.3`  |
| 20                 | `ghcr.io/paratoolsinc/salt-dev:1.4` | `paratools/salt-dev:1.4`  |
| 21                 | `ghcr.io/paratoolsinc/salt-dev:1.5` | `paratools/salt-dev:1.5`  |
| 22                 | `ghcr.io/paratoolsinc/salt-dev:1.6` | `paratools/salt-dev:1.6`  |
| 23                 | `ghcr.io/paratoolsinc/salt-dev:1.7` | `paratools/salt-dev:1.7`  |

## Getting Started

> [!NOTE]
> Configuring SALT-FM inside a git checkout (i.e. running `cmake` for the first
> time on a clone) sets `core.hooksPath` in that clone's local git config to
> point at the vendored `.githooks/` directory, so contributor-side
> `pre-commit` and `pre-push` hooks become active automatically. The hooks
> are advisory and version-controlled with the rest of the source tree; they
> are not installed system-wide. Distribution build recipes and other
> packaging contexts should disable this -- see [Notes for package
> maintainers](#notes-for-package-maintainers).

The build system for SALT-FM uses [CMake] as the build system generator
(also known as a meta-build system).
It also requires either a full installation of LLVM and Clang including the
`clang-cmake-exports` and `cmake-exports` components, or a more minimal set of features and components
(including both of the aforementioned `cmake-exports`) together with a patch applied to
two of the LLVM/Clang installed CMake files.

The development and testing of SALT-FM utilizes the [salt-dev] container,
which is [posted to Docker Hub].
The container image includes an installation of `git`, `cmake`, `llvm`/`clang`, and `ninja`.
The installed `llvm` and `clang` include the patched [CMake] files.

Below are the steps required to get started using SALT-FM:

### 1. Clone this repo:

``` shell
git clone --recursive https://github.com/ParaToolsInc/SALT-FM.git
```
Including the `--recursive` flag will ensure you have the patches
that are applied to the `llvm`/`clang` installed CMake files available to you.
This is not a requirement if using the [salt-dev] container image.
The patches prevent SALT-FM's configuration with CMake from failing when
incomplete/minimal installations of LLVM and Clang are present.
A minimal subset of components must be installed.
If SALT-FM is failing to be configured in the next step,
and you have write access to the LLVM and/or Clang installation,
applying the patches from https://github.com/ParaToolsInc/salt-llvm-patches
may fix the problem.

### 2. Launch the salt-dev Docker image (optional)

``` shell
$ docker pull paratools/salt-dev:latest
$ docker run -it --pull always --tmpfs=/dev/shm:rw,nosuid,nodev,exec \
    --privileged -v $(pwd):/home/salt/src paratools/salt-dev:latest
# cd /home/salt/src
# ccache --show-stats
```
This step is optional but recommended.
The `llvm`/`clang` CMake files are patched, and this is the environment used in CI testing.
The docker image also includes `ccache` and two configurations of TAU
for testing with both GCC and Clang.
(See `/usr/local/x86_64/lib` for the two installed `TAU_MAKEFILE`s.)

### 3. Configure and build SALT-FM:

This step is most easily performed in the [salt-dev] container,
but can also be performed if a suitable installation of LLVM and Clang is present.

``` shell
# Tell CMake to use the clang/clang++ compiler to build SALT-FM
export CC=clang
export CXX=clang++
# configure SALT-FM and generate build system
cmake -Wdev -Wdeprecated -S . -B build
cmake --build build --parallel # Add --verbose to debug something going wrong
```
To specify a TAU installation for testing other than the one used by the [salt-dev] container,
you can add `-DTAU_ROOT=<dir>` to the first cmake invocation.

If the Ninja build system is present and you prefer it to Makefiles
(it's already present in the [salt-dev] container image),
then you may add `-G Ninja` to the penultimate line above:

``` shell
cmake -Wdev -Wdeprecated -S . -B build -G Ninja
```

#### macOS (Homebrew LLVM 23)

Apple's bundled clang does not ship `flang`. Install the Homebrew formulae
([instructions](https://formulae.brew.sh/formula/llvm) and
[flang](https://formulae.brew.sh/formula/flang)):

``` shell
brew install llvm flang ninja
```

Homebrew `flang` 23.1.3_1 and later ship a CMake package and the Flang
headers, so SALT-FM also builds the Flang plugin. With an older
`flang`, run `brew upgrade flang`; otherwise SALT-FM builds without the
Flang plugin (C/C++ only).

Configure and build the same way as on Linux, pointing the compiler at
the Homebrew LLVM clang so `flang-new` is reachable:

``` shell
cmake -Wdev -Wdeprecated -G Ninja -S . -B build \
  -DCMAKE_C_COMPILER="$(brew --prefix llvm)/bin/clang" \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

CMake auto-detects the rest:

- `CMAKE_PREFIX_PATH` is seeded from the active C++ compiler's prefix,
  `llvm-config`, and `flang-new` (no Homebrew/Spack/conda queries) so
  `find_package(LLVM)` resolves without `-DCMAKE_PREFIX_PATH=...`.
- `CMAKE_OSX_SYSROOT` falls back to the active Xcode SDK; you do not
  need to pass it explicitly.
- A configure-time `try_compile` of `<ryml_all.hpp>` with the resolved
  toolchain catches misconfiguration with an actionable error before
  the real build starts.

Notes:

- `CMAKE_OSX_DEPLOYMENT_TARGET` defaults to `11.0` and is auto-raised
  to match the linked LLVM dylib's `LC_BUILD_VERSION minos` if higher
  (avoids ld "object file built for newer macOS version" warnings).
- If TAU is not available, pass `-DSALT_REQUIRE_TAU=OFF`; TAU-dependent
  tests are skipped automatically.

Once this is finished, you will have an executable `cparse-llvm` in the `build` directory.

### 4. Running the tests (optional):

Running the tests is encouraged to verify functionality.

``` shell
cd build
ctest --output-on-failure
```

The tests are all located in the `tests` subdirectory of the project.
[CMake] test fixtures and test dependencies ensure that:

 * The tests run in an order such that inter-test dependencies are satisfied
 * The SALT-FM configuration files are correctly staged to the build directory
 * Old instrumented versions of test sources are cleaned up just in time before being instrumented again
   * This is so the user can inspect instrumented sources
 * Old object files are cleaned up
 * Old profiles associated with each test are removed just in time
 * New profiles are moved to subdirectories indicating which test they are associated with

The tests write all their outputs (instrumented sources, logs, TAU binaries,
and profiles) under `test-output/` in the build directory.
`cmake --build build --target clean-test-artifacts` removes `test-output/` and
keeps the built tools; `cmake --build build --target clean` also removes the
compiled tools (`cparse-llvm` and the Flang plugin) and CTest's `Testing/`
directory.
A narrow `ctest -R <test>` also runs the setup and producer tests that it needs.

TAU's compiler wrappers and the test checks cannot handle spaces in paths, so
with testing on, configuring fails if the source or build directory path
contains a space.
Use paths without spaces, or configure with `-DBUILD_TESTING=OFF`.

By default, the tests assume a TAU installation matching the [salt-dev]
development image (located at `/usr/local/x86_64/` with a GCC and Clang configuration).

To use a different TAU installation, add `-DTAU_ROOT=<dir>` to the cmake invocation.
The specified directory should point to a TAU install built with at least these two configurations:

 `-pthread -cc=clang -c++=clang++ -bfd=download -unwind=download -dwarf=download -otf=download`

 and

 `-pthread -bfd=download -unwind=download -libdwarf=download -otf=download`

### 5. Example usage:

```
# Point to the appropriate config file if config_files/config.yaml is not in CWD
./cparse-llvm --config_file=/path/to/config.yaml ../tests/hello.c
```

This will produce a file `hello.inst.c`.
Passing the correct defines (see the [CMakeLists.txt file]) and
setting `TAU_MAKEFILE=/usr/local/x86_64/lib/Makefile.tau-pthread` will allow you
to compile `hello.inst.c` with `tau_cc.sh -optLinkOnly -D... hello.inst.c -o hello`.
Running `tau_exec ./hello` should then produce a `profile.0.0.0` file.

For Fortran, `fparse-llvm` (which `saltfm` runs for Fortran sources)
runs the Flang driver that SALT-FM was configured with, even if another
`flang` or `flang-new` comes first on `PATH`. To run a different driver,
pass `--flang_exe=<path>` or set `SALT_FLANG_EXE=<path>`; the option
takes priority. If the configured driver is missing, `fparse-llvm` warns
and uses `flang` or `flang-new` from `PATH`. The driver must report the
LLVM major version SALT-FM was built with, or `fparse-llvm` stops with
an error.

### C and C++ sources with errors

When Clang reports errors in a C or C++ source, `saltfm` exits with a
non-zero status and writes no output file. After many errors, such as
a missing header or an unknown type name, Clang marks declarations
that use unknown names invalid and keeps parsing. These options make
`saltfm` insert timers in the original source from the partial parse
tree that Clang builds, for example to instrument sources outside
their build, without the project's include paths:

- `--tau_allow_parse_errors`, or `SALT_ALLOW_PARSE_ERRORS` set to any
  value but empty, `0`, `false`, `no`, or `off` (in any case), makes
  `saltfm` print `WARNING: Clang reported errors; instrumenting the
  partial parse tree`, write the output file, and exit with status 0.
  Timers can then be wrong or missing: Clang uses `int` for an
  unknown type, so the timer of `void scale(RealD factor, ComplexD *z)`
  is named `void scale(int, int *)`, and a function such as
  `void f(std::vector<RealD> &v)` gets no timer.
- `--tau_fabricate_unknown_types`, or `SALT_FABRICATE_UNKNOWN_TYPES`,
  adds declarations for unknown qualified names such as `std::vector`
  to Clang's parse tree in memory, so more functions get timers, and
  the timer names use the types as the source spells them. Clang still
  reports errors for those names, so it also allows the errors, as
  `--tau_allow_parse_errors` does.

These options are for C and C++ only: when Flang reports an error, it
gives no parse tree to instrument, and `saltfm` fails. By default,
SALT-FM does not parse a source that the file lists of a selective
instrumentation file exclude, so its errors do not stop `saltfm` (see
[Selective instrumentation](#selective-instrumentation)).

### Configuration files

The YAML configuration file (`--config_file`; `config_files/` has one
for TAU, perfstubs, ITT, NVTX, and ROCTX) holds all the code that
SALT-FM inserts. `${full_timer_name}` in an entry stands for the timer
name, written for a `"`-delimited string literal: in C and C++, with a
`\` before each `\` and `"`; in Fortran, with each `"` doubled, and
split into pieces of at most 64 characters, each on a continuation
line, so a Fortran entry follows `${full_timer_name}` with `&` and
closes the literal on the next line, as `config_files/tau_config.yaml`
does.

- `include`: lines written at the top of an instrumented C or C++ file.
- `main_setup_insert` (optional): setup code at the start of `main`,
  written even when the selective instrumentation lists leave out
  main's timer, except as the parse levels under
  [Selective instrumentation](#selective-instrumentation) describe.
  `main_setup_insert_noargs` (optional) replaces it for a `main`
  without parameters.
- `main_insert`, or `main_insert_scope` with `--tau_use_cxx_api`: main's
  timer, written after the setup code when main is instrumented.
- `function_begin_insert` (or `function_begin_insert_scope`) and
  `function_end_insert`: the timer of every other function.
- Under `Fortran:`, `program_setup_insert` (optional) and
  `program_insert` do the same for the main program, and
  `procedure_begin_insert` and `procedure_end_insert` for other
  procedures. Since Fortran declarations come before statements, put
  the declarations that the main program's timer needs in
  `program_setup_insert`, as `tau_config.yaml` does.

### Selective instrumentation

`saltfm --tau_select_file=<file>` reads a TAU selective instrumentation
file; TAU's compiler wrappers pass the file given with
`-optTauSelectFile=<file>`.
SALT-FM removes the whitespace at the two ends of each line, including
the carriage return of a CRLF line ending, and has no line length limit.
It matches the entries of the lists as TAU does, in C, C++, and Fortran:

- A line that starts with `#` is a comment, even in a list. To start a
  routine pattern with the `#` wildcard, put the entry in quotes
  (`"#_internal"`); routine lists remove the quotes. In file lists, as
  in TAU, quotes are part of the pattern and SALT-FM warns about them;
  use `?` to match a file name that starts with `#`.
- In routine lists, `#` matches zero or more characters; in file lists,
  `*` does. In both, `?` matches one character, and every other
  character, such as `.`, `[`, or `$`, matches only itself. An entry
  matches the whole name, not a part of it.
- C and C++ routine entries match the routine's signature as the timer
  name shows it, for example `void compute(int)`. Fortran routine
  entries match the bare subprogram name and ignore case, as Fortran
  does; file entries and C and C++ routine entries are case-sensitive.
- A file entry with no `/` matches the basename of the source file. An
  entry with a `/` must match the whole path as given to SALT-FM, which
  is the path as written on the compile line when TAU's compiler
  wrappers run SALT-FM; SALT-FM does not make it absolute or remove a
  leading `./`. In file entries, `*` also matches `/`:

  | Pattern | `src/a.c` | `./src/a.c` | `/abs/src/a.c` |
  |---|---|---|---|
  | `src/a.c` | yes | no | no |
  | `*/src/a.c` | no | yes | yes |
  | `*src/a.c` | yes | yes | yes (also `mysrc/a.c`) |
  | `a.c` (no `/`, basename) | yes | yes | yes |
- An exclude list has priority over an include list. If the include
  list (`BEGIN_INCLUDE_LIST`) is not empty, SALT-FM instruments only the
  routines it matches.
- C and C++ inline functions get a timer only with
  `--tau_instrument_inline` or when they match the include list.
- When the routine lists leave out the main program, SALT-FM removes
  only its timer and still writes its setup code (`main_setup_insert`
  or `Fortran: program_setup_insert`), which for TAU is `TAU_INIT` and
  `TAU_PROFILE_SET_NODE(0)`, or `TAU_PROFILE_INIT` and
  `TAU_PROFILE_SET_NODE(0)` in Fortran, except at the `never` level
  below. Without it, a program run without MPI or `tau_exec` writes
  `profile.-1.0.0`, and a threaded one can fail with timer errors. When
  an include list that is not empty leaves out the main program, and no
  exclude list entry names it, SALT-FM warns that main still gets the
  setup code.

When its file lists exclude the source file, SALT-FM does not parse or
instrument it, by default: it writes the source to the output file
after a `#line 1` line that names it, so TAU's compiler wrappers compile
it without timers, and compiler messages, `__FILE__`, and debug
information name the source. In C and C++, that name escapes `\` and
`"`; in Fortran, it is as written. A parse error in an excluded source
does not stop `saltfm`, so excluding a file works around a source that
SALT-FM cannot parse. A main program in such a source gets no setup
code, so a program run without MPI or `tau_exec` writes
`profile.-1.0.0`. SALT-FM warns when an excluded source seems to hold
the main program: `main(` in C and C++, a `program` statement in
Fortran.

`--tau_main_setup_parse_level=<level>`, or the environment variable
`SALT_MAIN_SETUP_PARSE_LEVEL`, selects the behavior. The option has
priority, the values ignore case, an empty value selects the default,
and an unknown value stops `saltfm` with an error:

| Level | Values | A main that the routine lists leave out | A source that the file lists exclude |
|---|---|---|---|
| `never` | `never`, `no`, `off`, `false`, `0` | No setup code, and no warning about the include list | Copied without a parse |
| `function` (default) | `function`, `procedure`, `default`, `medium`, `yes`, `on`, `true`, `1` | Setup code, no timer | Copied without a parse |
| `always` | `always`, `file`, `force`, `all` | Setup code, no timer | Parsed; its main gets the setup code, and a parse error makes `saltfm` fail |

A main program that the selective instrumentation file includes gets
its setup code and its timer at all levels.

At the `never` and `function` levels, SALT-FM still parses an excluded
Fortran source that seems to define a module or submodule (a line that
starts, after blanks and a statement label, with `module` or
`submodule`), so that Flang writes the module files that other sources
need; the output is still a copy, and at `never` SALT-FM warns about
the parse. If Flang fails, SALT-FM warns that it does not write the
module files, writes the copy, and exits with status 0.
`--tau_parse_excluded_modules=off`, or `SALT_PARSE_EXCLUDED_MODULES`
set to a value of the `never` level, skips this parse, with a warning
that sources that use the modules can fail; use it when Flang cannot
finish the parse of an excluded source, for example when it takes too
long. `on`, the default, takes the values of the `function` level. At
`always`, SALT-FM parses all sources and this option has no effect.

## Notes for package maintainers

When SALT-FM is configured inside a git checkout, the build system installs
client-side git hooks (in the working tree's `.git/config` via
`core.hooksPath`) to enforce `.VERSION` / tag agreement during local
development. This is convenience tooling for contributors and is
irrelevant when packaging release tarballs or building from a source
snapshot pulled by a package manager.

Distribution build recipes (Spack, RPM, Debian, conda-forge, Nix, etc.)
should pass:

``` shell
-DSALT_INSTALL_GIT_HOOKS=OFF
```

to the cmake invocation, regardless of whether the source tree happens
to be a git checkout. The flag is a no-op when there is no `.git/`
directory, but setting it explicitly documents intent and avoids
modifying the local clone's git configuration in environments where
that is unexpected.

[TAU Performance System]: http://www.tau.uoregon.edu/
[CMake]: https://cmake.org
[salt-dev]: https://github.com/ParaToolsInc/salt-dev
[posted to Docker Hub]: https://hub.docker.com/repository/docker/paratools/salt-dev/general
[salt-dev Docker Hub]: https://hub.docker.com/repository/docker/paratools/salt-dev/general
[CMakeLists.txt file]: https://github.com/ParaToolsInc/SALT-FM/blob/364be5ddd0043281669ace6697dfaf05fe724511/CMakeLists.txt#L353-L386

## Funding Acknowledgement

This material is based upon work supported by the U.S. Department of Energy, Office of Science, Office of SBIR and STTR Programs under Award Number DE-SC0022511.

This material is based upon work supported by the National Aeronautics and Space Administration under Contract Number 80NSSC24PB401.
