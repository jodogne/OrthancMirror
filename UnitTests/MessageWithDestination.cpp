#include "../Core/IDynamicObject.h"

#include "../Core/OrthancException.h"

#include <memory>
#include <map>
#include <gtest/gtest.h>
#include <string>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace Orthanc
{
  class SharedMessageQueue
  {
  private:
    typedef std::list<IDynamicObject*>  Queue;

    unsigned int maxSize_;
    Queue queue_;
    boost::mutex mutex_;
    boost::condition_variable elementAvailable_;

  public:
    SharedMessageQueue(unsigned int maxSize = 0)
    {
      maxSize_ = maxSize;
    }

    ~SharedMessageQueue()
    {
      for (Queue::iterator it = queue_.begin(); it != queue_.end(); it++)
      {
        delete *it;
      }
    }

    void Enqueue(IDynamicObject* message)
    {
      boost::mutex::scoped_lock lock(mutex_);

      if (maxSize_ != 0 && queue_.size() > maxSize_)
      {
        // Too many elements in the queue: First remove the oldest
        delete queue_.front();
        queue_.pop_front();
      }

      queue_.push_back(message);
      elementAvailable_.notify_one();
    }

    IDynamicObject* Dequeue(int32_t timeout)
    {
      boost::mutex::scoped_lock lock(mutex_);

      // Wait for a message to arrive in the FIFO queue
      while (queue_.empty())
      {
        if (timeout == 0)
        {
          elementAvailable_.wait(lock);
        }
        else
        {
          bool success = elementAvailable_.timed_wait
            (lock, boost::posix_time::milliseconds(timeout));
          if (!success)
          {
            throw OrthancException(ErrorCode_Timeout);
          }
        }
      }

      std::auto_ptr<IDynamicObject> message(queue_.front());
      queue_.pop_front();

      return message.release();
    }

    IDynamicObject* Dequeue()
    {
      return Dequeue(0);
    }
  };


  /**
   * This class represents a message that is to be sent to some destination.
   **/
  class MessageToDispatch : public boost::noncopyable
  {
  private:
    IDynamicObject* message_;
    std::string destination_;

  public:
    /**
     * Create a new message with a destination.
     * \param message The content of the message (takes the ownership)
     * \param destination The destination of the message
     **/
    MessageToDispatch(IDynamicObject* message,
                      const char* destination)
    {
      message_ = message;
      destination_ = destination;
    }

    ~MessageToDispatch()
    {
      if (message_)
      {
        delete message_;
      }
    }
  };


  class IDestinationContext : public IDynamicObject
  {
  public:
    virtual void Handle(const IDynamicObject& message) = 0;
  };


  class IDestinationContextFactory : public IDynamicObject
  {
  public:
    virtual IDestinationContext* Construct(const char* destination) = 0;
  };


  class MessageDispatcher
  {
  private:
    typedef std::map<std::string, IDestinationContext*>  ActiveContexts;

    std::auto_ptr<IDestinationContextFactory> factory_;
    ActiveContexts activeContexts_;
    SharedMessageQueue queue_;

  public:
    MessageDispatcher(IDestinationContextFactory* factory)  // takes the ownership
    {
      factory_.reset(factory);
    }

    ~MessageDispatcher()
    {
      for (ActiveContexts::iterator it = activeContexts_.begin(); 
           it != activeContexts_.end(); it++)
      {
        delete it->second;
      }
    }
  };
}



#include "../Core/DicomFormat/DicomString.h"

using namespace Orthanc;

TEST(MessageToDispatch, A)
{
  MessageToDispatch a(new DicomString("coucou"), "pukkaj");
}

