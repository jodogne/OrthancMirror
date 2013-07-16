#pragma once

#include <boost/thread.hpp>

#include "../ICommand.h"

namespace Orthanc
{
  class ArrayFilledByThreads
  {
  public:
    class IFiller
    {
    public:
      virtual size_t GetFillerSize() = 0;

      virtual IDynamicObject* GetFillerItem(size_t index) = 0;
    };

  private:
    IFiller& filler_;
    boost::mutex  mutex_;
    std::vector<IDynamicObject*>  array_;
    bool filled_;
    unsigned int threadCount_;

    class Command;

    void Clear();

    void Update();

  public:
    ArrayFilledByThreads(IFiller& filler);

    ~ArrayFilledByThreads();
  
    void Reload();

    void Invalidate();

    void SetThreadCount(unsigned int t);

    unsigned int GetThreadCount() const
    {
      return threadCount_;
    }

    size_t GetSize();

    IDynamicObject& GetItem(size_t index);
  };
}

