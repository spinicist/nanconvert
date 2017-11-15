/*
 *  IO.h
 *
 *  Copyright (c) 2017 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  This file contains template definitions to read and write ITK images.
 *  These are declared extern, and the actual instantiations declared in IO.cpp
 *  This is to allow some measure of parallel compilation, and to avoid
 *  recompilation every time the front-end code changes.
 *
 */

#ifndef IO_H
#define IO_H

#include <string>

template<typename TImg>
extern auto ReadImage(const std::string &path) -> typename TImg::Pointer;

template<typename TImg>
extern void WriteImage(const TImg *ptr, const std::string &path);

#endif