/*
 *  nanconvert_dicom.cpp
 *
 *  Copyright (c) 2017 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <algorithm>
#include <set>

#include "fmt/format.h"
#include "fmt/ostream.h"
#include "itkComposeImageFilter.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkImageSeriesReader.h"
#include "itkJoinSeriesImageFilter.h"

#include "Args.h"
#include "IO.h"
#include "Util.h"

/*
 * Declare args here so things like verbose can be global
 */
args::ArgumentParser
    parser("Convert DICOM format to whatever you want\nhttp://github.com/spinicist/nanconvert");

args::Positional<std::string> input_arg(parser, "INPUT", "Input directory");

args::HelpFlag help(parser, "HELP", "Show this help menu", {'h', "help"});
args::Flag     verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
args::Flag double_precision(parser, "DOUBLE", "Write out double precision files", {'d', "double"});
args::Flag param_file(parser,
                      "PARAMS",
                      "Write out text file with parameters (TE/TR etc.)",
                      {'p', "params"});
args::ValueFlagList<std::string> rename_args(
    parser, "RENAME", "Rename using specified header fields (can be multiple)", {'r', "rename"});
args::ValueFlag<std::string>
    out_name(parser, "OUTNAME", "Use specified output name (overrides RENAME)", {'o', "out"});
args::ValueFlag<std::string> ext_flag(
    parser, "EXTENSION", "File extension/format to use (default .nii)", {'e', "ext"}, ".nii");
args::ValueFlag<std::string>
    prefix(parser, "PREFIX", "Add a prefix to output filename", {'p', "prefix"});

using Slice   = itk::Image<float, 2>;
using Volume  = itk::Image<float, 3>;
using Series  = itk::Image<float, 4>;
using XSeries = itk::Image<std::complex<float>, 4>;

template <typename T>
void write_image(typename T::Pointer image,
                 std::string const & filename,
                 float const         sl_thick,
                 float const         TR) {
    auto spacing = image->GetSpacing();
    spacing[2]   = sl_thick;
    spacing[3]   = TR;
    image->SetSpacing(spacing);

    auto writer = itk::ImageFileWriter<T>::New();
    writer->SetFileName(filename);
    writer->SetInput(image);
    if (verbose) {
        fmt::print("Writing: {}\n", filename);
    }
    writer->Update();
}

