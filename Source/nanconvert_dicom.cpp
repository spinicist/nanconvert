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
args::Positional<std::string> output_arg(parser, "EXTENSION", "Output extension");

args::HelpFlag help(parser, "HELP", "Show this help menu", {'h', "help"});
args::Flag     verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
args::Flag double_precision(parser, "DOUBLE", "Write out double precision files", {'d', "double"});
args::ValueFlagList<std::string> rename_args(
    parser, "RENAME", "Rename using specified header fields (can be multiple).", {'r', "rename"});
args::ValueFlag<std::string>
    prefix(parser, "PREFIX", "Add a prefix to output filename.", {'p', "prefix"});

using Slice  = itk::Image<float, 2>;
using Volume = itk::Image<float, 3>;
using Series = itk::Image<float, 4>;

int main(int argc, char **argv) {
    ParseArgs(parser, argc, argv);
    const std::string input_dir      = CheckPos(input_arg);
    const std::string extension      = GetExt(CheckPos(output_arg));
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
                float          slice; // Position
                int            echo, b0, temporal, instance;
                std::string    casl, coil;
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
                meta                = dicomIO->GetMetaDataDictionary();
                const auto slice    = GetMetaDataFromString<float>(meta, "0020|1041", 0);
                const auto echo     = GetMetaDataFromString<int>(meta, "0018|0086", 0);
                const auto b0       = GetMetaDataFromString<int>(meta, "0018|9087", 0);
                const auto temporal = GetMetaDataFromString<int>(meta, "0020|0100", 0);
                const auto instance = GetMetaDataFromString<int>(meta, "0020|0013", 0);
                const auto casl     = GetMetaDataFromString<std::string>(meta, "0008|0008", "0");
                const auto coil     = GetMetaDataFromString<std::string>(meta, "0018|1250", "0");
                dicoms[i] = {reader->GetOutput(), slice, echo, b0, temporal, instance, casl, coil};
                reader->GetOutput()->DisconnectPipeline();
            }

            if (verbose)
                fmt::print("Sorting images...\n");
            std::set<float>       slocs;
            std::set<std::string> coils;
            std::sort(dicoms.begin(), dicoms.end(), [&](dicom_entry &a, dicom_entry &b) {
                slocs.insert(a.slice);
                slocs.insert(b.slice);
                return (a.slice < b.slice) ||
                       ((a.slice == b.slice) &&
                        ((a.echo < b.echo) ||
                         ((a.echo == b.echo) &&
                          ((a.b0 < b.b0) ||
                           ((a.b0 == b.b0) &&
                            ((a.casl < b.casl) ||
                             ((a.casl == b.casl) &&
                              ((a.temporal < b.temporal) ||
                               ((a.temporal == b.temporal) &&
                                ((a.coil < b.coil) ||
                                 ((a.coil == b.coil) && (a.instance < b.instance))))))))))));
            });
            const auto vols = fileNames.size() / slocs.size();

            if (verbose)
                fmt::print("I think there are {} slices and {} volumes...\n", slocs.size(), vols);

            itk::FixedArray<unsigned int, 4> layout;
            layout[0] = layout[1] = 1;
            layout[2]             = slocs.size();
            layout[3]             = vols;
            auto tiler            = itk::TileImageFilter<Slice, Series>::New();
            tiler->SetLayout(layout);

            size_t tilerIndex = 0;
            for (size_t v = 0; v < vols; v++) {
                size_t dicomIndex = v;
                for (size_t s = 0; s < slocs.size(); s++) {
                    auto slice = dicoms[dicomIndex].image;
                    tiler->SetInput(tilerIndex++, slice);
                    dicomIndex += vols;
                }
            }
            tiler->Update();
            const auto series_number = GetMetaDataFromString<int>(meta, "0020|0011", 0);
            const auto series_description =
                SanitiseString(Trim(GetMetaDataFromString<std::string>(meta, "0008|103e", "")));
            const auto data_type_int = GetMetaDataFromString<int>(meta, "0043|102f", 0);
            // Can't trust 0018|0050 for zero-filled images
            const auto slice_thickness =
                std::abs(*slocs.rbegin() - *slocs.begin()) / (slocs.size() - 1);
            const auto      TR    = GetMetaDataFromString<float>(meta, "0018|0080", 1.0f);
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
            const auto filename = fmt::format(
                "{:04d}_{}{}{}", series_number, series_description, data_type_string, extension);
            auto writer = itk::ImageFileWriter<Series>::New();
            writer->SetFileName(filename);
            writer->SetInput(image);
            if (verbose) {
                fmt::print("Writing: {}\n", filename);
            }
            writer->Update();
        }
    } catch (itk::ExceptionObject &ex) {
        fmt::print("{}", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}