/*
 *  IO.cpp
 *
 *  Copyright (c) 2017 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  Contains template definitions and explicit instantiations (see note in IO.h)
 */

#include "IO.h"
#include "Macro.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"

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

template auto ReadImage<itk::Image<float, 2u>>(const std::string &path) -> itk::Image<float, 2u>::Pointer;
template auto ReadImage<itk::Image<float, 3u>>(const std::string &path) -> itk::Image<float, 3u>::Pointer;
template auto ReadImage<itk::Image<float, 4u>>(const std::string &path) -> itk::Image<float, 4u>::Pointer;
template auto ReadImage<itk::Image<double, 2u>>(const std::string &path) -> itk::Image<double, 2u>::Pointer;
template auto ReadImage<itk::Image<double, 3u>>(const std::string &path) -> itk::Image<double, 3u>::Pointer;
template auto ReadImage<itk::Image<double, 4u>>(const std::string &path) -> itk::Image<double, 4u>::Pointer;

template<typename TImg>
void WriteImage(const TImg *ptr, const std::string &path) {
    typedef itk::ImageFileWriter<TImg> TWriter;
    typename TWriter::Pointer file = TWriter::New();
    file->SetFileName(path);
    file->SetInput(ptr);
    file->Update();
}

template void WriteImage<itk::Image<float, 2u>>(const itk::Image<float, 2u> *ptr, const std::string &path);
template void WriteImage<itk::Image<float, 3u>>(const itk::Image<float, 3u> *ptr, const std::string &path);
template void WriteImage<itk::Image<float, 4u>>(const itk::Image<float, 4u> *ptr, const std::string &path);
template void WriteImage<itk::Image<double, 2u>>(const itk::Image<double, 2u> *ptr, const std::string &path);
template void WriteImage<itk::Image<double, 3u>>(const itk::Image<double, 3u> *ptr, const std::string &path);
template void WriteImage<itk::Image<double, 4u>>(const itk::Image<double, 4u> *ptr, const std::string &path);
