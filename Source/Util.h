/*
 *  Util.h
 *
 *  Copyright (c) 2017 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <iostream>
#include <vector>

const std::string &GetVersion();                    //!< Return the version of the QI library
const std::string &OutExt();                        //!< Return the extension stored in $QUIT_EXT
std::string StripExt(const std::string &filename);  //!< Remove the extension from a filename
std::string GetExt(const std::string &filename);    //!< Return the extension from a filename (including .)
std::string Basename(const std::string &path);      //!< Return only the filename part of a path

/*
 * Print a std::vector
 */
template<typename T>
inline std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
    os << "(";
    if (vec.size() > 0) {
        auto it = vec.begin();
        std::cout << *it;
        for (it++; it != vec.end(); it++) {
            std::cout << ", " << *it;
        }
    }
    os << ")";
    return os;
}

#endif // UTIL_H
