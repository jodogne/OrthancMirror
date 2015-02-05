/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


#pragma once

#include "../Core/HttpClient.h"

#include "Patient.h"

namespace OrthancClient
{
  /**
   * {summary}{Connection to an instance of %Orthanc.}
   * {description}{This class encapsulates a connection to a remote instance
   * of %Orthanc through its REST API.}
   **/  
  class LAAW_API OrthancConnection : 
    public boost::noncopyable,
    private Orthanc::ArrayFilledByThreads::IFiller
  {
  private:
    Orthanc::HttpClient client_;
    std::string orthancUrl_;
    Orthanc::ArrayFilledByThreads  patients_;
    Json::Value content_;

    void ReadPatients();

    virtual size_t GetFillerSize()
    {
      return content_.size();
    }

    virtual Orthanc::IDynamicObject* GetFillerItem(size_t index);

  public:
    /**
     * {summary}{Create a connection to an instance of %Orthanc.}
     * {param}{orthancUrl URL to which the REST API of %Orthanc is listening.}
     **/
    OrthancConnection(const char* orthancUrl);

    /**
     * {summary}{Create a connection to an instance of %Orthanc, with authentication.}
     * {param}{orthancUrl URL to which the REST API of %Orthanc is listening.}
     * {param}{username The username.}
     * {param}{password The password.}
     **/
    OrthancConnection(const char* orthancUrl,
                      const char* username, 
                      const char* password);

    virtual ~OrthancConnection()
    {
    }

    /**
     * {summary}{Returns the number of threads for this connection.}
     * {description}{Returns the number of simultaneous connections
     * that are used when downloading information from this instance
     * of %Orthanc.} 
     * {returns}{The number of threads.}
     **/
    uint32_t GetThreadCount() const
    {
      return patients_.GetThreadCount();
    }

    /**
     * {summary}{Sets the number of threads for this connection.}
     * {description}{Sets  the number of simultaneous connections
     * that are used when downloading information from this instance
     * of %Orthanc.} 
     * {param}{threadCount The number of threads.}
     **/
    void SetThreadCount(uint32_t threadCount)
    {
      patients_.SetThreadCount(threadCount);
    }

    /**
     * {summary}{Reload the list of the patients.}
     * {description}{This method will reload the list of the patients from the remote instance of %Orthanc. Pay attention to the fact that the patients that have been previously returned by GetPatient() will be invalidated.}
     **/
    void Reload()
    {
      ReadPatients();
      patients_.Invalidate();
    }

    LAAW_API_INTERNAL const Orthanc::HttpClient& GetHttpClient() const
    {
      return client_;
    }

    /**
     * {summary}{Returns the URL of this instance of %Orthanc.}
     * {description}{Returns the URL of the remote %Orthanc instance to which this object is connected.}
     * {returns}{The URL.}
     **/
    const char* GetOrthancUrl() const
    {
      return orthancUrl_.c_str();
    }

    /**
     * {summary}{Returns the number of patients.}
     * {description}{Returns the number of patients that are stored in the remote instance of %Orthanc.}
     * {returns}{The number of patients.}
     **/
    uint32_t GetPatientCount()
    {
      return patients_.GetSize();
    }

    /**
     * {summary}{Get some patient.}
     * {description}{This method will return an object that contains information about some patient. The patients are indexed by a number between 0 (inclusive) and the result of GetPatientCount() (exclusive).}
     * {param}{index The index of the patient of interest.}
     * {returns}{The patient.}
     **/
    Patient& GetPatient(uint32_t index);

    /**
     * {summary}{Delete some patient.}
     * {description}{Delete some patient from the remote instance of %Orthanc. Pay attention to the fact that the patients that have been previously returned by GetPatient() will be invalidated.}
     * {param}{index The index of the patient of interest.}
     * {returns}{The patient.}
     **/
    void DeletePatient(uint32_t index)
    {
      GetPatient(index).Delete();
      Reload();
    }

    /**
     * {summary}{Send a DICOM file.}
     * {description}{This method will store a DICOM file in the remote instance of %Orthanc. Pay attention to the fact that the patients that have been previously returned by GetPatient() will be invalidated.}
     * {param}{filename Path to the DICOM file}
     **/
    void StoreFile(const char* filename);

    /**
     * {summary}{Send a DICOM file that is contained inside a memory buffer.}
     * {description}{This method will store a DICOM file in the remote instance of %Orthanc. Pay attention to the fact that the patients that have been previously returned by GetPatient() will be invalidated.}
     * {param}{dicom The memory buffer containing the DICOM file.}
     * {param}{size The size of the DICOM file.}
     **/    
    void Store(const void* dicom, uint64_t size);
  };
}
