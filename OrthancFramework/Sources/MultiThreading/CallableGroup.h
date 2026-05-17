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

#include "IExecutorService.h"

#include <list>


namespace Orthanc
{
  // WARNING: This class is *not* thread-safe. It must be invoked from a single thread.
  class CallableGroup : public boost::noncopyable
  {
  private:
    boost::shared_ptr<IExecutorService>  executor_;
    unsigned int                         windowSize_;
    std::list<ICallable*>                pending_;
    std::list<Future*>                   futures_;
    bool                                 hasIterator_;

    void FillWindow();

  public:
    // A window size of "0" means "submit immediately all the callables"
    CallableGroup(boost::shared_ptr<IExecutorService>  executor,
                  unsigned int windowSize);

    ~CallableGroup();

    void Submit(ICallable* callable /* takes ownership */);

    class Iterator : public boost::noncopyable
    {
    private:
      CallableGroup&  that_;

    public:
      explicit Iterator(CallableGroup& that);

      bool HasNext() const;

      IDynamicObject* Next();
    };
  };
}
