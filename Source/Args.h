/*
 *  Args.h
 *
 *  Copyright (c) 2017 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef ARGS_H
#define ARGS_H

#include "args.hxx"
#include "Macro.h"

void ParseArgs(args::ArgumentParser &parser, int argc, char **argv) {
    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help const &) {
        std::cout << parser;
        exit(EXIT_SUCCESS);
    } catch (args::ParseError const &e) {
        FAIL(e.what() << std::endl << parser);
    } catch (args::ValidationError const &e) {
        FAIL(e.what() << std::endl << parser);
    }
}

template<typename T>
T CheckPos(args::Positional<T> &a) {
    if (a) {
        return a.Get();
    } else {
        FAIL(a.Name() << " was not specified. Use --help to see usage.");
    }
}

template<typename T>
std::vector<T> CheckList(args::PositionalList<T> &a) {
    if (a) {
        return a.Get();
    } else {
        FAIL("No values of " << a.Name() << " specified. Use --help to see usage.");
    }
}

#endif // ARGS_H