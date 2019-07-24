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
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkImageSeriesReader.h"
#include "itkTileImageFilter.h"

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
args::ValueFlag<std::string> ext_flag(
    parser, "EXTENSION", "File extension/format to use (default .nii)", {'e', "ext"}, ".nii");
args::ValueFlag<std::string>
    prefix(parser, "PREFIX", "Add a prefix to output filename", {'p', "prefix"});

using Slice  = itk::Image<float, 2>;
using Volume = itk::Image<float, 3>;
using Series = itk::Image<float, 4>;

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

        for (auto const &seriesID : seriesUIDs) {
            std::vector<std::string> fileNames = name_generator->GetFileNames(seriesID);
            if (verbose) {
                fmt::print("Reading {}\nContains {} slices\n", seriesID, fileNames.size());
            }
            if (fileNames.size() == 0) {
                continue;
            }
            struct dicom_entry {
                Slice::Pointer image;
                float          sloc, te; // Position
                int            b0, temporal, instance;
                std::string    casl, coil, b_dir;
            };
            std::vector<dicom_entry> dicoms(fileNames.size());

            auto reader  = itk::ImageFileReader<Slice>::New();
            auto dicomIO = itk::GDCMImageIO::New();
            dicomIO->LoadPrivateTagsOn();
            reader->SetImageIO(dicomIO);
            itk::MetaDataDictionary meta;
            for (size_t i = 0; i < fileNames.size(); i++) {
                reader->SetFileName(fileNames[i]);
                reader->Update();
                meta             = dicomIO->GetMetaDataDictionary();
                auto const sloc  = GetMetaDataFromString<float>(meta, "0020|1041", 0);
                auto const te    = GetMetaDataFromString<float>(meta, "0018|0081", 0);
                auto const b0    = GetMetaDataFromString<int>(meta, "0043|1039", 0);
                auto const b_dir = GetMetaDataFromString<std::string>(meta, "0019|10bb", "0") +
                                   "," +
                                   GetMetaDataFromString<std::string>(meta, "0019|10bc", "0") +
                                   "," + GetMetaDataFromString<std::string>(meta, "0019|10bd", "0");
                auto const temporal = GetMetaDataFromString<int>(meta, "0020|0100", 0);
                auto const instance = GetMetaDataFromString<int>(meta, "0020|0013", 0);
                auto const casl     = GetMetaDataFromString<std::string>(meta, "0008|0008", "0");
                auto const coil     = GetMetaDataFromString<std::string>(meta, "0018|1250", "0");
                dicoms[i]           = {
                    reader->GetOutput(), sloc, te, b0, temporal, instance, casl, coil, b_dir};
                reader->GetOutput()->DisconnectPipeline();
            }

            if (verbose)
                fmt::print("Sorting images...\n");
            std::sort(dicoms.begin(), dicoms.end(), [&](dicom_entry &a, dicom_entry &b) {
                return (a.sloc < b.sloc) ||
                       ((a.sloc == b.sloc) &&
                        ((a.te < b.te) ||
                         ((a.te == b.te) &&
                          ((a.b0 < b.b0) ||
                           ((a.b0 == b.b0) &&
                            ((a.b_dir == b.b_dir) &&
                             ((a.b_dir < b.b_dir) ||
                              ((a.casl < b.casl) ||
                               ((a.casl == b.casl) &&
                                ((a.temporal < b.temporal) ||
                                 ((a.temporal == b.temporal) &&
                                  ((a.coil < b.coil) ||
                                   ((a.coil == b.coil) && (a.instance < b.instance))))))))))))));
            });

            if (verbose)
                fmt::print("Extracting unique information...\n");
            std::set<float> slocs, tes, b0s;
            for (auto const &d : dicoms) {
                slocs.insert(d.sloc);
                tes.insert(d.te);
                b0s.insert(d.b0);
            }
            auto const vols = fileNames.size() / slocs.size();
            if (verbose)
                fmt::print("I think there are {} slices and {} volumes...\n", slocs.size(), vols);

            itk::FixedArray<unsigned int, 4> layout;
            layout[0] = layout[1] = 1;
            layout[2]             = slocs.size();
            layout[3]             = vols;
            auto tiler            = itk::TileImageFilter<Slice, Series>::New();
            tiler->SetLayout(layout);

            std::vector<std::string> b_dirs;
            size_t                   tilerIndex = 0;
            for (size_t v = 0; v < vols; v++) {
                size_t dicomIndex = v;
                b_dirs.push_back(dicoms[dicomIndex].b_dir);
                for (size_t s = 0; s < slocs.size(); s++) {
                    auto slice = dicoms[dicomIndex].image;
                    tiler->SetInput(tilerIndex++, slice);
                    dicomIndex += vols;
                }
            }
            tiler->Update();
            auto const series_number = GetMetaDataFromString<int>(meta, "0020|0011", 0);
            auto const series_description =
                SanitiseString(Trim(GetMetaDataFromString<std::string>(meta, "0008|103e", "")));
            auto const data_type_int = GetMetaDataFromString<int>(meta, "0043|102f", 0);
            // Can't trust 0018|0050 for zero-filled images
            auto const slice_thickness =
                std::abs(*slocs.rbegin() - *slocs.begin()) / (slocs.size() - 1);
            auto const      TR    = GetMetaDataFromString<float>(meta, "0018|0080", 1.0f);
            Series::Pointer image = tiler->GetOutput();
            image->DisconnectPipeline();
            auto spacing = image->GetSpacing();
            spacing[2]   = slice_thickness;
            spacing[3]   = TR;
            image->SetSpacing(spacing);
            std::string data_type_string;
            switch (data_type_int) {
            case 0:
                data_type_string = "";
                break;
            case 1:
                data_type_string = "_phase";
                break;
            case 2:
                data_type_string = "_real";
                break;
            case 3:
                data_type_string = "_imag";
                break;
            default:
                FAIL("Unknown data-type: " << data_type_int);
            }
            auto const filename = fmt::format(
                "{:04d}_{}{}{}", series_number, series_description, data_type_string, extension);
            auto writer = itk::ImageFileWriter<Series>::New();
            writer->SetFileName(filename);
            writer->SetInput(image);
            if (verbose) {
                fmt::print("Writing: {}\n", filename);
            }
            writer->Update();
            auto const infoname = fmt::format(
                "{:04d}_{}{}{}", series_number, series_description, data_type_string, ".txt");

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
                        info << b_dir << "\n";
                    }
                }
            }
        }
    } catch (itk::ExceptionObject &ex) {
        fmt::print("{}", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}