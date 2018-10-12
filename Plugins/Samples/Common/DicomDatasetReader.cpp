/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "DicomDatasetReader.h"

#include "OrthancPluginException.h"

#include <boost/lexical_cast.hpp>

namespace OrthancPlugins
{
  // This function is copied-pasted from "../../../Core/Toolbox.cpp",
  // in order to avoid the dependency of plugins against the Orthanc core
  static std::string StripSpaces(const std::string& source)
  {
    size_t first = 0;

    while (first < source.length() &&
           isspace(source[first]))
    {
      first++;
    }

    if (first == source.length())
    {
      // String containing only spaces
      return "";
    }

    size_t last = source.length();
    while (last > first &&
           isspace(source[last - 1]))
    {
      last--;
    }          
    
    assert(first <= last);
    return source.substr(first, last - first);
  }


  DicomDatasetReader::DicomDatasetReader(const IDicomDataset& dataset) :
    dataset_(dataset)
  {
  }
  

  std::string DicomDatasetReader::GetStringValue(const DicomPath& path,
                                                 const std::string& defaultValue) const
  {
    std::string s;
    if (dataset_.GetStringValue(s, path))
    {
      return s;
    }
    else
    {
      return defaultValue;
    }
  }


  std::string DicomDatasetReader::GetMandatoryStringValue(const DicomPath& path) const
  {
    std::string s;
    if (dataset_.GetStringValue(s, path))
    {
      return s;
    }
    else
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(InexistentTag);
    }
  }


  template <typename T>
  static bool GetValueInternal(T& target,
                               const IDicomDataset& dataset,
                               const DicomPath& path)
  {
    try
    {
      std::string s;

      if (dataset.GetStringValue(s, path))
      {
        target = boost::lexical_cast<T>(StripSpaces(s));
        return true;
      }
      else
      {
        return false;
      }
    }
    catch (boost::bad_lexical_cast&)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);        
    }
  }


  bool DicomDatasetReader::GetIntegerValue(int& target,
                                           const DicomPath& path) const
  {
    return GetValueInternal<int>(target, dataset_, path);
  }


  bool DicomDatasetReader::GetUnsignedIntegerValue(unsigned int& target,
                                                   const DicomPath& path) const
  {
    int value;

    if (!GetIntegerValue(value, path))
    {
      return false;
    }
    else if (value >= 0)
    {
      target = static_cast<unsigned int>(value);
      return true;
    }
    else
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(ParameterOutOfRange);
    }
  }


  bool DicomDatasetReader::GetFloatValue(float& target,
                                         const DicomPath& path) const
  {
    return GetValueInternal<float>(target, dataset_, path);
  }


  bool DicomDatasetReader::GetDoubleValue(double& target,
                                          const DicomPath& path) const
  {
    return GetValueInternal<double>(target, dataset_, path);
  }
}
