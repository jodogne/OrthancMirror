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
#include "RestApiCall.h"

#include "../OrthancException.h"


namespace Orthanc
{
  void RestApiCall::GetUriComponentsNames(std::set<std::string>& components) const
  {
    components.clear();
    
    for (HttpToolbox::Arguments::const_iterator it = uriComponents_.begin();
         it != uriComponents_.end(); ++it)
    {
      components.insert(it->first);
    }
  }
  

  std::string RestApiCall::FlattenUri() const
  {
    std::string s = "/";

    for (size_t i = 0; i < fullUri_.size(); i++)
    {
      s += fullUri_[i] + "/";
    }

    return s;
  }


  RestApiCallDocumentation& RestApiCall::GetDocumentation()
  {
    if (documentation_.get() == NULL)
    {
      documentation_.reset(new RestApiCallDocumentation(method_));
    }
    
    return *documentation_;
  }


  bool RestApiCall::ParseBoolean(const std::string& value)
  {
    std::string stripped = Toolbox::StripSpaces(value);

    if (stripped == "0" ||
        stripped == "false")
    {
      return false;
    }
    else if (stripped == "1" ||
             stripped == "true")
    {
      return true;
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Boolean value expected");
    }
  }
}
