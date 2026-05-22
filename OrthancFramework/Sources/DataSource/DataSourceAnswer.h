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

#if defined(__EMSCRIPTEN__)
#  error This file is currently not available if targeting WebAssembly
#endif

#include "../Compatibility.h"
#include "../IDynamicObject.h"
#include "IDataIdentifier.h"

#include <boost/shared_ptr.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <list>


namespace Orthanc
{
  class OrthancException;

  namespace Internals
  {
    class DataSourceMemoryBudget;
  }

  class ORTHANC_PUBLIC DataSourceAnswer : public boost::noncopyable
  {
  public:
    class ORTHANC_PUBLIC Item : public boost::noncopyable
    {
      friend class DataSourceAnswer;

    private:
      std::unique_ptr<IDataIdentifier>            id_;
      boost::shared_ptr<IDynamicObject>           value_;
      std::unique_ptr<OrthancException>           error_;
      boost::shared_ptr<Internals::DataSourceMemoryBudget>  budget_;
      size_t                                      memorySize_;
      std::unique_ptr<IDynamicObject>             userData_;

      Item(IDataIdentifier* id /* takes ownership */,
           boost::shared_ptr<IDynamicObject>& value,
           boost::shared_ptr<Internals::DataSourceMemoryBudget>& budget,
           size_t memorySize);

      Item(IDataIdentifier* id /* takes ownership */,
           const OrthancException& error);

    public:
      ~Item();

      const IDataIdentifier& GetId() const;

      bool HasUserData() const
      {
        return userData_.get() != NULL;
      }

      IDynamicObject* ReleaseUserData()
      {
        return userData_.release();
      }

      const boost::shared_ptr<IDynamicObject>& GetValue() const;
    };

  private:
    typedef std::list<Item*>  Content;

    boost::mutex               mutex_;
    boost::condition_variable  cond_;
    Content                    content_;
    unsigned int               countEnqueued_;
    unsigned int               finalSize_;

    void EnqueueInternal(Item* item);

  public:
    explicit DataSourceAnswer(unsigned int finalSize);

    ~DataSourceAnswer();

    Item* Dequeue();

    void EnqueueValue(IDataIdentifier* id,
                      boost::shared_ptr<IDynamicObject>& value,
                      boost::shared_ptr<Internals::DataSourceMemoryBudget>& budget,
                      size_t memorySize);

    void EnqueueError(IDataIdentifier* id,
                      const OrthancException& error);
  };
}
