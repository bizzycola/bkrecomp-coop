# Banjo-Kazooie Co-Op Mod

This is the recomp mod code for the Banjo-Kazooie Recomp co-op mod.
It's based both on the BK mod template, and also the [LT-Schmiddy MM Recomp Mod Template](https://github.com/LT-Schmiddy/SchmiddysMMRecompModTemplate) from which I borrowed all the python build tools.

The following build sections are like-for-like from the MM mod template.

## Tools

This template has somewhat different requirements from the default mod template. In order to run it, you'll need the following:

* `make`
* `python` (or `python3` on POSIX systems).
* `cmake`
* `ninja`

On Linux and MacOS, you'll need to also ensure that you have the `zip` utility installed.

All of these can (and should) be installed via using [chocolatey](https://chocolatey.org/) on Windows, Homebrew on MacOS, or your distro's package manager on Linux.

**You do NOT need the `RecompModTool` tool or any additional compilers installed. The build scripting will download all of these for you.**

The default configuration (as well as the example configurations) will download the N64RecompEssentials package from [here](https://github.com/LT-Schmiddy/n64recomp-clang)
for your platform, which contains the `RecompModTool` and the MIPS-only LLVM 21 `clang` and `ld.lld` compiler and linker pair for your system.

It will also download Zig 0.14 for project configurations that require it. This is done because Zig's packaging is inconsistent across ecosystems, but the compiler
itself is thankfully small.

## Building

TL;DR: Run `git submodule update --init --recursive` to make sure you've clones all submodules. Then, run `./modbuild.py` to prepare a debug build.
Run `./modbuild.py thunderstore` to create a Thunderstore package.

Due to issues where certain complex tasks become difficult to do in a cross-platform way using Make (and trying invoke Python functions from Make resulted in some
of the worse spaghetti code I've ever written), I've decided to not have Make be the entrypoint for the build process. Instead, I've turned to a lightweight,
Make-inspired Python library called `pyinvoke` to help me create an all-inclusive build script: `modbuild.py`. This script is capable of building the entire project
from scratch, or simply running parts of the build process depending on the subcommands and their arguments.

You can run `./modbuild.py -h` or `./modbuild.py [subcommand] -h` for usage information on the script and various subcommands.

All commands are defined in `tasks.py`, in accordance to the `pyinvoke` library. See that documentation at
[https://docs.pyinvoke.org/en/stable/](https://docs.pyinvoke.org/en/stable/) for info on how to define additional commands.

(Don't worry, there are no Python packages you need to install. All of the required Python code has been incorperated into this template).