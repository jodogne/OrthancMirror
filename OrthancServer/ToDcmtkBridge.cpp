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


#include "ToDcmtkBridge.h"

#include <memory>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmnet/diutil.h>


namespace Palanthir
{
  DcmTagKey ToDcmtkBridge::Convert(const DicomTag& tag)
  {
    return DcmTagKey(tag.GetGroup(), tag.GetElement());
  }


  DcmDataset* ToDcmtkBridge::Convert(const DicomMap& map)
  {
    std::auto_ptr<DcmDataset> result(new DcmDataset);

    for (DicomMap::Map::const_iterator 
           it = map.map_.begin(); it != map.map_.end(); it++)
    {
      std::string s = it->second->AsString();
      DU_putStringDOElement(result.get(), Convert(it->first), s.c_str());
    }

    return result.release();
  }
}
