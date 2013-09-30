/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
 * Belgium
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


#pragma once

#include "Study.h"

namespace OrthancClient
{
  class LAAW_API Patient : 
    public Orthanc::IDynamicObject, 
    private Orthanc::ArrayFilledByThreads::IFiller
  {
  private:
    const OrthancConnection& connection_;
    std::string id_;
    Json::Value patient_;
    Orthanc::ArrayFilledByThreads  studies_;

    void ReadPatient();

    virtual size_t GetFillerSize()
    {
      return patient_["Studies"].size();
    }

    virtual Orthanc::IDynamicObject* GetFillerItem(size_t index);

  public:
    Patient(const OrthancConnection& connection,
            const char* id);

    void Reload()
    {
      studies_.Reload();
    }

    uint32_t GetStudyCount()
    {
      return studies_.GetSize();
    }

    Study& GetStudy(uint32_t index)
    {
      return dynamic_cast<Study&>(studies_.GetItem(index));
    }

    const char* GetId() const
    {
      return id_.c_str();
    }

    const char* GetMainDicomTag(const char* tag, 
                                const char* defaultValue) const;

    LAAW_API_INTERNAL void Delete();
  };
}
