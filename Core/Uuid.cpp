/**
 * Palanthir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "Uuid.h"

// http://stackoverflow.com/a/1626302

extern "C"
{
#ifdef WIN32
#include <rpc.h>
#else
#include <uuid/uuid.h>
#endif
}

namespace Palanthir
{
  namespace Toolbox
  {
    std::string GenerateUuid()
    {
#ifdef WIN32
      UUID uuid;
      UuidCreate ( &uuid );

      unsigned char * str;
      UuidToStringA ( &uuid, &str );

      std::string s( ( char* ) str );

      RpcStringFreeA ( &str );
#else
      uuid_t uuid;
      uuid_generate_random ( uuid );
      char s[37];
      uuid_unparse ( uuid, s );
#endif
      return s;
    }


    bool IsUuid(const std::string& str)
    {
      if (str.size() != 36)
      {
        return false;
      }

      for (size_t i = 0; i < str.length(); i++)
      {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
          if (str[i] != '-')
            return false;
        }
        else
        {
          if (!isalnum(str[i]))
            return false;
        }
      }

      return true;
    }
  }
}
