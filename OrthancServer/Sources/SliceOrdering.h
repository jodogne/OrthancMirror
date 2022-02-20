/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#pragma once

#include "../../OrthancFramework/Sources/DicomFormat/DicomMap.h"

namespace Orthanc
{
  class ServerIndex;
  
  class SliceOrdering
  {
  private:
    typedef float Vector[3];

    struct Instance;
    class  PositionComparator;

    ServerIndex&             index_;
    std::string              seriesId_;
    bool                     hasNormal_;
    Vector                   normal_;
    std::vector<Instance*>   instances_;
    bool                     isVolume_;

    static bool ComputeNormal(Vector& normal,
                              const DicomMap& dicom);

    static bool IsParallelOrOpposite(const Vector& a,
                                     const Vector& b);

    static bool IndexInSeriesComparator(const SliceOrdering::Instance* a,
                                        const SliceOrdering::Instance* b);

    void ComputeNormal();

    void CreateInstances();

    bool SortUsingPositions();

    bool SortUsingIndexInSeries();

  public:
    SliceOrdering(ServerIndex& index,
                  const std::string& seriesId);

    ~SliceOrdering();

    size_t  GetInstancesCount() const
    {
      return instances_.size();
    }

    const std::string& GetInstanceId(size_t index) const;

    unsigned int GetFramesCount(size_t index) const;

    void Format(Json::Value& result) const;
  };
}
