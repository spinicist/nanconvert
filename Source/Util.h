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

#include "itkMetaDataObject.h"
#include <iostream>
#include <string>
#include <vector>

const std::string &OutExt(); //!< Return the extension stored in $QUIT_EXT
std::string        StripExt(const std::string &filename); //!< Remove the extension from a filename
std::string
            GetExt(const std::string &filename); //!< Return the extension from a filename (including .)
std::string Basename(const std::string &path);    //!< Return only the filename part of a path
std::string Trim(const std::string &s);           //!< Remove leading and trailing whitespace
std::string SanitiseString(const std::string &s); //!< Remove undesirable characters from a filename

/*
 * Print a std::vector
 */
template <typename T> inline std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec) {
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

/*
 * Helper function to recover a value from an ITK meta-data dictionary
 */
template <typename T> T GetMetaData(const itk::MetaDataDictionary &dict, const std::string &name) {
    T value;
    if (!ExposeMetaData(dict, name, value)) {
        throw std::runtime_error("Could not read parameter: " + name);
    }
    return value;
}

/*
 * Version with a default for when a value is required
 */
template <typename T>
T GetMetaDataFromString(const itk::MetaDataDictionary &dict,
                        const std::string &            name,
                        const T &                      def) {
    std::string string_value;
    if (!ExposeMetaData(dict, name, string_value)) {
        return def;
    } else {
        T                 value;
        std::stringstream stream_value(string_value);
        stream_value >> value;
        return value;
    }
}

template <>
std::string GetMetaDataFromString(const itk::MetaDataDictionary &dict,
                                  const std::string &            name,
                                  const std::string &            def);

#endif // UTIL_H
