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
            if (verbose) {
                std::cout << "The directory: ";
                std::cout << input_dir << std::endl;
                std::cout << "Contains the following DICOM Series: ";
                std::cout << std::endl;
            }
        } else {
            std::cout << "No DICOMs in: " << input_dir << std::endl;
            return EXIT_SUCCESS;
        }

        while (series_it != seriesUID.end()) {
            if (verbose) std::cout  << *series_it << std::endl;
            ++series_it;
        }

        series_it = seriesUID.begin();
        while (series_it != seriesUID.end()) {
            std::string seriesIdentifier = *series_it;
            ++series_it;
            if (verbose) std::cout << "Reading: " << seriesIdentifier << std::endl;
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
            const int n_slices = std::stoi(GetMetaData<std::string>(meta, "0021|104f", "1")); // LocationsInAcquisition
            auto stacked = reader->GetOutput();
            const int stacked_size = stacked->GetLargestPossibleRegion().GetSize()[2];
            const int n_vols = (stacked_size < n_slices) ? 1 : stacked_size / n_slices; // RUFIS data has funny LocationsInAcquisition value
            if (verbose) std::cout << "Header slices: " << n_slices << " Stacked Slices: " << stacked_size << " Volumes: " << n_vols << std::endl;
            itk::FixedArray<unsigned int, 4> layout;
            layout[0] = layout[1] = layout[2] = 1;
            layout[3] = n_vols;
            auto tiler = itk::TileImageFilter<VolumeF, SeriesF>::New();
            tiler->SetLayout(layout);
            auto roi = stacked->GetLargestPossibleRegion();
            if (n_vols > 1)
                roi.GetModifiableSize()[2] = n_slices;
            for (int i = 0; i < n_vols; i ++) {
                auto one_vol = itk::RegionOfInterestImageFilter<VolumeF, VolumeF>::New();
                one_vol->SetRegionOfInterest(roi);
                one_vol->SetInput(stacked);
                one_vol->Update();
                tiler->SetInput(i, one_vol->GetOutput());
                roi.GetModifiableIndex()[2] += n_slices;
            }
            if (verbose) std::cout << "Unstacking" << std::endl;
            tiler->Update();
            auto out_image = tiler->GetOutput();
            // Fix header information
            auto spacing   = out_image->GetSpacing();
            auto origin    = out_image->GetOrigin();
            auto direction = out_image->GetDirection();
            direction.SetIdentity();
            for (size_t i = 0; i < 3; i++) {
                spacing[i] = stacked->GetSpacing()[i];
                origin[i] =  stacked->GetOrigin()[i];
                for (size_t j = 0; j < 3; j++) {
                    direction[i][j] = stacked->GetDirection()[i][j];
                }
            }
            //spacing[3] = std::stod(GetMetaData<std::string>(meta, "0018|0080")); // Repetition Time - currently ITK doesn't respect this
            out_image->SetSpacing(spacing);
            out_image->SetOrigin(origin);
            out_image->SetDirection(direction);
            const auto series_number      = std::stoi(GetMetaData<std::string>(meta, "0020|0011"));
            const auto series_description = SanitiseString(Trim(GetMetaData<std::string>(meta, "0008|103e")));
            const auto data_type_int = std::stoi(GetMetaData<std::string>(meta, "0043|102f", "0"));
            std::string data_type_string;
            switch (data_type_int) {
                case 0: data_type_string = ""; break;
                case 1: data_type_string = "phase_"; break;
                case 2: data_type_string = "real_"; break;
                case 3: data_type_string = "imag_"; break;
                default: FAIL("Unknown data-type: " << data_type_int);
            }
            const auto echo_number = std::stoi(GetMetaData<std::string>(meta, "0018|0086", "1"));
            const auto filename = fmt::format("{:04d}_{}_{}{:02d}{}",
                                                series_number,
                                                series_description,
                                                data_type_string,
                                                echo_number,
                                                extension);
            auto writer = itk::ImageFileWriter<itk::Image<float, 4>>::New();
            writer->SetFileName(filename);
            writer->SetInput(out_image);
            if (verbose) std::cout << "Writing: " << filename << std::endl;
            writer->Update();
        }
    } catch (itk::ExceptionObject &ex) {
        std::cerr << ex << std::endl;
        return EXIT_FAILURE;
    }
  return EXIT_SUCCESS;
}