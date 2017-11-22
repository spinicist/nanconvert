/*
 *  nanconvert_bruker.cpp
 *
 *  Copyright (c) 2017 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <iomanip>

#include "itkImage.h"
#include "itkMetaDataObject.h"
#include "itkImageIOFactory.h"

#include "Args.h"
#include "Util.h"
#include "IO.h"

/*
 * Declare args here so things like verbose can be global
 */
args::ArgumentParser parser(std::string("Convert Bruker format to whatever you want\n") + GetVersion() + "\nhttp://github.com/spinicist/nanconvert");

args::Positional<std::string> input_file(parser, "INPUT", "Input file, must include extension.");
args::Positional<std::string> output_arg(parser, "OUTPUT", "Output file, must include extension. If the rename flag is used, only the extension is used.");

args::HelpFlag help(parser, "HELP", "Show this help menu", {'h', "help"});
args::Flag     verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
args::Flag     double_precision(parser, "DOUBLE", "Write out double precision files", {'d', "double"});
args::Flag     scale(parser, "SCALE", "Scale image x10 for compatibility with SPM etc.", {'s', "scale"});
args::ValueFlagList<std::string> rename_args(parser, "RENAME", "Rename using specified header fields (can be multiple).", {'r', "rename"});
args::ValueFlag<std::string>     prefix(parser, "PREFIX", "Add a prefix to output filename.", {'p', "prefix"});

/*
 * Helper function to recover a value from an ITK meta-data dictionary
 */
template<typename T>
T GetParameter(const itk::MetaDataDictionary &dict, const std::string &name) {
    T value;
    if (!ExposeMetaData(dict, name, value)) {
        FAIL("Could not read parameter: " << name);
    }
    return value;
}

/*
 * Helper function to sanitise meta-data to be suitable for a filename
 */
std::string SanitiseString(const std::string &s) {
    const std::string forbidden = " \\/:?\"<>|*+-=";
    std::string out(s.size(), ' ');
    std::transform(s.begin(), s.end(), out.begin(),
                   [&forbidden](char c) { return forbidden.find(c) != std::string::npos ? '_' : c; });
    return out;
}

/*
 * Helper function to work out the name of the output file
 */
std::string RenameFromHeader(const itk::MetaDataDictionary &header) {
    bool append_delim = false;
    std::string output;
    for (const auto rename_field: args::get(rename_args)) {
        std::vector<std::vector<std::string>> string_array_array_value;
        std::vector<std::string> string_array_value;
        std::vector<double> double_array_value;
        std::string string_value;
        double double_value;
        if (!header.HasKey(rename_field)) {
            if (verbose) std::cout << "Rename field '" << rename_field << "' not found in header. Ignoring" << std::endl;
            continue;
        }
        if (append_delim) {
            output.append("_");
        } else {
            append_delim = true;
        }
        if (ExposeMetaData(header, rename_field, string_array_array_value)) {
            output.append(SanitiseString(string_array_array_value[0][0]));
        } else if (ExposeMetaData(header, rename_field, string_array_value)) {
            output.append(SanitiseString(string_array_value[0]));
        } else if (ExposeMetaData(header, rename_field, string_value)) {
            output.append(SanitiseString(string_value));
        } else if (ExposeMetaData(header, rename_field, double_array_value)) {
            std::ostringstream formatted;
            formatted << double_array_value;
            output.append(SanitiseString(formatted.str()));
        } else if (ExposeMetaData(header, rename_field, double_value)) {
            std::ostringstream formatted;
            formatted << double_value;
            output.append(SanitiseString(formatted.str()));
        } else {
            if (verbose) std::cout << "Could not determine type of rename header field, ignoring:" << rename_field << std::endl;
        }
    }
    return output;
}

/*
 * Templated conversion functions to avoid macros
 */
template<typename T, int D>
void Convert(const std::string &input, const std::string &output) {
    typedef itk::Image<T, D> TImage;
    if (verbose) std::cout << "Reading image: " << input << std::endl;
    auto image = ReadImage<TImage>(input);
    if (scale) {
        auto spacing = image->GetSpacing();
        auto origin  = image->GetOrigin();
        for (int i = 0; i < D; i++) {
            spacing[i] *= 10;
            origin[i]  *= 10;
        }
        image->SetSpacing(spacing);
        image->SetOrigin(origin);
    }
    if (verbose) std::cout << "Writing image: " << output << std::endl;
    WriteImage<TImage>(image, output);
    if (verbose) std::cout << "Finished." << std::endl;
}

template<typename T>
void ConvertFile(const std::string &input, const std::string &output, const int D) {
    switch (D) {
        case 2: Convert<T, 2>(input, output); break;
        case 3: Convert<T, 3>(input, output); break;
        case 4: Convert<T, 4>(input, output); break;
        default: FAIL("Unsupported dimension: " << D);
    }
}

int main(int argc, char **argv) {
    ParseArgs(parser, argc, argv);

    const std::string input = CheckPos(input_file);
    itk::ImageIOBase::Pointer header = itk::ImageIOFactory::CreateImageIO(input.c_str(), itk::ImageIOFactory::ReadMode);
    if (!header) FAIL("Could not open: " << input);
    if (verbose) std::cout << "Reading header information: " << input << std::endl;
    header->SetFileName(input);
    header->ReadImageInformation();
    auto dict = header->GetMetaDataDictionary();

    /* Deal with renaming */
    std::string output_path = prefix.Get();
    if (rename_args) {
        output_path += RenameFromHeader(dict);
        output_path += GetExt(CheckPos(output_arg));
    } else {
        output_path += CheckPos(output_arg);
    }
    
    auto dims = header->GetNumberOfDimensions();
    /* We don't need the pixel type because Bruker 'complex' images are real volumes then imaginary volumes */

    if (double_precision) {
        ConvertFile<double>(input, output_path, dims);
    } else {
        ConvertFile<float>(input, output_path, dims);
    }

    if (dict.HasKey("PVM_DwEffBval")) {
        /* It's a diffusion image, write out the b-values and vectors */
        auto bvals = GetParameter<std::vector<double>>(dict, "PVM_DwEffBval");
        auto bvecs = GetParameter<std::vector<double>>(dict, "PVM_DwGradVec");
        // GradAmp is stored in percent - convert to fraction
        auto bvec_scale = GetParameter<std::vector<double>>(dict, "PVM_DwGradAmp")[0] / 100.;

        std::ofstream bvals_file(StripExt(output_path) + ".bval");
        for (const auto &bval : bvals) {
            bvals_file << bval << "\t";
        }

        std::ofstream bvecs_file(StripExt(output_path) + ".bvec");
        bvecs_file << std::setprecision(std::numeric_limits<double>::digits10 + 1);
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < bvals.size(); ++col) {
                bvecs_file << bvecs[col*3 + row] / bvec_scale << "\t";
            }
            bvecs_file << "\n";
        }
    }

    return EXIT_SUCCESS;
}
