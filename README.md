# Not Another Neuroimaging Converter - nanconvert #

[![Build Status](https://travis-ci.org/spinicist/nanconvert.svg?branch=master)](https://travis-ci.org/spinicist/nanconvert)

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

MacOS and Linux binaries are available from 
http://github.com/spinicist/nanconvert/releases in .tar.gz format. Unpack the
archives using `tar -xzf nanconvert-platform.tar.gz` and move the contained
files to somewhere on $PATH.

# Usage #

The executable `nanconvert_bruker` can be called to process a single Bruker
2dseq file. Specify the path to the 2dseq file itself, not the folder it resides
in. Type `nanconvert_bruker -h` to see a full list of options.

In addition, the script `nanbruker` is provided which can convert multiple
ParaVision datasets. Type `nanbruker` to see a full list of options. If
`nanbruker -q` is called, then individual 2dseq files will be converted using
jobs submitted to a Sun Grid Engine queue.
