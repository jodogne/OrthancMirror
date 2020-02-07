/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

#include "../Core/DicomFormat/DicomMap.h"

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
    std::vector<Instance*>   instances_;        // this vector owns the instances
    std::vector<Instance*>   sortedInstances_;  // this vectore references the instances of instances_
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

    size_t  GetSortedInstancesCount() const
    {
      return sortedInstances_.size();
    }

    const std::string& GetSortedInstanceId(size_t index) const;

    unsigned int GetSortedInstanceFramesCount(size_t index) const;

    void Format(Json::Value& result) const;
  };
}
