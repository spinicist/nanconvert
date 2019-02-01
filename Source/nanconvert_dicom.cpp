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
#include <fmt/format.h>

#include "itkImage.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include "itkImageSeriesReader.h"
#include "itkImageFileWriter.h"
#include "itkRegionOfInterestImageFilter.h"
#include "itkTileImageFilter.h"

#include "Args.h"
#include "Util.h"
#include "IO.h"

/*
 * Declare args here so things like verbose can be global
 */
args::ArgumentParser parser(std::string("Convert DICOM format to whatever you want\n") + GetVersion() + "\nhttp://github.com/spinicist/nanconvert");

args::Positional<std::string> input_arg(parser, "INPUT", "Input directory");
args::Positional<std::string> output_arg(parser, "EXTENSION", "Output extension");

args::HelpFlag help(parser, "HELP", "Show this help menu", {'h', "help"});
args::Flag verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
args::Flag double_precision(parser, "DOUBLE", "Write out double precision files", {'d', "double"});
args::ValueFlagList<std::string> rename_args(parser, "RENAME", "Rename using specified header fields (can be multiple).", {'r', "rename"});
args::ValueFlag<std::string> prefix(parser, "PREFIX", "Add a prefix to output filename.", {'p', "prefix"});

using Volume = itk::Image<float, 3>;
using Series = itk::Image<float, 4>;

int main(int argc, char **argv)
{
    ParseArgs(parser, argc, argv);
    const std::string input_dir = CheckPos(input_arg);
    const std::string extension = GetExt(CheckPos(output_arg));
    auto name_generator = itk::GDCMSeriesFileNames::New();
    name_generator->SetUseSeriesDetails(true);
    name_generator->SetLoadSequences(true);
    name_generator->SetLoadPrivateTags(true);
    // name_generator->AddSeriesRestriction("0018|1314"); // Flip Angle
    // name_generator->AddSeriesRestriction("0018|0080"); // Repetition time
    name_generator->AddSeriesRestriction("0018|0086"); // Echo numbers
    name_generator->AddSeriesRestriction("0018|1060"); // Trigger time
    // name_generator->AddSeriesRestriction("0008|0032"); // Acquisition time
    // name_generator->AddSeriesRestriction("0008|0031"); // Series time
    // name_generator->AddSeriesRestriction("0008|0033"); // Content time
    name_generator->AddSeriesRestriction("0018|9087"); // Diffusion b0
    name_generator->AddSeriesRestriction("0008|0008"); // Image Type (CASL)
    name_generator->AddSeriesRestriction("0043|1030"); // "Vas collapse flag"
    name_generator->AddSeriesRestriction("0027|1035"); // Plane Type / Orientation
    name_generator->AddSeriesRestriction("0020|0100"); // Temporal Position Identifier
    name_generator->AddSeriesRestriction("0018|1250"); // Coil (for ASSET)
    name_generator->AddSeriesRestriction("0043|102f"); // Magnitude/real/imaginary
    name_generator->SetGlobalWarningDisplay(false);
    name_generator->SetDirectory(input_dir);

    try
    {
        std::vector<std::string> seriesUID = name_generator->GetSeriesUIDs();
        std::vector<std::string>::const_iterator series_it = seriesUID.begin();

        if (series_it != seriesUID.end())
        {
            if (verbose)
            {
                fmt::print(stderr, "The directory: {}\n Contains the following DICOM Series:\n", input_dir);
                while (series_it != seriesUID.end())
                {
                    fmt::print(stderr, "{}\n", *series_it);
                    ++series_it;
                }
            }
            else
            {
                fmt::print(stderr, "No DICOMs in: {}\n", input_dir);
                return EXIT_SUCCESS;
            }
        }

        itk::FixedArray<unsigned int, 4> layout;
        layout[0] = layout[1] = layout[2] = 1;
        layout[3] = seriesUID.size();
        auto tiler = itk::TileImageFilter<Volume, Series>::New();
        tiler->SetLayout(layout);

        itk::MetaDataDictionary meta;
        std::sort(seriesUID.begin(), seriesUID.end());
        series_it = seriesUID.begin();

        struct volume_entry
        {
            Volume::Pointer image;
            int triggertime;
            int imagetype;
        };
        std::vector<volume_entry> volumes;
        while (series_it != seriesUID.end())
        {
            std::string seriesIdentifier = *series_it;
            ++series_it;
            if (verbose)
            {
                fmt::print(stderr, "Reading: {}\n", seriesIdentifier);
            }
            std::vector<std::string> fileNames = name_generator->GetFileNames(seriesIdentifier);

            auto reader = itk::ImageSeriesReader<Volume>::New();
            auto dicomIO = itk::GDCMImageIO::New();
            dicomIO->LoadPrivateTagsOn();
            reader->SetImageIO(dicomIO);
            reader->SetFileNames(fileNames);
            reader->Update();
            meta = dicomIO->GetMetaDataDictionary();

            const auto type = std::stoi(GetMetaData<std::string>(meta, "0043|102f"));
            int triggertime = 0;
            try
            {
                if (meta.HasKey("0018|1060"))
                {
                    triggertime = std::stoi(GetMetaData<std::string>(meta, "0018|1060"));
                }
            }
            catch (std::exception &e)
            {
                if (verbose)
                {
                    fmt::print(stderr, "Invalid trigger time, set to 0\n");
                }
            }
            volumes.push_back({reader->GetOutput(), triggertime, type});
            reader->GetOutput()->DisconnectPipeline();
        }
        std::sort(volumes.begin(),
                  volumes.end(),
                  [](volume_entry &a, volume_entry &b) { return (a.imagetype > b.imagetype) || (a.triggertime > b.triggertime); });
        for (unsigned int volume = 0; volume < volumes.size(); volume++)
        {
            tiler->SetInput(volume, volumes[volume].image);
        }
        tiler->Update();
        const auto series_number = std::stoi(GetMetaData<std::string>(meta, "0020|0011"));
        const auto series_description = SanitiseString(Trim(GetMetaData<std::string>(meta, "0008|103e")));
        const auto data_type_int = std::stoi(GetMetaData<std::string>(meta, "0043|102f", "0"));
        std::string data_type_string;
        switch (data_type_int)
        {
        case 0:
            data_type_string = "";
            break;
        case 1:
            data_type_string = "phase_";
            break;
        case 2:
            data_type_string = "real_";
            break;
        case 3:
            data_type_string = "imag_";
            break;
        default:
            FAIL("Unknown data-type: " << data_type_int);
        }
        const auto echo_number = std::stoi(GetMetaData<std::string>(meta, "0018|0086", "1"));
        const auto filename = fmt::format("{:04d}_{}_{:02d}{}",
                                          series_number,
                                          series_description,
                                          // data_type_string,
                                          echo_number,
                                          extension);
        auto writer = itk::ImageFileWriter<Series>::New();
        writer->SetFileName(filename);
        writer->SetInput(tiler->GetOutput());
        if (verbose)
        {
            fmt::print(stderr, "Writing: {}\n", filename);
        }
        writer->Update();
    }
    catch (itk::ExceptionObject &ex)
    {
        fmt::print(stderr, "{}\n", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}