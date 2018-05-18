/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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


#include "../../PrecompiledHeaders.h"
#include "SequenceOfOperationsJob.h"

#include "../../Logging.h"
#include "../../OrthancException.h"

namespace Orthanc
{
  class SequenceOfOperationsJob::Operation : public boost::noncopyable
  {
  private:
    JobOperationValues            originalInputs_;
    JobOperationValues            workInputs_;
    std::auto_ptr<IJobOperation>  operation_;
    std::list<Operation*>         nextOperations_;
    size_t                        currentInput_;

  public:
    Operation(IJobOperation* operation) :
    operation_(operation),
    currentInput_(0)
    {
      if (operation == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    void AddOriginalInput(const JobOperationValue& value)
    {
      if (currentInput_ != 0)
      {
        // Cannot add input after processing has started
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        originalInputs_.Append(value.Clone());
      }
    }

    const JobOperationValues& GetOriginalInputs() const
    {
      return originalInputs_;
    }

    void Reset()
    {
      workInputs_.Clear();
      currentInput_ = 0;
    }

    void AddNextOperation(Operation& other)
    {
      if (currentInput_ != 0)
      {
        // Cannot add input after processing has started
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        nextOperations_.push_back(&other);
      }
    }

    bool IsDone() const
    {
      return currentInput_ >= originalInputs_.GetSize() + workInputs_.GetSize();
    }

    void Step()
    {
      if (IsDone())
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }

      const JobOperationValue* input;

      if (currentInput_ < originalInputs_.GetSize())
      {
        input = &originalInputs_.GetValue(currentInput_);
      }
      else
      {
        input = &workInputs_.GetValue(currentInput_ - originalInputs_.GetSize());
      }

      JobOperationValues outputs;
      operation_->Apply(outputs, *input);

      if (!nextOperations_.empty())
      {
        std::list<Operation*>::iterator first = nextOperations_.begin();
        outputs.Move((*first)->workInputs_);

        std::list<Operation*>::iterator current = first;
        ++current;

        while (current != nextOperations_.end())
        {
          (*first)->workInputs_.Copy((*current)->workInputs_);
          ++current;
        }
      }

      currentInput_ += 1;
    }
  };


  // Invoked from constructors
  void SequenceOfOperationsJob::Setup()
  {
    done_ = false;
    current_ = 0;
    trailingTimeout_ = boost::posix_time::milliseconds(1000); 
  }


  SequenceOfOperationsJob::~SequenceOfOperationsJob()
  {
    for (size_t i = 0; i < operations_.size(); i++)
    {
      if (operations_[i] != NULL)
      {
        delete operations_[i];
      }
    }
  }


  void SequenceOfOperationsJob::Register(IObserver& observer)
  {
    boost::mutex::scoped_lock lock(mutex_);
    observers_.push_back(&observer);
  }


  void SequenceOfOperationsJob::Lock::SetTrailingOperationTimeout(unsigned int timeout)
  {
    that_.trailingTimeout_ = boost::posix_time::milliseconds(timeout);
  }


  size_t SequenceOfOperationsJob::Lock::AddOperation(IJobOperation* operation)
  {
    if (IsDone())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    that_.operations_.push_back(new Operation(operation));
    that_.operationAdded_.notify_one();

    return that_.operations_.size() - 1;
  }


  void SequenceOfOperationsJob::Lock::AddInput(size_t index,
                                               const JobOperationValue& value)
  {
    if (IsDone())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (index >= that_.operations_.size() ||
             index < that_.current_)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      that_.operations_[index]->AddOriginalInput(value);
    }
  }
      

  void SequenceOfOperationsJob::Lock::Connect(size_t input,
                                              size_t output)
  {
    if (IsDone())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (input >= output ||
             input >= that_.operations_.size() ||
             output >= that_.operations_.size() ||
             input < that_.current_ ||
             output < that_.current_)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      Operation& a = *that_.operations_[input];
      Operation& b = *that_.operations_[output];
      a.AddNextOperation(b);
    }
  }


  JobStepResult SequenceOfOperationsJob::ExecuteStep()
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (current_ == operations_.size())
    {
      LOG(INFO) << "Executing the trailing timeout in the sequence of operations";
      operationAdded_.timed_wait(lock, trailingTimeout_);
            
      if (current_ == operations_.size())
      {
        // No operation was added during the trailing timeout: The
        // job is over
        LOG(INFO) << "The sequence of operations is over";
        done_ = true;

        for (std::list<IObserver*>::iterator it = observers_.begin(); 
             it != observers_.end(); ++it)
        {
          (*it)->SignalDone(*this);
        }

        return JobStepResult::Success();
      }
      else
      {
        LOG(INFO) << "New operation added to the sequence of operations";
      }
    }

    assert(current_ < operations_.size());

    while (current_ < operations_.size() &&
           operations_[current_]->IsDone())
    {
      current_++;
    }

    if (current_ < operations_.size())
    {
      operations_[current_]->Step();
    }

    return JobStepResult::Continue();
  }


  void SequenceOfOperationsJob::SignalResubmit()
  {
    boost::mutex::scoped_lock lock(mutex_);
      
    current_ = 0;
    done_ = false;

    for (size_t i = 0; i < operations_.size(); i++)
    {
      operations_[i]->Reset();
    }
  }


  float SequenceOfOperationsJob::GetProgress()
  {
    boost::mutex::scoped_lock lock(mutex_);
      
    return (static_cast<float>(current_) / 
            static_cast<float>(operations_.size() + 1));
  }


  void SequenceOfOperationsJob::GetPublicContent(Json::Value& value)
  {
    boost::mutex::scoped_lock lock(mutex_);

    value["CountOperations"] = static_cast<unsigned int>(operations_.size());
  }
}
