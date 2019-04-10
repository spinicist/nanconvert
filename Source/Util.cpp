/*
 *  Util.cpp
 *
 *  Copyright (c) 2017 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "Util.h"
#include "Macro.h"

std::string StripExt(const std::string &filename) {
    std::size_t dot = filename.find_last_of(".");
    if (dot != std::string::npos) {
        /* Deal with .nii.gz files */
        if (filename.substr(dot) == ".gz") {
            dot = filename.find_last_of(".", dot - 1);
        }
        return filename.substr(0, dot);
    } else {
        return filename;
    }
}

std::string GetExt(const std::string &filename) {
    std::size_t dot = filename.find_last_of(".");
    if (dot != std::string::npos) {
        /* Deal with .nii.gz files */
        if (filename.substr(dot) == ".gz") {
            dot = filename.find_last_of(".", dot - 1);
        }
        return filename.substr(dot);
    } else {
        FAIL("No extension found in string: " << filename);
    }
}

std::string Basename(const std::string &path) {
    std::size_t slash = path.find_last_of("/");
    if (slash != std::string::npos) {
        return StripExt(path.substr(slash));
    } else {
        return StripExt(path);
    }
}

std::string Trim(const std::string &s) {
    size_t            f = s.find_first_not_of(' ');
    size_t            l = s.find_last_not_of(' ');
    const std::string o = s.substr(f, (l - f + 1));
    return o;
}

/*
 * Helper function to sanitise meta-data to be suitable for a filename
 */
std::string SanitiseString(const std::string &s) {
    const std::string forbidden = "\\/:?\"<>|*+-=.";
    std::string       temp(s.size(), ' ');
    // First, transform all forbidden characters into one forbidden character
    std::transform(s.begin(), s.end(), temp.begin(), [&forbidden](char c) {
        return forbidden.find(c) != std::string::npos ? '?' : c;
    });
    // Now remove all occurrences of that string
    temp.erase(std::remove(temp.begin(), temp.end(), '?'), temp.end());
    // Now transform all spaces to underscores
    std::string out(temp.size(), ' ');
    std::transform(
        temp.begin(), temp.end(), out.begin(), [](char c) { return c == ' ' ? '_' : c; });
    return out;
}

template <>
std::string GetMetaDataFromString(const itk::MetaDataDictionary &dict,
                                  const std::string &            name,
                                  const std::string &            def) {
    std::string string_value;
    if (!ExposeMetaData(dict, name, string_value)) {
        return def;
    } else {
        return string_value;
    }
}