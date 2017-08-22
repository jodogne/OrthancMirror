/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017 Osimis, Belgium
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


#include "PrecompiledHeaders.h"
#include "SystemToolbox.h"


#if defined(_WIN32)
#  include <windows.h>
#  include <process.h>   // For "_spawnvp()" and "_getpid()"
#else
#  include <unistd.h>    // For "execvp()"
#  include <sys/wait.h>  // For "waitpid()"
#endif


#if defined(__APPLE__) && defined(__MACH__)
#  include <mach-o/dyld.h> /* _NSGetExecutablePath */
#  include <limits.h>      /* PATH_MAX */
#endif


#if defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__)
#  include <limits.h>      /* PATH_MAX */
#  include <signal.h>
#  include <unistd.h>
#endif


#if defined(__OpenBSD__)
#  include <sys/sysctl.h>  // For "sysctl", "CTL_KERN" and "KERN_PROC_ARGS"
#endif


// Inclusions for UUID
// http://stackoverflow.com/a/1626302

extern "C"
{
#if defined(_WIN32)
#  include <rpc.h>
#else
#  include <uuid/uuid.h>
#endif
}


#include "Logging.h"
#include "OrthancException.h"
#include "Toolbox.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>


namespace Orthanc
{
  static bool finish_;
  static ServerBarrierEvent barrierEvent_;

#if defined(_WIN32)
  static BOOL WINAPI ConsoleControlHandler(DWORD dwCtrlType)
  {
    // http://msdn.microsoft.com/en-us/library/ms683242(v=vs.85).aspx
    finish_ = true;
    return true;
  }
#else
  static void SignalHandler(int signal)
  {
    if (signal == SIGHUP)
    {
      barrierEvent_ = ServerBarrierEvent_Reload;
    }

    finish_ = true;
  }
#endif


  static ServerBarrierEvent ServerBarrierInternal(const bool* stopFlag)
  {
#if defined(_WIN32)
    SetConsoleCtrlHandler(ConsoleControlHandler, true);
#else
    signal(SIGINT, SignalHandler);
    signal(SIGQUIT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGHUP, SignalHandler);
#endif
  
    // Active loop that awakens every 100ms
    finish_ = false;
    barrierEvent_ = ServerBarrierEvent_Stop;
    while (!(*stopFlag || finish_))
    {
      SystemToolbox::USleep(100 * 1000);
    }

#if defined(_WIN32)
    SetConsoleCtrlHandler(ConsoleControlHandler, false);
#else
    signal(SIGINT, NULL);
    signal(SIGQUIT, NULL);
    signal(SIGTERM, NULL);
    signal(SIGHUP, NULL);
#endif

    return barrierEvent_;
  }


  ServerBarrierEvent SystemToolbox::ServerBarrier(const bool& stopFlag)
  {
    return ServerBarrierInternal(&stopFlag);
  }


  ServerBarrierEvent SystemToolbox::ServerBarrier()
  {
    const bool stopFlag = false;
    return ServerBarrierInternal(&stopFlag);
  }


