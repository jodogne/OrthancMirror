#include "../Core/IDynamicObject.h"

#include <gtest/gtest.h>
#include <string>

namespace Orthanc
{
  /**
   * This class represents a message that is to be sent to some destination.
   **/
  class MessageWithDestination : public boost::noncopyable
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
    MessageWithDestination(IDynamicObject* message,
                           const char* destination)
    {
      message_ = message;
      destination_ = destination;
    }

    ~MessageWithDestination()
    {
      if (message_)
      {
        delete message_;
      }
    }
  };
}



#include "../Core/DicomFormat/DicomString.h"

using namespace Orthanc;

TEST(MessageWithDestination, A)
{
  MessageWithDestination a(new DicomString("coucou"), "pukkaj");
}

