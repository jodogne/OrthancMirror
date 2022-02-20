/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeaders.h"
#include "RestApiGetCall.h"

#include "../OrthancException.h"
#include "../SerializationToolbox.h"

namespace Orthanc
{
  bool RestApiGetCall::ParseJsonRequest(Json::Value& result) const
  {
    result.clear();

    for (HttpToolbox::Arguments::const_iterator 
           it = getArguments_.begin(); it != getArguments_.end(); ++it)
    {
      result[it->first] = it->second;
    }

    return true;
  }

  
  bool RestApiGetCall::GetBooleanArgument(const std::string& name,
                                          bool defaultValue) const
  {
    HttpToolbox::Arguments::const_iterator found = getArguments_.find(name);

    bool value;
    
    if (found == getArguments_.end())
    {
      return defaultValue;
    }
    else if (found->second.empty())
    {
      return true;
    }
    else if (SerializationToolbox::ParseBoolean(value, found->second))
    {
      return value;
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "Expected a Boolean for GET argument \"" +
                             name + "\", found: " + found->second);
    }
  }
}
