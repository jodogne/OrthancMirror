/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#pragma once

#include "../MultiThreading/IExecutorService.h"
#include "DataSourceAnswer.h"


namespace Orthanc
{
  class DataSourceReader;
  class OrthancException;

  /**
   * This class retrieves items from a data source in a deterministic
   * order while still using parallel execution through a thread pool.
   *
   * Internally, it uses a sliding-window approach. Since items may
   * complete out of order, the class buffers them until earlier items
   * become available. This additional buffering is necessary because
   * applying backpressure (as done in "DataSourceReader::Submit()")
   * could otherwise lead to deadlock. If item ordering is not
   * required, prefer "DataSourceReader::Submit()", which supports
   * backpressure and avoids the extra memory overhead.
   *
   * Note that this class is not thread safe, it should be invoked
   * from a single thread.
   **/
  class ORTHANC_PUBLIC DataSourceSequentialReader : public boost::noncopyable
  {
  public:
    class ORTHANC_PUBLIC Item : public IDynamicObject
    {
    private:
      std::unique_ptr<IDynamicObject>    value_;
      std::unique_ptr<OrthancException>  error_;
      std::unique_ptr<IDynamicObject>    userData_;
      size_t                             estimatedSize_;

    public:
      Item(IDynamicObject* value,
           size_t estimatedSize);

      Item(const OrthancException& error);

      const IDynamicObject& GetValue() const;

      void SetUserData(IDynamicObject* userData);

      bool HasUserData() const
      {
        return userData_.get() != NULL;
      }

      const IDynamicObject& GetUserData() const;

      size_t GetEstimatedSize() const
      {
        return estimatedSize_;
      }

      IDynamicObject* ReleaseValue();

      IDynamicObject* ReleaseUserData();
    };


    /**
     * Extracts the value from an answer item so that it can be
     * buffered independently and is no longer subject to
     * backpressure.
     **/
    class ORTHANC_PUBLIC IValueDisconnector : public boost::noncopyable
    {
    public:
      virtual ~IValueDisconnector()
      {
      }

      // Note that "source" will never contain user data, those are handled at the level above
      virtual IDynamicObject* Apply(DataSourceAnswer::Item* source) = 0;
    };


  private:
    class Callable;

    typedef std::list<IDataIdentifier*>  PendingRequests;
    typedef std::list<Future*>           RunningRequests;

    boost::shared_ptr<IExecutorService>    executor_;
    boost::shared_ptr<DataSourceReader>    reader_;
    boost::shared_ptr<IValueDisconnector>  disconnector_;
    unsigned int                           windowSize_;
    uint64_t                               windowCapacity_;

    bool             started_;
    PendingRequests  pendingRequests_;
    RunningRequests  runningRequests_;
    uint64_t         runningSize_;

    void FillWindow();

  public:
    DataSourceSequentialReader(const boost::shared_ptr<IExecutorService>& executor,
                               const boost::shared_ptr<DataSourceReader>& reader,
                               IValueDisconnector* disconnector /* takes ownership */,
                               unsigned int windowSize /* number of elements in the sliding window */,
                               uint64_t windowCapacity /* if 0, memory usage is only controlled by the size of the sliding window */);

    ~DataSourceSequentialReader();

    void Submit(IDataIdentifier* request /* takes ownership */);

    void Start();

    bool HasNext() const;

    Item* Next();
  };
}
