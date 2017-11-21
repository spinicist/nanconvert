# Not Another Neuroimaging Converter - nanconvert #

# Brief Description #

A set of command-line tools for converting between neuroimaging data formats,
using the ITK library for Input/Output, with useful features like writing out
diffusion b-value files. Currently supports Bruker 2dseq and limited DICOM (GE
flavor).

If you find the tools useful the author would love to hear from you.

# Contact #

Credit / Blame / Contact - Tobias Wood - tobias.wood@kcl.ac.uk

# License #

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Installation #

## Dependencies & Requirements ##

1. A C++11 compliant compiler (GCC 4.8.0+, Clang 3.1+)
2. CMake version 2.8.1 or higher (http://www.cmake.org)
3. ITK version 4.13.0 (http://www.itk.org)

Optional - I recommend http://www.ninja-build.org instead of plain `make`

## Compilation ##

1. Install or compile ITK version 4.13 (as of 2017/11/21, this means compiling
   the development version.
2. Type the following commands: 
   `git clone --recursive https://github.com/spinicist/nanconvert.git`
   `cd nanconvert`
   `git submodule init; git submodule update`
3. Create a build directory and `cd` to it.
4. Type `ccmake path_to_nanconvert_folder`
5. Follow the on screen instructions. In particular make sure the path to ITK
   is specified correctly, then press `c` and `g` to configure and generate the
   build files, then `q` to quit.
6. Type `make -j`, and optionally `make install` (or `ninja install`)

# Usage #

The executable `nanconvert_bruker` can be called to process a single Bruker
2dseq file. Specify the path to the 2dseq file itself, not the folder it resides
in. Type `nanconvert_bruker -h` to see a full list of options.

In addition, the script `nanbruker` is provided which can convert multiple
ParaVision datasets. Type `nanbruker` to see a full list of options. If
`nanbruker -q` is called, then individual 2dseq files will be converted using
jobs submitted to a Sun Grid Engine queue.
