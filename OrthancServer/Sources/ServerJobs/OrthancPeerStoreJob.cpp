/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeadersServer.h"
#include "OrthancPeerStoreJob.h"

#include "../../../OrthancFramework/Sources/Compression/GzipCompressor.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../ServerContext.h"

#include <dcmtk/dcmdata/dcfilefo.h>


namespace Orthanc
{
  bool OrthancPeerStoreJob::HandleInstance(const std::string& instance)
  {
    //boost::this_thread::sleep(boost::posix_time::milliseconds(500));

    if (client_.get() == NULL)
    {
      client_.reset(new HttpClient(peer_, "instances"));
      client_->SetMethod(HttpMethod_Post);

      if (compress_)
      {
        client_->AddHeader("Expect", "");
        client_->AddHeader("Content-Encoding", "gzip");
      }
    }
      
    LOG(INFO) << "Sending instance " << instance << " to peer \"" 
              << peer_.GetUrl() << "\"";

    // Lifetime of "body" must exceed the call to "client_->Apply()" because of "SetExternalBody()"
    std::string body;

    try
    {
      if (transcode_)
      {
        std::string dicom;
        context_.ReadDicom(dicom, instance);

        std::set<DicomTransferSyntax> syntaxes;
        syntaxes.insert(transferSyntax_);
        
        IDicomTranscoder::DicomImage source, transcoded;
        source.SetExternalBuffer(dicom);

        if (context_.Transcode(transcoded, source, syntaxes, true))
        {
          body.assign(reinterpret_cast<const char*>(transcoded.GetBufferData()),
                      transcoded.GetBufferSize());
        }
        else
        {
          body.swap(dicom);
        }
      }
      else
      {
        context_.ReadDicom(body, instance);
      }
    }
    catch (OrthancException& e)
    {
      LOG(WARNING) << "An instance was removed after the job was issued: " << instance;
      return false;
    }

    // Lifetime of "compressedBody" must exceed the call to "client_->Apply()" because of "SetExternalBody()"
    std::string compressedBody;

    if (compress_)
    {
      GzipCompressor compressor;
      compressor.SetCompressionLevel(9);  // Max compression level
      IBufferCompressor::Compress(compressedBody, compressor, body);

      client_->SetExternalBody(compressedBody);
      size_ += compressedBody.size();
    }
    else
    {
      client_->SetExternalBody(body);
      size_ += body.size();
    }

    std::string answer;
    if (client_->Apply(answer))
    {
      return true;
    }
    else
    {
      if (compress_)
      {
        LOG(ERROR) << "Cannot send DICOM over HTTP using \"gzip\" as "
                   << "\"Content-Encoding\": Make sure that the version "
                   << "of the remote Orthanc server is >= 1.6.0";
      }

      throw OrthancException(ErrorCode_NetworkProtocol);
    }
  }
    

  bool OrthancPeerStoreJob::HandleTrailingStep()
  {
    throw OrthancException(ErrorCode_InternalError);
  }


  void OrthancPeerStoreJob::SetPeer(const WebServiceParameters& peer)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      peer_ = peer;
    }
  }


  DicomTransferSyntax OrthancPeerStoreJob::GetTransferSyntax() const
  {
    if (transcode_)
    {
      return transferSyntax_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
  

  void OrthancPeerStoreJob::SetTranscode(DicomTransferSyntax syntax)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      transcode_ = true;
      transferSyntax_ = syntax;
    }    
  }
  

  void OrthancPeerStoreJob::SetTranscode(const std::string& transferSyntaxUid)
  {
    DicomTransferSyntax s;
    if (LookupTransferSyntax(s, transferSyntaxUid))
    {
      SetTranscode(s);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Unknown transfer syntax UID: " + transferSyntaxUid);
    }
  }


  void OrthancPeerStoreJob::ClearTranscode()
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      transcode_ = false;
    }
  }


  void OrthancPeerStoreJob::SetCompress(bool compress)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      compress_ = compress;
    }    
  }


  void OrthancPeerStoreJob::Stop(JobStopReason reason)   // For pausing jobs
  {
    client_.reset(NULL);
  }


  void OrthancPeerStoreJob::GetPublicContent(Json::Value& value)
  {
    SetOfInstancesJob::GetPublicContent(value);
    
    Json::Value v;
    peer_.Serialize(v, 
                    false /* allow simple format if possible */,
                    false /* don't include passwords */);
    value["Peer"] = v;
    value["Compress"] = compress_;
    
    if (transcode_)
    {
      value["Transcode"] = GetTransferSyntaxUid(transferSyntax_);
    }

    static const uint64_t MEGA_BYTES = 1024 * 1024;
    value["Size"] = boost::lexical_cast<std::string>(size_);
    value["SizeMB"] = static_cast<unsigned int>(size_ / MEGA_BYTES);
  }


  static const char* PEER = "Peer";
  static const char* TRANSCODE = "Transcode";
  static const char* COMPRESS = "Compress";
  static const char* SIZE = "Size";

  OrthancPeerStoreJob::OrthancPeerStoreJob(ServerContext& context,
                                           const Json::Value& serialized) :
    SetOfInstancesJob(serialized),
    context_(context)
  {
    assert(serialized.type() == Json::objectValue);
    peer_ = WebServiceParameters(serialized[PEER]);

    if (serialized.isMember(TRANSCODE))
    {
      SetTranscode(SerializationToolbox::ReadString(serialized, TRANSCODE));
    }
    else
    {
      transcode_ = false;
      transferSyntax_ = DicomTransferSyntax_LittleEndianExplicit;  // Dummy value
    }

    if (serialized.isMember(COMPRESS))
    {
      SetCompress(SerializationToolbox::ReadBoolean(serialized, COMPRESS));
    }
    else
    {
      compress_ = false;
    }

    if (serialized.isMember(SIZE))
    {
      size_ = boost::lexical_cast<uint64_t>(SerializationToolbox::ReadString(serialized, SIZE));
    }
    else
    {
      size_ = 0;
    }
  }


  bool OrthancPeerStoreJob::Serialize(Json::Value& target)
  {
    if (!SetOfInstancesJob::Serialize(target))
    {
      return false;
    }
    else
    {
      assert(target.type() == Json::objectValue);
      peer_.Serialize(target[PEER],
                      true /* force advanced format */,
                      true /* include passwords */);

      if (transcode_)
      {
        target[TRANSCODE] = GetTransferSyntaxUid(transferSyntax_);
      }

      target[COMPRESS] = compress_;
      target[SIZE] = boost::lexical_cast<std::string>(size_);
      
      return true;
    }
  }  
}
