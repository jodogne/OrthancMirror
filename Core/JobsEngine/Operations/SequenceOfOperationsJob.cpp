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


#include "../../PrecompiledHeaders.h"
#include "SequenceOfOperationsJob.h"

#include "../../Logging.h"
#include "../../OrthancException.h"
#include "../../SerializationToolbox.h"
#include "../IJobUnserializer.h"

namespace Orthanc
{
  static const char* CURRENT = "Current";
  static const char* DESCRIPTION = "Description";
  static const char* DICOM_TIMEOUT = "DicomTimeout";
  static const char* NEXT_OPERATIONS = "Next";
  static const char* OPERATION = "Operation";
  static const char* OPERATIONS = "Operations";
  static const char* ORIGINAL_INPUTS = "OriginalInputs";
  static const char* TRAILING_TIMEOUT = "TrailingTimeout";
  static const char* TYPE = "Type";
  static const char* WORK_INPUTS = "WorkInputs";

  
  class SequenceOfOperationsJob::Operation : public boost::noncopyable
  {
  private:
    size_t                               index_;
    std::unique_ptr<IJobOperation>       operation_;
    std::unique_ptr<JobOperationValues>  originalInputs_;
    std::unique_ptr<JobOperationValues>  workInputs_;
    std::list<Operation*>                nextOperations_;
    size_t                               currentInput_;

