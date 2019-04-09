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

#include "fmt/format.h"
#include "fmt/ostream.h"
#include "itkImage.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include "itkImageSeriesReader.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkTileImageFilter.h"

#include "Args.h"
#include "Util.h"
#include "IO.h"

/*
 * Declare args here so things like verbose can be global
 */
args::ArgumentParser parser("Convert DICOM format to whatever you want\nhttp://github.com/spinicist/nanconvert");

args::Positional<std::string> input_arg(parser, "INPUT", "Input directory");
args::Positional<std::string> output_arg(parser, "EXTENSION", "Output extension");

args::HelpFlag help(parser, "HELP", "Show this help menu", {'h', "help"});
args::Flag verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
args::Flag double_precision(parser, "DOUBLE", "Write out double precision files", {'d', "double"});
args::ValueFlagList<std::string> rename_args(parser, "RENAME", "Rename using specified header fields (can be multiple).", {'r', "rename"});
args::ValueFlag<std::string> prefix(parser, "PREFIX", "Add a prefix to output filename.", {'p', "prefix"});

using Slice = itk::Image<float, 2>;
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
    //* name_generator->AddSeriesRestriction("0018|1314"); // Flip Angle
    //* name_generator->AddSeriesRestriction("0018|0080"); // Repetition time
    // name_generator->AddSeriesRestriction("0018|0086"); // Echo numbers
    // name_generator->AddSeriesRestriction("0018|1060"); // Trigger time
    //* name_generator->AddSeriesRestriction("0008|0032"); // Acquisition time
    //* name_generator->AddSeriesRestriction("0008|0031"); // Series time
    //* name_generator->AddSeriesRestriction("0008|0033"); // Content time
    // name_generator->AddSeriesRestriction("0018|9087"); // Diffusion b0
    // name_generator->AddSeriesRestriction("0008|0008"); // Image Type (CASL)
    name_generator->AddSeriesRestriction("0043|1030"); // "Vas collapse flag"
    name_generator->AddSeriesRestriction("0027|1035"); // Plane Type / Orientation
    // name_generator->AddSeriesRestriction("0020|0100"); // Temporal Position Identifier
    // name_generator->AddSeriesRestriction("0018|1250"); // Coil (for ASSET)
    name_generator->AddSeriesRestriction("0043|102f"); // Magnitude/real/imaginary
    name_generator->SetGlobalWarningDisplay(false);
    name_generator->SetDirectory(input_dir);

    try
    {
        auto seriesUIDs = name_generator->GetSeriesUIDs();
        if (seriesUIDs.size() == 0)
        {
            fmt::print("No DICOMs in: {}", input_dir);
            return EXIT_SUCCESS;
        }
        else
        {
            if (verbose)
            {
                fmt::print("Directory: {}\nContains {} DICOM Series\n", input_dir, seriesUIDs.size());
            }
        }

        for (auto const &seriesID : seriesUIDs)
        {
            if (verbose)
            {
                fmt::print("Reading: {}\n", seriesID);
            }
            std::vector<std::string> fileNames = name_generator->GetFileNames(seriesID);
            struct dicom_entry
            {
                Slice::Pointer image;
                int slice, echo, b0, temporal, instance;
                std::string casl, coil;
            };
            std::vector<dicom_entry> dicoms(fileNames.size());

            auto reader = itk::ImageFileReader<Slice>::New();
            auto dicomIO = itk::GDCMImageIO::New();
            dicomIO->LoadPrivateTagsOn();
            reader->SetImageIO(dicomIO);
            itk::MetaDataDictionary meta;
            for (size_t i = 0; i < fileNames.size(); i++)
            {
                reader->SetFileName(fileNames[i]);
                reader->Update();
                meta = dicomIO->GetMetaDataDictionary();
                const auto slice = std::stoi(GetMetaData<std::string>(meta, "0020|9057"));
                const auto echo = std::stoi(GetMetaData<std::string>(meta, "0018|0086", "0"));
                const auto b0 = std::stoi(GetMetaData<std::string>(meta, "0018|9087", "0"));
                const auto temporal = std::stoi(GetMetaData<std::string>(meta, "0020|0100", "0"));
                const auto instance = std::stoi(GetMetaData<std::string>(meta, "0020|0013"));
                const auto casl = GetMetaData<std::string>(meta, "0008|0008", "0");
                const auto coil = GetMetaData<std::string>(meta, "0018|1250", "0");
                dicoms[i] = {reader->GetOutput(), slice, echo, b0, temporal, instance, casl, coil};
                reader->GetOutput()->DisconnectPipeline();
            }

            std::sort(dicoms.begin(),
                      dicoms.end(),
                      [](dicom_entry &a, dicom_entry &b) {
                          return (a.slice < b.slice) ||
                                 ((a.slice == b.slice) && ((a.echo < b.echo) ||
                                                           (a.b0 < b.b0) ||
                                                           (a.casl < b.casl) ||
                                                           (a.temporal < b.temporal) ||
                                                           (a.coil < b.coil) ||
                                                           (a.instance < b.instance)));
                      });

            const auto slocs = std::stoi(GetMetaData<std::string>(meta, "0021|104f"));
            const auto vols = fileNames.size() / slocs;

            itk::FixedArray<unsigned int, 4> layout;
            layout[0] = layout[1] = 1;
            layout[2] = slocs;
            layout[3] = vols;
            auto tiler = itk::TileImageFilter<Slice, Series>::New();
            tiler->SetLayout(layout);

            int tilerIndex = 0;
            for (unsigned int v = 0; v < vols; v++)
            {
                int dicomIndex = v;
                for (int s = 0; s < slocs; s++)
                {
                    auto slice = dicoms[dicomIndex].image;
                    dicomIndex += vols;
                    tiler->SetInput(tilerIndex++, slice);
                }
            }
            tiler->Update();
            const auto series_number = std::stoi(GetMetaData<std::string>(meta, "0020|0011"));
            const auto series_description = SanitiseString(Trim(GetMetaData<std::string>(meta, "0008|103e")));
            const auto data_type_int = std::stoi(GetMetaData<std::string>(meta, "0043|102f", "0"));
            const auto slice_thickness = std::stof(GetMetaData<std::string>(meta, "0018|0050", "1"));
            const auto TR = std::stof(GetMetaData<std::string>(meta, "0018|0080", "1"));
            Series::Pointer image = tiler->GetOutput();
            image->DisconnectPipeline();
            auto spacing = image->GetSpacing();
            spacing[2] = slice_thickness;
            spacing[3] = TR;
            image->SetSpacing(spacing);
            std::string data_type_string;
            switch (data_type_int)
            {
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
            const auto filename = fmt::format("{:04d}_{}{}{}",
                                              series_number,
                                              series_description,
                                              data_type_string,
                                              extension);
            auto writer = itk::ImageFileWriter<Series>::New();
            writer->SetFileName(filename);
            writer->SetInput(image);
            if (verbose)
            {
                fmt::print("Writing: {}\n", filename);
            }
            writer->Update();
        }
    }
    catch (itk::ExceptionObject &ex)
    {
        fmt::print("{}", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}