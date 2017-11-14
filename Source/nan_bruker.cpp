/*
 *  nan_bruker.cpp
 *
 *  Copyright (c) 2017 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <iostream>
#include <algorithm>

#include "itkMetaDataObject.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "Args.h"
#include "Util.h"

/*
 * Declare args here so things like verbose can be global
 */
args::ArgumentParser parser(std::string("Convert Bruker format to whatever you want\n") + GetVersion() + "\nhttp://github.com/spinicist/nanconvert");

args::Positional<std::string> input_file(parser, "INPUT", "Input file, must include extension.");
args::Positional<std::string> output_arg(parser, "OUTPUT", "Output file, must include extension. If the rename flag is used, only the extension is used.");

args::HelpFlag help(parser, "HELP", "Show this help menu", {'h', "help"});
args::Flag     verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
args::ValueFlagList<std::string> rename_args(parser, "RENAME", "Rename using specified header fields (can be multiple).", {'r', "rename"});
args::ValueFlag<std::string> prefix(parser, "PREFIX", "Add a prefix to output filename.", {'p', "prefix"});


/*
 * Templated helper functions to avoid macros
 */
template<typename TImg>
auto ReadImage(const std::string &path) -> typename TImg::Pointer {
    typedef itk::ImageFileReader<TImg> TReader;
    typename TReader::Pointer file = TReader::New();
    file->SetFileName(path);
    file->Update();
    typename TImg::Pointer img = file->GetOutput();
    if (!img) {
        FAIL("Failed to read file: " << path);
    }
    img->DisconnectPipeline();
    return img;
}

template<typename TImg>
void WriteImage(const TImg *ptr, const std::string &path) {
    typedef itk::ImageFileWriter<TImg> TWriter;
    typename TWriter::Pointer file = TWriter::New();
    file->SetFileName(path);
    file->SetInput(ptr);
    file->Update();
}

 template<typename TImage>
void Convert(const std::string &input, const std::string &output) {
    if (verbose) std::cout << "Reading image: " << input << std::endl;
    auto image = ReadImage<TImage>(input);
    if (verbose) std::cout << "Writing image: " << output << std::endl;
    WriteImage<TImage>(image, output);
    if (verbose) std::cout << "Finished." << std::endl;
}

template<int D, typename T>
void ConvertPixel(const std::string &input, const std::string &output,
                  const itk::ImageIOBase::IOPixelType &pix)
{
    switch (pix) {
        case itk::ImageIOBase::SCALAR: Convert<itk::Image<T, D>>(input, output); break;
        case itk::ImageIOBase::COMPLEX: Convert<itk::Image<std::complex<T>, D>>(input, output); break;
        default: FAIL("Unsupported pixel type in image " << input);
    }
}

template<int D>
void ConvertComponent(const std::string &input, const std::string &output,
                      const itk::ImageIOBase::IOComponentType &component,
                      const itk::ImageIOBase::IOPixelType &pix)
{
    switch (component) {
        case itk::ImageIOBase::UNKNOWNCOMPONENTTYPE: FAIL("Unknown component type in image " << input);
        case itk::ImageIOBase::UCHAR:     ConvertPixel<D, unsigned char>(input, output, pix); break;
        case itk::ImageIOBase::CHAR:      ConvertPixel<D, char>(input, output, pix); break;
        case itk::ImageIOBase::USHORT:    ConvertPixel<D, unsigned short>(input, output, pix); break;
        case itk::ImageIOBase::SHORT:     ConvertPixel<D, short>(input, output, pix); break;
        case itk::ImageIOBase::UINT:      ConvertPixel<D, unsigned int>(input, output, pix); break;
        case itk::ImageIOBase::INT:       ConvertPixel<D, int>(input, output, pix); break;
        case itk::ImageIOBase::ULONG:     ConvertPixel<D, unsigned long>(input, output, pix); break;
        case itk::ImageIOBase::LONG:      ConvertPixel<D, long>(input, output, pix); break;
        // case itk::ImageIOBase::ULONGLONG: ConvertPixel<D, unsigned long long>(input, output, pix); break;
        // case itk::ImageIOBase::LONGLONG:  ConvertPixel<D, long long>(input, output, pix); break;
        case itk::ImageIOBase::FLOAT:     ConvertPixel<D, float>(input, output, pix); break;
        case itk::ImageIOBase::DOUBLE:    ConvertPixel<D, double>(input, output, pix); break;
    }
}

void ConvertDims(const std::string &input, const std::string &output, const int D,
                 const itk::ImageIOBase::IOComponentType &component,
                 const itk::ImageIOBase::IOPixelType &pix)
{
    switch (D) {
        case 2: ConvertComponent<2>(input, output, component, pix); break;
        case 3: ConvertComponent<3>(input, output, component, pix); break;
        case 4: ConvertComponent<4>(input, output, component, pix); break;
        default: FAIL("Unsupported dimension: " << D);
    }
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
        std::vector<std::string> string_array_value;
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
        if (ExposeMetaData(header, rename_field, string_array_value)) {
            output.append(SanitiseString(string_array_value[0]));
        } else if (ExposeMetaData(header, rename_field, string_value)) {
            output.append(SanitiseString(string_value));
        } else if (ExposeMetaData(header, rename_field, double_value)) {
            std::ostringstream formatted;
            formatted << double_value;
            output.append(formatted.str());
        } else {
            FAIL("Could not determine type of rename header field:" << rename_field);
        }
    }
    return output;
}

int main(int argc, char **argv) {
    ParseArgs(parser, argc, argv);

    const std::string input = CheckPos(input_file);
    itk::ImageIOBase::Pointer header = itk::ImageIOFactory::CreateImageIO(input.c_str(), itk::ImageIOFactory::ReadMode);
    if (!header) FAIL("Could not open: " << input);
    if (verbose) std::cout << "Reading header information: " << input << std::endl;
    header->SetFileName(input);
    header->ReadImageInformation();

    /* Deal with renaming */
    std::string output_path = prefix.Get();
    if (rename_args) {
        output_path += RenameFromHeader(header->GetMetaDataDictionary());
        output_path += GetExt(CheckPos(output_arg));
    } else {
        output_path += CheckPos(output_arg);
    }
    std::cout << output_path << std::endl;

    auto dims = header->GetNumberOfDimensions();
    auto pixel_type = header->GetPixelType();
    auto component_type = header->GetComponentType();

    ConvertDims(input, output_path, dims, component_type, pixel_type);

    return EXIT_SUCCESS;
}
