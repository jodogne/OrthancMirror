#include "ArrayFilledByThreads.h"

#include "../MultiThreading/ThreadedCommandProcessor.h"
#include "../OrthancException.h"

namespace Orthanc
{
  class ArrayFilledByThreads::Command : public ICommand
  {
  private:
    ArrayFilledByThreads&  that_;
    size_t  index_;

  public:
    Command(ArrayFilledByThreads& that,
            size_t index) :
      that_(that),
      index_(index)
    {
    }

    virtual bool Execute()
    {
      std::auto_ptr<IDynamicObject> obj(that_.filler_.GetFillerItem(index_));
      if (obj.get() == NULL)
      {
        return false;
      }
      else
      {
        boost::mutex::scoped_lock lock(that_.mutex_);
        that_.array_[index_] = obj.release();
        return true;
      }
    }
  };

  void ArrayFilledByThreads::Clear()
  {
    for (size_t i = 0; i < array_.size(); i++)
    {
      if (array_[i])
        delete array_[i];
    }

    array_.clear();
    filled_ = false;
  }

  void ArrayFilledByThreads::Update()
  {
    if (!filled_)
    {
      array_.resize(filler_.GetFillerSize());

      Orthanc::ThreadedCommandProcessor processor(threadCount_);
      for (size_t i = 0; i < array_.size(); i++)
      {
        processor.Post(new Command(*this, i));
      }

      processor.Join();
      filled_ = true;
    }
  }


  ArrayFilledByThreads::ArrayFilledByThreads(IFiller& filler) : filler_(filler)
  {
    filled_ = false;
    threadCount_ = 4;
  }


  ArrayFilledByThreads::~ArrayFilledByThreads()
  {
    Clear();
  }

  
  void ArrayFilledByThreads::Reload()
  {
    Clear();
    Update();
  }


  void ArrayFilledByThreads::Invalidate()
  {
    Clear();
  }


  void ArrayFilledByThreads::SetThreadCount(unsigned int t)
  {
    if (t < 1)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    threadCount_ = t;
  }


  size_t ArrayFilledByThreads::GetSize()
  {
    Update();
    return array_.size();
  }


  IDynamicObject& ArrayFilledByThreads::GetItem(size_t index)
  {
    if (index >= GetSize())
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
    }

    return *array_[index];
  }
}