int main(int argc, char **argv) {
    ParseArgs(parser, argc, argv);
    const std::string input_dir      = CheckPos(input_arg);
    const std::string extension      = GetExt(ext_flag.Get());
    auto              name_generator = itk::GDCMSeriesFileNames::New();
    name_generator->SetUseSeriesDetails(true);
    name_generator->SetLoadSequences(true);
    name_generator->SetLoadPrivateTags(true);
    name_generator->AddSeriesRestriction("0043|102f"); // Magnitude/real/imaginary
    name_generator->SetGlobalWarningDisplay(false);
    name_generator->SetDirectory(input_dir);

    try {
        auto seriesUIDs = name_generator->GetSeriesUIDs();
        if (seriesUIDs.size() == 0) {
            fmt::print("No DICOMs in: {}", input_dir);
            return EXIT_SUCCESS;
        } else {
            if (verbose) {
                fmt::print(
                    "Directory: {}\nContains {} DICOM Series\n", input_dir, seriesUIDs.size());
            }
        }

        struct Vec3 {
            float x, y, z;
        };
        std::vector<Series::Pointer> all_series;
        std::vector<int>             all_types;
        itk::MetaDataDictionary      meta;            // Need this after the loop for writing
        std::set<float>              slocs, tes, b0s; // Need these after loop
        std::vector<Vec3>            b_dirs;
        for (auto const &seriesID : seriesUIDs) {
            std::vector<std::string> allNames = name_generator->GetFileNames(seriesID);
            if (verbose) {
                fmt::print("Reading {}\nContains {} slices\n", seriesID, allNames.size());
            }
            if (allNames.size() == 0) {
                continue;
            }
            struct dicom_entry {
                std::string path;
                float       sloc, te; // Position
                int         b0, temporal, instance;
                Vec3        b_dir;
                std::string casl, coil;
            };
            std::vector<dicom_entry> dicoms(allNames.size());

            auto dicomIO = itk::GDCMImageIO::New();
            dicomIO->LoadPrivateTagsOn();
            for (size_t i = 0; i < allNames.size(); i++) {
                dicomIO->SetFileName(allNames[i]);
                dicomIO->ReadImageInformation();
                meta               = dicomIO->GetMetaDataDictionary();
                dicoms[i].path     = allNames[i];
                dicoms[i].sloc     = GetMetaDataFromString<float>(meta, "0020|1041", 0);
                dicoms[i].te       = GetMetaDataFromString<float>(meta, "0018|0081", 0);
                dicoms[i].b0       = GetMetaDataFromString<int>(meta, "0043|1039", 0);
                dicoms[i].b_dir.x  = GetMetaDataFromString<float>(meta, "0019|10bb", 0);
                dicoms[i].b_dir.y  = GetMetaDataFromString<float>(meta, "0019|10bc", 0);
                dicoms[i].b_dir.z  = GetMetaDataFromString<float>(meta, "0019|10bd", 0);
                dicoms[i].temporal = GetMetaDataFromString<int>(meta, "0020|0100", 0);
                dicoms[i].instance = GetMetaDataFromString<int>(meta, "0020|0013", 0);
                dicoms[i].casl     = GetMetaDataFromString<std::string>(meta, "0008|0008", "0");
                dicoms[i].coil     = GetMetaDataFromString<std::string>(meta, "0018|1250", "0");
            }

            if (verbose)
                fmt::print("Sorting images...\n");
            std::sort(dicoms.begin(), dicoms.end(), [&](dicom_entry &a, dicom_entry &b) {
                return (a.sloc < b.sloc) ||
                       ((a.sloc == b.sloc) &&
                        ((a.te < b.te) ||
                         ((a.te == b.te) && ((a.instance < b.instance) ||
                                             ((a.instance == b.instance) &&
                                              ((a.casl < b.casl) || ((a.casl == b.casl) &&
                                                                     ((a.temporal < b.temporal) ||
                                                                      ((a.temporal == b.temporal) &&
                                                                       ((a.coil < b.coil)))))))))));
            });

            if (verbose)
                fmt::print("Extracting unique information...\n");

            // Some weird GE series can have differing numbers of slices
            slocs.clear();
            tes.clear();
            b0s.clear();
            for (auto const &d : dicoms) {
                slocs.insert(d.sloc);
                tes.insert(d.te);
                b0s.insert(d.b0);
            }

            auto const vols = allNames.size() / slocs.size();
            if (verbose)
                fmt::print("I think there are {} slices and {} volumes...\n", slocs.size(), vols);

            if (b_dirs.size() == 0) {
                for (size_t v = 0; v < vols; v++) {
                    b_dirs.push_back(dicoms[v].b_dir);
                }
            }

            auto joiner = itk::JoinSeriesImageFilter<Volume, Series>::New();
            for (size_t v = 0; v < vols; v++) {
                size_t                   dicomIndex = v;
                std::vector<std::string> volNames(slocs.size());
                for (size_t s = 0; s < slocs.size(); s++) {
                    volNames[s] = dicoms[dicomIndex].path;
                    dicomIndex += vols;
                }
                auto reader = itk::ImageSeriesReader<Volume>::New();
                reader->SetImageIO(dicomIO);
                reader->SetFileNames(volNames);
                reader->ForceOrthogonalDirectionOff();
                reader->Update();
                joiner->SetInput(v, reader->GetOutput());
            }
            joiner->Update();
            all_series.push_back(joiner->GetOutput());
            all_types.push_back(GetMetaDataFromString<int>(meta, "0043|102f", 0));
        }

        auto const series_number = GetMetaDataFromString<int>(meta, "0020|0011", 0);
        auto const series_description =
            SanitiseString(Trim(GetMetaDataFromString<std::string>(meta, "0008|103e", "")));

        auto const TR = GetMetaDataFromString<float>(meta, "0018|0080", 1.0f);
        // Can't trust 0018|0050 for zero-filled images
        float const slice_thickness =
            slocs.size() > 1 ? std::abs(*slocs.rbegin() - *slocs.begin()) / (slocs.size() - 1) :
                               1.0;

        auto const filename =
            out_name ? out_name.Get() : fmt::format("{:04d}_{}", series_number, series_description);
        std::set<int> processed_indices;
        for (size_t i = 0; i < all_series.size(); i++) {
            std::string const tag = all_series.size() > 1 ? fmt::format("{}", i + 1) : "";
            if (all_types[i] == 2) { // Real series
                if ((i + 1 < all_series.size()) && (all_types[i + 1] == 3) &&
                    (all_series.at(i)->GetOrigin().GetVnlVector().is_equal(
                        all_series.at(i + 1)->GetOrigin().GetVnlVector(), 2.e-6))) {

                    // We have matching real/imaginary series, convert to complex
                    auto to_complex = itk::ComposeImageFilter<Series, XSeries>::New();
                    to_complex->SetInput(0, all_series.at(i));
                    to_complex->SetInput(1, all_series.at(i + 1));
                    to_complex->Update();
                    XSeries::Pointer x = to_complex->GetOutput();
                    x->DisconnectPipeline();
                    write_image<XSeries>(x, filename + extension, slice_thickness, TR);
                } else {
                    Series::Pointer m = all_series.at(i);
                    write_image<Series>(m, filename + tag + extension, slice_thickness, TR);
                }
                processed_indices.insert(i);
                processed_indices.insert(i + 1);
            } else {
                // Could be anything, write it if we haven't done so already
                if (processed_indices.find(i) == processed_indices.end()) {

                    Series::Pointer m = all_series.at(i);
                    write_image<Series>(m, filename + tag + extension, slice_thickness, TR);
                    processed_indices.insert(i);
                }
            }
        }

        auto const infoname = fmt::format("{:04d}_{}{}", series_number, series_description, ".txt");

        if (param_file) {
            std::ofstream info(infoname);
            info << "TR: " << TR << "\n";
            info << "TE: ";
            for (auto const &te : tes) {
                info << te << "\t";
            }

            info << "\n";
            if (b0s.size() > 1) {
                info << "b0: ";
                for (auto const &b0 : b0s) {
                    info << b0 << "\t";
                }
                info << "\n";
                info << "b_dirs:\n";
                for (auto const &b_dir : b_dirs) {
                    info << fmt::format("{},{},{}\n", b_dir.x, b_dir.y, b_dir.z);
                }
            }
        }
    } catch (itk::ExceptionObject &ex) {
        fmt::print("{}", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}