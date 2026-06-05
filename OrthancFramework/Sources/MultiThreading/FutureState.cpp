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


#include "../PrecompiledHeaders.h"
#include "FutureState.h"

#include <cassert>


namespace Orthanc
{
  namespace Internals
  {
    IDynamicObject* FutureState::ReleaseInternal(bool hasTimeout,
                                                 unsigned int millisecondsTimeout)
    {
      boost::mutex::scoped_lock lock(mutex_);

      for (;;)
      {
        switch (state_)
        {
          case State_Pending:
            if (hasTimeout)
            {
              bool success = completed_.timed_wait(lock, boost::posix_time::milliseconds(millisecondsTimeout));
              if (success)
              {
                if (state_ != State_Success &&
                    state_ != State_Failed)
                {
                  THROW_WITH_FILE_AND_LINE_INFO(ErrorCode_InternalError);  // Should never happen
                }
              }
            }
            else
            {
              completed_.wait(lock);
            }
            break;

          case State_Canceled:
            throw OrthancException(ErrorCode_CanceledJob);

          case State_Failed:
            if (error_.get() == NULL)
            {
              throw OrthancException(ErrorCode_BadSequenceOfCalls, "The result of the future has already been released");
            }
            else
            {
              std::unique_ptr<OrthancException> released(error_.release());
              throw OrthancException(*released);
            }

          case State_Success:
            if (result_.get() == NULL)
            {
              throw OrthancException(ErrorCode_BadSequenceOfCalls, "The result of the future has already been released");
            }
            else
            {
              return result_.release();
            }

          default:
            THROW_WITH_FILE_AND_LINE_INFO(ErrorCode_InternalError);  // Should never happen
        }
      }
    }


    void FutureState::Cancel() ORTHANC_NOEXCEPT
    {
      boost::mutex::scoped_lock lock(mutex_);

      if (state_ == State_Pending)
      {
        state_ = State_Canceled;
        completed_.notify_all();
      }
    }


    void FutureState::AcquireResult(IDynamicObject* result)
    {
      std::unique_ptr<IDynamicObject> protection(result);

      if (result == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
      else
      {
        boost::mutex::scoped_lock lock(mutex_);

        switch (state_)
        {
          case State_Pending:
            assert(result_.get() == NULL);
            assert(error_.get() == NULL);

            result_.reset(protection.release());
            state_ = State_Success;
            completed_.notify_all();
            break;

          case State_Canceled:
            break;  // Ignore

          case State_Failed:
          case State_Success:
            throw OrthancException(ErrorCode_BadSequenceOfCalls);

          default:
            THROW_WITH_FILE_AND_LINE_INFO(ErrorCode_InternalError);  // Should never happen
        }
      }
    }


    void FutureState::SetError(const OrthancException& error)
    {
      boost::mutex::scoped_lock lock(mutex_);

      switch (state_)
      {
        case State_Pending:
          assert(result_.get() == NULL);
          assert(error_.get() == NULL);

          error_.reset(new OrthancException(error));
          state_ = State_Failed;
          completed_.notify_all();
          break;

        case State_Canceled:
          break;  // Ignore

        case State_Failed:
        case State_Success:
          throw OrthancException(ErrorCode_BadSequenceOfCalls);

        default:
          THROW_WITH_FILE_AND_LINE_INFO(ErrorCode_InternalError);  // Should never happen
      }
    }
  }
}
