/*
 *  Macro.h
 *
 *  Copyright (c) 2017 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */
 
 /*
 * The following macros are adapted from ITK for use in QUIT files which don't include any of ITK
 */

#ifndef MACRO_H
#define MACRO_H

#include <sstream>

#if defined( _WIN32 ) && !defined( __MINGW32__ )
    #define LOCATION __FUNCSIG__
#elif defined( __GNUC__ )
    #define LOCATION __PRETTY_FUNCTION__   
#else
    #define LOCATION __FUNCTION__
#endif

#define EXCEPTION( x )                               \
{                                                       \
    std::ostringstream message;                         \
    message << LOCATION << std::endl                 \
            << __FILE__ << ":" << __LINE__ << std::endl \
            << x << std::endl;                          \
    throw(std::runtime_error(message.str()));                 \
}

#define FAIL( x )             \
{                                \
    std::cerr << x << std::endl; \
    exit(EXIT_FAILURE);          \
}

#endif // MACRO_H