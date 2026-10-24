[![CI](https://github.com/ParaToolsInc/SALT-FM/actions/workflows/CI.yaml/badge.svg)](https://github.com/ParaToolsInc/SALT-FM/actions/workflows/CI.yaml)

SALT-FM: An LLVM-based Source Analysis Toolkit for HPC
======================================================

SALT-FM adds performance instrumentation to C, C++, and Fortran
sources. It parses each source with Clang or Flang, and writes an
instrumented copy. The inserted code comes from a YAML configuration
file. SALT-FM includes configuration files for the
[TAU Performance System], perfstubs, ITT, NVTX, and ROCTX.
For the changes in each release, see [CHANGELOG.md](CHANGELOG.md).

SALT-FM supports LLVM/Clang 19 through 23. Each version has a
[salt-dev] container image:

| LLVM/Clang version | GitHub Container Registry           | Docker Hub                |
| ------------------ | ----------------------------------- | ------------------------- |
| 19                 | `ghcr.io/paratoolsinc/salt-dev:1.3` | `paratools/salt-dev:1.3`  |
| 20                 | `ghcr.io/paratoolsinc/salt-dev:1.4` | `paratools/salt-dev:1.4`  |
| 21                 | `ghcr.io/paratoolsinc/salt-dev:1.5` | `paratools/salt-dev:1.5`  |
| 22                 | `ghcr.io/paratoolsinc/salt-dev:1.6` | `paratools/salt-dev:1.6`  |
| 23                 | `ghcr.io/paratoolsinc/salt-dev:1.8` | `paratools/salt-dev:1.8`  |

## Getting Started

To install SALT-FM with [Spack], run:

``` shell
spack install saltfm
```

To build SALT-FM from source, you need [CMake] 3.23 or later and an
installation of LLVM and Clang. The installation must include the
`cmake-exports` and `clang-cmake-exports` components. For Fortran, it
must also include MLIR and Flang. A minimal LLVM or Clang installation
can need two patched CMake files (see step 1).

The [salt-dev] container image has `git`, `cmake`, `ninja`, and a
patched LLVM and Clang.

### 1. Clone this repo

``` shell
git clone --recursive https://github.com/ParaToolsInc/SALT-FM.git
```

The `--recursive` option also gets the patches for the LLVM and Clang
CMake files, in `cmake/llvm-patches`. You do not need them in the
[salt-dev] container. If the configuration in step 3 fails with a
minimal LLVM or Clang installation, apply these patches to it. The
patches are also at https://github.com/ParaToolsInc/salt-llvm-patches.

### 2. Launch the salt-dev Docker image (optional)

``` shell
$ docker pull paratools/salt-dev:latest
$ docker run -it --pull always --tmpfs=/dev/shm:rw,nosuid,nodev,exec \
    --privileged -v $(pwd):/home/salt/src paratools/salt-dev:latest
# cd /home/salt/src
# ccache --show-stats
```

We recommend this step. The image has patched LLVM and Clang CMake
files, `ccache`, and TAU configurations for GCC and Clang. Their
`TAU_MAKEFILE`s are in `/usr/local/x86_64/lib`.

### 3. Configure, build, and install SALT-FM

Do this step in the [salt-dev] container, or on a system with a
suitable LLVM and Clang installation.

``` shell
# Tell CMake to use the clang/clang++ compiler to build SALT-FM
export CC=clang
export CXX=clang++
# configure SALT-FM and generate build system
cmake -Wdev -Wdeprecated -S . -B build
cmake --build build --parallel # Add --verbose to debug something going wrong
```

To use Ninja instead of Make, add `-G Ninja` to the first `cmake`
command:

``` shell
cmake -Wdev -Wdeprecated -S . -B build -G Ninja
```

The build puts `saltfm`, the command that you run, in `build/bin`.
When CMake finds MLIR and Flang, the build also makes the Fortran
instrumentor.

To install SALT-FM, run:

``` shell
cmake --install build --prefix <dir>
```

This puts `saltfm` in `<dir>/bin`, and the configuration files in
`<dir>/share/saltfm/config_files`.

#### macOS (Homebrew LLVM 23)

Apple's clang does not include `flang`. Install LLVM, Flang, and Ninja
with Homebrew ([llvm](https://formulae.brew.sh/formula/llvm) and
[flang](https://formulae.brew.sh/formula/flang)):

``` shell
brew install llvm flang ninja
```

Use Homebrew `flang` 23.1.3_1 or later to build the Fortran
instrumentor. With an older `flang`, run `brew upgrade flang`.
Otherwise, SALT-FM builds for C and C++ only.

Set the compilers to the Homebrew clang, and configure and build:

``` shell
cmake -Wdev -Wdeprecated -G Ninja -S . -B build \
  -DCMAKE_C_COMPILER="$(brew --prefix llvm)/bin/clang" \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

You do not need to set `CMAKE_PREFIX_PATH` or `CMAKE_OSX_SYSROOT`.
CMake finds them. If configuration stops with `Cannot compile
<ryml_all.hpp>`, make sure that `CMAKE_CXX_COMPILER` is the Homebrew
`clang++`. Without TAU, CMake leaves out the tests that need TAU.

### 4. Run the tests (optional)

Run the tests to verify the build:

``` shell
cd build
ctest --output-on-failure
```

The tests write their output, such as instrumented sources, logs, and
profiles, to `test-output/` in the build directory. To remove it, run
`cmake --build build --target clean-test-artifacts`. The command
`ctest -R <test>` also runs the setup tests that `<test>` needs.

The tests cannot use paths that contain spaces. If the source or build
directory path contains a space, configuration fails. Use paths
without spaces, or configure with `-DBUILD_TESTING=OFF`.

The TAU tests need a TAU installation. CMake uses `-DTAU_ROOT=<dir>`,
then the `TAU_ROOT` environment variable, then `tau_exec` on `PATH`.
The [salt-dev] image has TAU in `/usr/local/x86_64/`. The TAU
installation must have these two configurations:

 `-pthread -cc=clang -c++=clang++ -bfd=download -unwind=download -dwarf=download -otf=download`

 and

 `-pthread -bfd=download -unwind=download -dwarf=download -otf=download`

### 5. Instrument a source

Run `saltfm` on a source:

``` shell
build/bin/saltfm tests/hello.c
```

`saltfm` writes `hello.inst.c` in the working directory. To name the
output file, use `--tau_output=<file>`. A run that fails, for example
on a full disk, leaves no partial output file. For all options, run
`saltfm --help`. An option has priority over its environment variable.

`saltfm` reads a source as Fortran if its extension is `.f`, `.for`,
`.ftn`, `.f77`, `.fpp`, `.f90`, `.f95`, `.f03`, `.f08`, or `.f18`, in
lowercase or uppercase (but not `.F77`). For another extension, give
`--lang=fortran`.

To instrument and compile in one step, use TAU's compiler wrappers with
`-optSaltInst`. For example, in a container of the latest [salt-dev]
image, set `TAU_MAKEFILE` to a TAU makefile from
`/usr/local/x86_64/lib`:

``` shell
export TAU_MAKEFILE=/usr/local/x86_64/lib/Makefile.tau-clang-pthread-pdt
tau_cc.sh -optSaltInst -optSaltParser="$PWD/build/bin/saltfm" tests/hello.c -o hello
./hello
```

The run writes `profile.0.0.0`. Use `tau_cxx.sh` for C++ and
`tau_f90.sh` for Fortran.

For Fortran, `saltfm` runs the Flang driver that SALT-FM was built
with, even if another `flang` comes first on `PATH`. To use a
different driver, pass `--flang_exe=<path>` or set
`SALT_FLANG_EXE=<path>`. If the configured driver is missing, SALT-FM
warns and uses `flang` or `flang-new` from `PATH`. The driver must have
the LLVM major version that SALT-FM was built with. Otherwise, SALT-FM
stops with an error.

By default, the Fortran parse reads and writes only `.saltmod` module
files, so that it cannot overwrite or misread the `.mod` files of a
build with another compiler, such as gfortran. If the build compiles
with the same LLVM Flang version that SALT-FM parses with, give
`--tau_build_uses_salt_flang`, or set `SALT_BUILD_USES_SALT_FLANG` to a
value other than empty or `0`. SALT-FM then reads and writes the `.mod`
files in the build's module directories, and reads those of its
libraries, such as MPI or HDF5. The libraries must be built with that
Flang version too. Otherwise, Flang refuses their module files, and
SALT-FM fails with a note that names the version.

### C and C++ sources with errors

When Clang reports errors in a C or C++ source, `saltfm` exits with a
non-zero status and writes no output file. Clang still builds a
partial parse tree. Two options make `saltfm` instrument that tree. Use
them, for example, to instrument a source without the include paths of
its build:

- `--tau_allow_parse_errors`, or `SALT_ALLOW_PARSE_ERRORS`, makes
  `saltfm` print a warning, write the output file, and exit with status
  0. Some timers can be wrong or missing. Clang uses `int` for an
  unknown type, so `void scale(RealD factor, ComplexD *z)` gets the
  timer name `void scale(int, int *)`. A function such as
  `void f(std::vector<RealD> &v)` gets no timer.
- `--tau_fabricate_unknown_types`, or `SALT_FABRICATE_UNKNOWN_TYPES`,
  adds declarations for unknown qualified names, such as `std::vector`.
  More functions then get timers, and the timer names use the types
  that the source spells. This option also allows the errors.

To turn on an option with its environment variable, set the variable
to any value except empty, `0`, `false`, `no`, or `off`, in any case.

These options are for C and C++ only. When Flang reports an error,
`saltfm` fails. By default, SALT-FM does not parse a source that the
file lists exclude, so its errors do not stop `saltfm` (see
[Selective instrumentation](#selective-instrumentation)).

### Configuration files

The YAML configuration file holds all the code that SALT-FM inserts.
`config_files/` has one file each for TAU, perfstubs, ITT, NVTX, and
ROCTX. By default, SALT-FM uses the TAU file. To select a different
file, use `--config_file=<file>`, or set `SALT_CONFIG_FILE=<file>`.

In an entry, `${full_timer_name}` is the timer name, ready for a string
literal in double quotes. In Fortran, the name continues over lines.
Put `&` after `${full_timer_name}`, and close the literal on the next
line, as `config_files/tau_config.yaml` does.

- `include`: lines at the top of an instrumented C or C++ source.
- `main_setup_insert` (optional): setup code at the start of `main`.
  `main_setup_insert_noargs` (optional) replaces it for a `main`
  without parameters. To learn when SALT-FM writes the setup code, see
  [Selective instrumentation](#selective-instrumentation).
- `main_insert`, or `main_insert_scope` with `--tau_use_cxx_api`: the
  timer of `main`, after the setup code.
- `function_begin_insert` and `function_end_insert`: the timer of each
  other function. With `--tau_use_cxx_api`, SALT-FM writes
  `function_begin_insert_scope` and no end code.
- Under `Fortran:`, `program_setup_insert` (optional) and
  `program_insert` do the same for the main program.
  `procedure_begin_insert` and `procedure_end_insert` do the same for
  other procedures. Fortran declarations come before statements. Thus,
  put the declarations for the main program's timer in
  `program_setup_insert`, as `tau_config.yaml` does.

When a source needs the configuration file, `saltfm` checks it. If the
file has a problem, `saltfm` stops with an error and writes no output.
The error names the file, and the bad key or the parse error position.

### Selective instrumentation

To read a selective instrumentation file, use
`saltfm --tau_select_file=<file>`. TAU's compiler wrappers pass the
file that `-optTauSelectFile=<file>` names. If `saltfm` cannot read
the file, it stops with an error and writes no output. SALT-FM uses
these rules in C, C++, and Fortran:

- A line that starts with `#` is a comment, also in a list.
- In routine lists, `#` matches zero or more characters. In file lists,
  `*` matches zero or more characters. In all lists, `?` matches one
  character. All other characters, such as `.`, `[`, or `$`, match only
  themselves. An entry must match the full name, not a part of it.
- To start a routine entry with the `#` wildcard, put the entry in
  quotes, for example `"#_internal"`. Routine lists remove the quotes.
  In file lists, quotes are part of the pattern, and SALT-FM warns
  about them. To match a file name that starts with `#`, use `?`.
- A C or C++ routine entry matches the signature in the timer name, for
  example `void compute(int)`. A Fortran routine entry matches the
  subprogram name and ignores case. File entries and C and C++ routine
  entries are case-sensitive.
- A file entry without `/` matches the basename of the source. A file
  entry with `/` must match the full path that SALT-FM gets. With TAU's
  compiler wrappers, this is the path on the compile line. SALT-FM
  does not make the path absolute or remove a leading `./`. In file
  entries, `*` also matches `/`:

  | Pattern | `src/a.c` | `./src/a.c` | `/abs/src/a.c` |
  |---|---|---|---|
  | `src/a.c` | yes | no | no |
  | `*/src/a.c` | no | yes | yes |
  | `*src/a.c` | yes | yes | yes (also `mysrc/a.c`) |
  | `a.c` (no `/`, basename) | yes | yes | yes |
- An exclude list has priority over an include list. If the include
  list (`BEGIN_INCLUDE_LIST`) is not empty, SALT-FM instruments only
  the routines that it matches.
- A C or C++ inline function gets a timer only with
  `--tau_instrument_inline`, or when it matches the include list.

Setup code makes sure that the measurement library starts correctly.
For TAU, it is `TAU_INIT` and `TAU_PROFILE_SET_NODE(0)`, or
`tau_profile_init` and `tau_profile_set_node(0)` in Fortran. If `main`
has no setup code and no timer, a threaded C or C++ program can crash.
Without setup code, a program that uses no MPI or threads and runs
without `tau_exec` writes `profile.-1.0.0`. The `-1` means that no node
number was set; node numbers start at 0.

When the routine lists leave out the main program, SALT-FM removes only
its timer. It still writes the setup code (`main_setup_insert`, or
`program_setup_insert` under `Fortran:`). An include list that is not
empty can leave out the main program while no exclude list entry
names it. Then, SALT-FM warns that the main program still gets setup
code.

By default, SALT-FM does not parse or instrument a source that the file
lists exclude. It copies the source to the output file, so the compiler
builds it without timers. Compiler messages, `__FILE__`, and debug
information still name the source. A parse error in an excluded
source does not stop `saltfm`. Thus, to skip a source that SALT-FM
cannot parse, exclude it.

The main program in an excluded source gets no setup code. SALT-FM
warns when an excluded source seems to contain the main program. In
Fortran, SALT-FM finds the main program only by its `program`
statement. To give the main program setup code, use
`--tau_excluded_main_setup=file`.

`--tau_excluded_main_setup=<none|routine|file>`, or the environment
variable `SALT_EXCLUDED_MAIN_SETUP`, selects which excluded main
programs get setup code:

| Value | A main that the routine lists leave out | A source that the file lists exclude |
|---|---|---|
| `none` | No setup code, and no warning about the include list | Copied without a parse |
| `routine` (default) | Setup code, no timer | Copied without a parse |
| `file` | Setup code, no timer | Parsed, with no timers; its main gets setup code, and a parse error makes `saltfm` fail |

A main program that the selective instrumentation file includes gets
its setup code and its timer at the `none`, `routine`, and `file`
levels.

At the `none` and `routine` levels, SALT-FM still parses an excluded
Fortran source that seems to define a module, or that has an `include`
line or `#include`. Flang then writes the module files that other
sources `use`; the output is still a copy. To change which excluded
sources SALT-FM parses, use `--tau_parse_excluded_modules` (see
`saltfm --help`).

## Notes for package maintainers

When you configure SALT-FM in a git checkout, CMake sets
`core.hooksPath` in the local git configuration of that clone. This
turns on the contributor git hooks in `.githooks/`. Packages do not
need them. In distribution build recipes, such as Spack, RPM, Debian,
conda-forge, or Nix, pass this option to `cmake`:

``` shell
-DSALT_INSTALL_GIT_HOOKS=OFF
```

Without a `.git/` directory, the option has no effect. Set it anyway,
so that the build never changes the git configuration of a clone.

[TAU Performance System]: http://www.tau.uoregon.edu/
[CMake]: https://cmake.org
[Spack]: https://spack.io
[salt-dev]: https://github.com/ParaToolsInc/salt-dev

## Funding Acknowledgement

This material is based upon work supported by the U.S. Department of Energy, Office of Science, Office of SBIR and STTR Programs under Award Number DE-SC0022511.

This material is based upon work supported by the National Aeronautics and Space Administration under Contract Number 80NSSC24PB401.
