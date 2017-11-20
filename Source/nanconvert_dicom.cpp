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

#include "itkImage.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include "itkImageSeriesReader.h"
#include "itkImageFileWriter.h"

#include "Args.h"
#include "Util.h"
#include "IO.h"

/*
 * Declare args here so things like verbose can be global
 */
args::ArgumentParser parser(std::string("Convert DICOM format to whatever you want\n") + GetVersion() + "\nhttp://github.com/spinicist/nanconvert");

args::Positional<std::string> input_arg(parser, "INPUT", "Input directory.");
args::Positional<std::string> output_arg(parser, "OUTPUT", "Output file, must include extension. If the rename flag is used, only the extension is used.");

args::HelpFlag help(parser, "HELP", "Show this help menu", {'h', "help"});
args::Flag     verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
args::Flag     double_precision(parser, "DOUBLE", "Write out double precision files", {'d', "double"});
args::ValueFlagList<std::string> rename_args(parser, "RENAME", "Rename using specified header fields (can be multiple).", {'r', "rename"});
args::ValueFlag<std::string>     prefix(parser, "PREFIX", "Add a prefix to output filename.", {'p', "prefix"});

int main(int argc, char **argv) {
    ParseArgs(parser, argc, argv);
    const std::string input_dir = CheckPos(input_arg);

    auto name_generator = itk::GDCMSeriesFileNames::New();
    name_generator->SetUseSeriesDetails(true);
    name_generator->SetLoadSequences(true);
    name_generator->SetLoadPrivateTags(true);
    name_generator->AddSeriesRestriction("0043|102f"); // Magnitude/real/imaginary?
    // name_generator->AddSeriesRestriction("0018|0086"); // Echo Numbers
    name_generator->SetGlobalWarningDisplay(false);
    name_generator->SetDirectory(input_dir);

    try {
        typedef std::vector<std::string>    SeriesIdContainer;
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

            auto reader = itk::ImageSeriesReader<itk::Image<float, 4>>::New();
            auto dicomIO = itk::GDCMImageIO::New();
            reader->SetImageIO(dicomIO);
            reader->SetFileNames(fileNames);
            reader->Update();
            auto meta = dicomIO->GetMetaDataDictionary();
            std::cout << "There are " << meta.GetKeys().size() << " keys\n";
            for (const auto & s : meta.GetKeys()) { std::cout << s << "\n"; }
            if (meta.HasKey("0025|1011")) {
                std::cout << "HOORAY" << std::endl;
            } else {
                std::cout << "Awwww" << std::endl;
            }
            auto writer = itk::ImageFileWriter<itk::Image<float, 4>>::New();
            writer->SetFileName(seriesIdentifier + CheckPos(output_arg));
            writer->UseCompressionOn();
            writer->SetInput(reader->GetOutput());
            std::cout << "Writing: " << output_arg.Get() << std::endl;
            writer->Update();
        }
    } catch (itk::ExceptionObject &ex) {
        std::cout << ex << std::endl;
        return EXIT_FAILURE;
    }
  return EXIT_SUCCESS;
}