  void SystemToolbox::USleep(uint64_t microSeconds)
  {
#if defined(_WIN32)
    ::Sleep(static_cast<DWORD>(microSeconds / static_cast<uint64_t>(1000)));
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__native_client__)
    usleep(microSeconds);
#else
#error Support your platform here
#endif
  }


  static std::streamsize GetStreamSize(std::istream& f)
  {
    // http://www.cplusplus.com/reference/iostream/istream/tellg/
    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    return size;
  }


  void SystemToolbox::ReadFile(std::string& content,
                               const std::string& path) 
  {
    if (!IsRegularFile(path))
    {
      LOG(ERROR) << std::string("The path does not point to a regular file: ") << path;
      throw OrthancException(ErrorCode_RegularFileExpected);
    }

    boost::filesystem::ifstream f;
    f.open(path, std::ifstream::in | std::ifstream::binary);
    if (!f.good())
    {
      throw OrthancException(ErrorCode_InexistentFile);
    }

    std::streamsize size = GetStreamSize(f);
    content.resize(static_cast<size_t>(size));
    if (size != 0)
    {
      f.read(reinterpret_cast<char*>(&content[0]), size);
    }

    f.close();
  }


  bool SystemToolbox::ReadHeader(std::string& header,
                                 const std::string& path,
                                 size_t headerSize)
  {
    if (!IsRegularFile(path))
    {
      LOG(ERROR) << std::string("The path does not point to a regular file: ") << path;
      throw OrthancException(ErrorCode_RegularFileExpected);
    }

    boost::filesystem::ifstream f;
    f.open(path, std::ifstream::in | std::ifstream::binary);
    if (!f.good())
    {
      throw OrthancException(ErrorCode_InexistentFile);
    }

    bool full = true;

    {
      std::streamsize size = GetStreamSize(f);
      if (size <= 0)
      {
        headerSize = 0;
        full = false;
      }
      else if (static_cast<size_t>(size) < headerSize)
      {
        headerSize = static_cast<size_t>(size);  // Truncate to the size of the file
        full = false;
      }
    }

    header.resize(headerSize);
    if (headerSize != 0)
    {
      f.read(reinterpret_cast<char*>(&header[0]), headerSize);
    }

    f.close();

    return full;
  }


  void SystemToolbox::WriteFile(const void* content,
                                size_t size,
                                const std::string& path)
  {
    boost::filesystem::ofstream f;
    f.open(path, std::ofstream::out | std::ofstream::binary);
    if (!f.good())
    {
      throw OrthancException(ErrorCode_CannotWriteFile);
    }

    if (size != 0)
    {
      f.write(reinterpret_cast<const char*>(content), size);

      if (!f.good())
      {
        f.close();
        throw OrthancException(ErrorCode_FileStorageCannotWrite);
      }
    }

    f.close();
  }


  void SystemToolbox::WriteFile(const std::string& content,
                                const std::string& path)
  {
    WriteFile(content.size() > 0 ? content.c_str() : NULL,
              content.size(), path);
  }


  void SystemToolbox::RemoveFile(const std::string& path)
  {
    if (boost::filesystem::exists(path))
    {
      if (IsRegularFile(path))
      {
        boost::filesystem::remove(path);
      }
      else
      {
        throw OrthancException(ErrorCode_RegularFileExpected);
      }
    }
  }


  uint64_t SystemToolbox::GetFileSize(const std::string& path)
  {
    try
    {
      return static_cast<uint64_t>(boost::filesystem::file_size(path));
    }
    catch (boost::filesystem::filesystem_error&)
    {
      throw OrthancException(ErrorCode_InexistentFile);
    }
  }


  void SystemToolbox::MakeDirectory(const std::string& path)
  {
    if (boost::filesystem::exists(path))
    {
      if (!boost::filesystem::is_directory(path))
      {
        throw OrthancException(ErrorCode_DirectoryOverFile);
      }
    }
    else
    {
      if (!boost::filesystem::create_directories(path))
      {
        throw OrthancException(ErrorCode_MakeDirectory);
      }
    }
  }


  bool SystemToolbox::IsExistingFile(const std::string& path)
  {
    return boost::filesystem::exists(path);
  }


#if defined(_WIN32)
  static std::string GetPathToExecutableInternal()
  {
    // Yes, this is ugly, but there is no simple way to get the 
    // required buffer size, so we use a big constant
    std::vector<char> buffer(32768);
    /*int bytes =*/ GetModuleFileNameA(NULL, &buffer[0], static_cast<DWORD>(buffer.size() - 1));
    return std::string(&buffer[0]);
  }

#elif defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__)
  static std::string GetPathToExecutableInternal()
  {
    // NOTE: For FreeBSD, using KERN_PROC_PATHNAME might be a better alternative

    std::vector<char> buffer(PATH_MAX + 1);
    ssize_t bytes = readlink("/proc/self/exe", &buffer[0], buffer.size() - 1);
    if (bytes == 0)
    {
      throw OrthancException(ErrorCode_PathToExecutable);
    }

    return std::string(&buffer[0]);
  }

#elif defined(__APPLE__) && defined(__MACH__)
  static std::string GetPathToExecutableInternal()
  {
    char pathbuf[PATH_MAX + 1];
    unsigned int  bufsize = static_cast<int>(sizeof(pathbuf));

    _NSGetExecutablePath( pathbuf, &bufsize);

    return std::string(pathbuf);
  }

