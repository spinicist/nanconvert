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

#include <cstdlib>
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
args::Flag     verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
args::Flag     double_precision(parser, "DOUBLE", "Write out double precision files", {'d', "double"});
args::ValueFlagList<std::string> rename_args(parser, "RENAME", "Rename using specified header fields (can be multiple).", {'r', "rename"});
args::ValueFlag<std::string>     prefix(parser, "PREFIX", "Add a prefix to output filename.", {'p', "prefix"});

typedef itk::Image<float, 3> VolumeF;
typedef itk::Image<float, 4> SeriesF;

int main(int argc, char **argv) {
    ParseArgs(parser, argc, argv);
    const std::string input_dir = CheckPos(input_arg);
    const std::string extension = GetExt(CheckPos(output_arg));
    auto name_generator = itk::GDCMSeriesFileNames::New();
    name_generator->SetUseSeriesDetails(true);
    name_generator->SetLoadSequences(true);
    name_generator->SetLoadPrivateTags(true);
    name_generator->AddSeriesRestriction("0018|0086"); // Echo numbers
    name_generator->AddSeriesRestriction("0043|102f"); // Magnitude/real/imaginary?
    name_generator->SetGlobalWarningDisplay(false);
    name_generator->SetDirectory(input_dir);

    try {
        const std::vector<std::string> &seriesUID = name_generator->GetSeriesUIDs();
        std::vector<std::string>::const_iterator series_it = seriesUID.begin();

        if (series_it != seriesUID.end()) {
            std::cout << "The directory: ";
            std::cout << input_dir << std::endl;
            std::cout << "Contains the following DICOM Series: ";
            std::cout << std::endl;
        } else {
            std::cout << "No DICOMs in: " << input_dir << std::endl;
            return EXIT_SUCCESS;
        }

        while (series_it != seriesUID.end()) {
            std::cout  << *series_it << std::endl;
            ++series_it;
        }

        series_it = seriesUID.begin();
        while (series_it != seriesUID.end()) {
            std::string seriesIdentifier = *series_it;
            ++series_it;
            std::cout << "\nReading: " << seriesIdentifier << std::endl;
            std::vector<std::string> fileNames = name_generator->GetFileNames(seriesIdentifier);

            // std::cout << "DICOM names:\n";
            // for (const auto &s : fileNames) { std::cout << s << "\n"; }

            auto reader = itk::ImageSeriesReader<itk::Image<float, 3>>::New();
            auto dicomIO = itk::GDCMImageIO::New();
            dicomIO->LoadPrivateTagsOn();
            reader->SetImageIO(dicomIO);
            reader->SetFileNames(fileNames);
            reader->Update();

            /* GDCM reads these DICOMs with all volumes concatenated along Z.
             * So break them up along Z and stitch them back together in T
             */
            auto meta = dicomIO->GetMetaDataDictionary();
            const int n_slices = std::stoi(GetMetaData<std::string>(meta, "0021|104f")); // LocationsInAcquisition
            auto stacked = reader->GetOutput();
            const int stacked_size = stacked->GetLargestPossibleRegion().GetSize()[2];
            const int n_vols = stacked_size / n_slices;
            std::cout << "Header slices: " << n_slices << " Stacked Slices: " << stacked_size << " Volumes: " << n_vols << std::endl;
            itk::FixedArray<unsigned int, 4> layout;
            layout[0] = layout[1] = layout[2] = 1;
            layout[3] = n_vols;
            auto tiler = itk::TileImageFilter<VolumeF, SeriesF>::New();
            tiler->SetLayout(layout);
            auto roi = stacked->GetLargestPossibleRegion();
            roi.GetModifiableSize()[2] = n_slices;
            for (int i = 0; i < n_vols; i ++) {
                auto one_vol = itk::RegionOfInterestImageFilter<VolumeF, VolumeF>::New();
                one_vol->SetRegionOfInterest(roi);
                one_vol->SetInput(stacked);
                one_vol->Update();
                tiler->SetInput(i, one_vol->GetOutput());
                roi.GetModifiableIndex()[2] += n_slices;
            }
            std::cout << "Unstacking" << std::endl;
            tiler->Update();
            const auto series_number      = std::stoi(GetMetaData<std::string>(meta, "0020|0011"));
            const auto series_description = GetMetaData<std::string>(meta, "0008|103e");
            const auto data_type_int = std::stoi(GetMetaData<std::string>(meta, "0043|102f"));
            std::string data_type_string;
            switch (data_type_int) {
                case 0: data_type_string = ""; break;
                case 1: data_type_string = "_phase"; break;
                case 2: data_type_string = "_real"; break;
                case 3: data_type_string = "_imag"; break;
                default: FAIL("Unknown data-type: " << data_type_int);
            }
            const auto echo_time = std::stoi(GetMetaData<std::string>(meta, "0018|0086"));
            const auto filename = fmt::format("{:04d}_{}{}_{:02d}{}",
                                                series_number,
                                                SanitiseString(series_description.substr(0, series_description.size()-1)),
                                                data_type_string,
                                                echo_time,
                                                extension);
            auto writer = itk::ImageFileWriter<itk::Image<float, 4>>::New();
            writer->SetFileName(filename);
            writer->SetInput(tiler->GetOutput());
            std::cout << "Writing: " << filename << std::endl;
            writer->Update();
        }
    } catch (itk::ExceptionObject &ex) {
        std::cout << ex << std::endl;
        return EXIT_FAILURE;
    }
  return EXIT_SUCCESS;
}