  public:
    Operation(size_t index,
              IJobOperation* operation) :
      index_(index),
      operation_(operation),
      originalInputs_(new JobOperationValues),
      workInputs_(new JobOperationValues),
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
        originalInputs_->Append(value.Clone());
      }
    }

    const JobOperationValues& GetOriginalInputs() const
    {
      return *originalInputs_;
    }

    void Reset()
    {
      workInputs_->Clear();
      currentInput_ = 0;
    }

    void AddNextOperation(Operation& other,
                          bool unserializing)
    {
      if (other.index_ <= index_)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      if (!unserializing &&
          currentInput_ != 0)
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
      return currentInput_ >= originalInputs_->GetSize() + workInputs_->GetSize();
    }

    void Step(IDicomConnectionManager& connectionManager)
    {
      if (IsDone())
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }

      const JobOperationValue* input;

      if (currentInput_ < originalInputs_->GetSize())
      {
        input = &originalInputs_->GetValue(currentInput_);
      }
      else
      {
        input = &workInputs_->GetValue(currentInput_ - originalInputs_->GetSize());
      }

      JobOperationValues outputs;
      operation_->Apply(outputs, *input, connectionManager);

      if (!nextOperations_.empty())
      {
        std::list<Operation*>::iterator first = nextOperations_.begin();
        outputs.Move(*(*first)->workInputs_);

        std::list<Operation*>::iterator current = first;
        ++current;

        while (current != nextOperations_.end())
        {
          (*first)->workInputs_->Copy(*(*current)->workInputs_);
          ++current;
        }
      }

      currentInput_ += 1;
    }

    void Serialize(Json::Value& target) const
    {
      target = Json::objectValue;
      target[CURRENT] = static_cast<unsigned int>(currentInput_);
      operation_->Serialize(target[OPERATION]);
      originalInputs_->Serialize(target[ORIGINAL_INPUTS]);
      workInputs_->Serialize(target[WORK_INPUTS]);      

      Json::Value tmp = Json::arrayValue;
      for (std::list<Operation*>::const_iterator it = nextOperations_.begin();
           it != nextOperations_.end(); ++it)
      {
        tmp.append(static_cast<int>((*it)->index_));
      }

      target[NEXT_OPERATIONS] = tmp;
    }

    Operation(IJobUnserializer& unserializer,
              Json::Value::ArrayIndex index,
              const Json::Value& serialized) :
      index_(index)
    {
      if (serialized.type() != Json::objectValue ||
          !serialized.isMember(OPERATION) ||
          !serialized.isMember(ORIGINAL_INPUTS) ||
          !serialized.isMember(WORK_INPUTS))
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      currentInput_ = SerializationToolbox::ReadUnsignedInteger(serialized, CURRENT);
      operation_.reset(unserializer.UnserializeOperation(serialized[OPERATION]));
      originalInputs_.reset(JobOperationValues::Unserialize
                            (unserializer, serialized[ORIGINAL_INPUTS]));
      workInputs_.reset(JobOperationValues::Unserialize
                        (unserializer, serialized[WORK_INPUTS]));
    }
  };


  SequenceOfOperationsJob::SequenceOfOperationsJob() :
    done_(false),
    current_(0),
    trailingTimeout_(boost::posix_time::milliseconds(1000))
  {
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


  void SequenceOfOperationsJob::SetDescription(const std::string& description)
  {
    boost::mutex::scoped_lock lock(mutex_);
    description_ = description;
  }


  void SequenceOfOperationsJob::GetDescription(std::string& description)
  {
    boost::mutex::scoped_lock lock(mutex_);
    description = description_;    
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

  
  void SequenceOfOperationsJob::Lock::SetDicomAssociationTimeout(unsigned int timeout)
  {
    that_.connectionManager_.SetTimeout(timeout);
  }


  size_t SequenceOfOperationsJob::Lock::AddOperation(IJobOperation* operation)
  {
    if (IsDone())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    size_t index = that_.operations_.size();

    that_.operations_.push_back(new Operation(index, operation));
    that_.operationAdded_.notify_one();

    return index;
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
      a.AddNextOperation(b, false /* not unserializing */);
    }
  }


  JobStepResult SequenceOfOperationsJob::Step(const std::string& jobId)
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

        connectionManager_.Close();
        return JobStepResult::Success();
      }
      else
      {
        LOG(INFO) << "New operation were added to the sequence of operations";
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
      operations_[current_]->Step(connectionManager_);
    }

    connectionManager_.CheckTimeout();

    return JobStepResult::Continue();
  }


  void SequenceOfOperationsJob::Reset()
  {
    boost::mutex::scoped_lock lock(mutex_);
      
    current_ = 0;
    done_ = false;

    for (size_t i = 0; i < operations_.size(); i++)
    {
      operations_[i]->Reset();
    }
  }


  void SequenceOfOperationsJob::Stop(JobStopReason reason)
  {
    boost::mutex::scoped_lock lock(mutex_);
    connectionManager_.Close();
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
    value["Description"] = description_;
  }


  bool SequenceOfOperationsJob::Serialize(Json::Value& value)
  {
    boost::mutex::scoped_lock lock(mutex_);

    value = Json::objectValue;

    std::string jobType;
    GetJobType(jobType);
    value[TYPE] = jobType;
    
    value[DESCRIPTION] = description_;
    value[TRAILING_TIMEOUT] = static_cast<unsigned int>(trailingTimeout_.total_milliseconds());
    value[DICOM_TIMEOUT] = connectionManager_.GetTimeout();
    value[CURRENT] = static_cast<unsigned int>(current_);
    
    Json::Value tmp = Json::arrayValue;
    for (size_t i = 0; i < operations_.size(); i++)
    {
      Json::Value operation = Json::objectValue;
      operations_[i]->Serialize(operation);
      tmp.append(operation);
    }

    value[OPERATIONS] = tmp;

    return true;
  }


  SequenceOfOperationsJob::SequenceOfOperationsJob(IJobUnserializer& unserializer,
                                                   const Json::Value& serialized) :
    done_(false)
  {
    std::string jobType;
    GetJobType(jobType);
    
    if (SerializationToolbox::ReadString(serialized, TYPE) != jobType ||
        !serialized.isMember(OPERATIONS) ||
        serialized[OPERATIONS].type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    description_ = SerializationToolbox::ReadString(serialized, DESCRIPTION);
    trailingTimeout_ = boost::posix_time::milliseconds
      (SerializationToolbox::ReadUnsignedInteger(serialized, TRAILING_TIMEOUT));
    connectionManager_.SetTimeout
      (SerializationToolbox::ReadUnsignedInteger(serialized, DICOM_TIMEOUT));
    current_ = SerializationToolbox::ReadUnsignedInteger(serialized, CURRENT);

    const Json::Value& ops = serialized[OPERATIONS];

    // Unserialize the individual operations
    operations_.reserve(ops.size());
    for (Json::Value::ArrayIndex i = 0; i < ops.size(); i++)
    {
      operations_.push_back(new Operation(unserializer, i, ops[i]));
    }

    // Connect the next operations
    for (Json::Value::ArrayIndex i = 0; i < ops.size(); i++)
    {
      if (!ops[i].isMember(NEXT_OPERATIONS) ||
          ops[i][NEXT_OPERATIONS].type() != Json::arrayValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      const Json::Value& next = ops[i][NEXT_OPERATIONS];
      for (Json::Value::ArrayIndex j = 0; j < next.size(); j++)
      {
        if (next[j].type() != Json::intValue ||
            next[j].asInt() < 0 ||
            next[j].asUInt() >= operations_.size())
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }
        else
        {
          operations_[i]->AddNextOperation(*operations_[next[j].asUInt()], true);
        }
      }
    }  
  }
}