#elif defined(__OpenBSD__)
  static std::string GetPathToExecutableInternal()
  {
    // This is an adapted version of the patch proposed in issue #64
    // without an explicit call to "malloc()" to prevent memory leak
    // https://bitbucket.org/sjodogne/orthanc/issues/64/add-openbsd-support
    // https://stackoverflow.com/q/31494901/881731

    const int mib[4] = { CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV };

    size_t len;
    if (sysctl(mib, 4, NULL, &len, NULL, 0) == -1) 
    {
      throw OrthancException(ErrorCode_PathToExecutable);
    }

    std::string tmp;
    tmp.resize(len);

    char** buffer = reinterpret_cast<char**>(&tmp[0]);

    if (sysctl(mib, 4, buffer, &len, NULL, 0) == -1) 
    {
      throw OrthancException(ErrorCode_PathToExecutable);
    }
    else
    {
      return std::string(buffer[0]);
    }
  }

#else
#error Support your platform here
#endif


  std::string SystemToolbox::GetPathToExecutable()
  {
    boost::filesystem::path p(GetPathToExecutableInternal());
    return boost::filesystem::absolute(p).string();
  }


  std::string SystemToolbox::GetDirectoryOfExecutable()
  {
    boost::filesystem::path p(GetPathToExecutableInternal());
    return boost::filesystem::absolute(p.parent_path()).string();
  }


  void SystemToolbox::ExecuteSystemCommand(const std::string& command,
                                           const std::vector<std::string>& arguments)
  {
    // Convert the arguments as a C array
    std::vector<char*>  args(arguments.size() + 2);

    args.front() = const_cast<char*>(command.c_str());

    for (size_t i = 0; i < arguments.size(); i++)
    {
      args[i + 1] = const_cast<char*>(arguments[i].c_str());
    }

    args.back() = NULL;

    int status;

#if defined(_WIN32)
    // http://msdn.microsoft.com/en-us/library/275khfab.aspx
    status = static_cast<int>(_spawnvp(_P_OVERLAY, command.c_str(), &args[0]));

#else
    int pid = fork();

    if (pid == -1)
    {
      // Error in fork()
#if ORTHANC_ENABLE_LOGGING == 1
      LOG(ERROR) << "Cannot fork a child process";
#endif

      throw OrthancException(ErrorCode_SystemCommand);
    }
    else if (pid == 0)
    {
      // Execute the system command in the child process
      execvp(command.c_str(), &args[0]);

      // We should never get here
      _exit(1);
    }
    else
    {
      // Wait for the system command to exit
      waitpid(pid, &status, 0);
    }
#endif

    if (status != 0)
    {
#if ORTHANC_ENABLE_LOGGING == 1
      LOG(ERROR) << "System command failed with status code " << status;
#endif

      throw OrthancException(ErrorCode_SystemCommand);
    }
  }


  int SystemToolbox::GetProcessId()
  {
#if defined(_WIN32)
    return static_cast<int>(_getpid());
#else
    return static_cast<int>(getpid());
#endif
  }


  bool SystemToolbox::IsRegularFile(const std::string& path)
  {
    namespace fs = boost::filesystem;

    try
    {
      if (fs::exists(path))
      {
        fs::file_status status = fs::status(path);
        return (status.type() == boost::filesystem::regular_file ||
                status.type() == boost::filesystem::reparse_file);   // Fix BitBucket issue #11
      }
    }
    catch (fs::filesystem_error&)
    {
    }

    return false;
  }


  FILE* SystemToolbox::OpenFile(const std::string& path,
                                FileMode mode)
  {
#if defined(_WIN32)
    // TODO Deal with special characters by converting to the current locale
#endif

    const char* m;
    switch (mode)
    {
      case FileMode_ReadBinary:
        m = "rb";
        break;

      case FileMode_WriteBinary:
        m = "wb";
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    return fopen(path.c_str(), m);
  }


  std::string SystemToolbox::GenerateUuid()
  {
#ifdef WIN32
    UUID uuid;
    UuidCreate ( &uuid );

    unsigned char * str;
    UuidToStringA ( &uuid, &str );

    std::string s( ( char* ) str );

    RpcStringFreeA ( &str );
#else
    uuid_t uuid;
    uuid_generate_random ( uuid );
    char s[37];
    uuid_unparse ( uuid, s );
#endif
    return s;
  }


  std::string SystemToolbox::GetNowIsoString()
  {
    boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
    return boost::posix_time::to_iso_string(now);
  }

  
  void SystemToolbox::GetNowDicom(std::string& date,
                                  std::string& time)
  {
    boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
    tm tm = boost::posix_time::to_tm(now);

    char s[32];
    sprintf(s, "%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    date.assign(s);

    // TODO milliseconds
    sprintf(s, "%02d%02d%02d.%06d", tm.tm_hour, tm.tm_min, tm.tm_sec, 0);
    time.assign(s);
  }
}
