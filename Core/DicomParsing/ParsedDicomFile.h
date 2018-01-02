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


#pragma once

#if !defined(ORTHANC_ENABLE_JPEG)
#  error Macro ORTHANC_ENABLE_JPEG must be defined to use this file
#endif

#if !defined(ORTHANC_ENABLE_PNG)
#  error Macro ORTHANC_ENABLE_PNG must be defined to use this file
#endif

#if !defined(ORTHANC_ENABLE_CIVETWEB)
#  error Macro ORTHANC_ENABLE_CIVETWEB must be defined to use this file
#endif

#if !defined(ORTHANC_ENABLE_MONGOOSE)
#  error Macro ORTHANC_ENABLE_MONGOOSE must be defined to use this file
#endif

#include "../DicomFormat/DicomInstanceHasher.h"
#include "../Images/ImageAccessor.h"
#include "../IDynamicObject.h"
#include "../Toolbox.h"

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
#  include "../RestApi/RestApiOutput.h"
#endif


class DcmDataset;
class DcmFileFormat;

namespace Orthanc
{
  class ParsedDicomFile : public IDynamicObject
  {
  private:
    struct PImpl;
    PImpl* pimpl_;

    ParsedDicomFile(ParsedDicomFile& other);

    void CreateFromDicomMap(const DicomMap& source,
                            Encoding defaultEncoding);

    void RemovePrivateTagsInternal(const std::set<DicomTag>* toKeep);

    void UpdateStorageUid(const DicomTag& tag,
                          const std::string& value,
                          bool decodeDataUriScheme);

    void InvalidateCache();

    bool EmbedContentInternal(const std::string& dataUriScheme);

  public:
    ParsedDicomFile(bool createIdentifiers);  // Create a minimal DICOM instance

    ParsedDicomFile(const DicomMap& map,
                    Encoding defaultEncoding);

    ParsedDicomFile(const DicomMap& map);

    ParsedDicomFile(const void* content,
                    size_t size);

    ParsedDicomFile(const std::string& content);

    ParsedDicomFile(DcmDataset& dicom);

    ParsedDicomFile(DcmFileFormat& dicom);

    ~ParsedDicomFile();

    DcmFileFormat& GetDcmtkObject() const;

    ParsedDicomFile* Clone();

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
    void SendPathValue(RestApiOutput& output,
                       const UriComponents& uri);

    void Answer(RestApiOutput& output);
#endif

    void Remove(const DicomTag& tag);

    // Replace the DICOM tag as a NULL/empty value (e.g. for anonymization)
    void Clear(const DicomTag& tag,
               bool onlyIfExists);

    void Replace(const DicomTag& tag,
                 const std::string& utf8Value,
                 bool decodeDataUriScheme,
                 DicomReplaceMode mode);

    void Replace(const DicomTag& tag,
                 const Json::Value& value,  // Assumed to be encoded with UTF-8
                 bool decodeDataUriScheme,
                 DicomReplaceMode mode);

    void Insert(const DicomTag& tag,
                const Json::Value& value,   // Assumed to be encoded with UTF-8
                bool decodeDataUriScheme);

    void ReplacePlainString(const DicomTag& tag,
                            const std::string& utf8Value)
    {
      Replace(tag, utf8Value, false, DicomReplaceMode_InsertIfAbsent);
    }

    void RemovePrivateTags()
    {
      RemovePrivateTagsInternal(NULL);
    }

    void RemovePrivateTags(const std::set<DicomTag>& toKeep)
    {
      RemovePrivateTagsInternal(&toKeep);
    }

    // WARNING: This function handles the decoding of strings to UTF8
    bool GetTagValue(std::string& value,
                     const DicomTag& tag);

    DicomInstanceHasher GetHasher();

    void SaveToMemoryBuffer(std::string& buffer);

    void SaveToFile(const std::string& path);

    void EmbedContent(const std::string& dataUriScheme);

    void EmbedImage(const ImageAccessor& accessor);

#if (ORTHANC_ENABLE_JPEG == 1 &&  \
     ORTHANC_ENABLE_PNG == 1)
    void EmbedImage(const std::string& mime,
                    const std::string& content);
#endif

    Encoding GetEncoding() const;

    // WARNING: This function only sets the encoding, it will not
    // convert the encoding of the tags. Use "ChangeEncoding()" if need be.
    void SetEncoding(Encoding encoding);

    void DatasetToJson(Json::Value& target, 
                       DicomToJsonFormat format,
                       DicomToJsonFlags flags,
                       unsigned int maxStringLength);

    void DatasetToJson(Json::Value& target, 
                       DicomToJsonFormat format,
                       DicomToJsonFlags flags,
                       unsigned int maxStringLength,
                       const std::set<DicomTag>& ignoreTagLength);
      
    // This version uses the default parameters for
    // FileContentType_DicomAsJson
    void DatasetToJson(Json::Value& target,
                       const std::set<DicomTag>& ignoreTagLength);

    void DatasetToJson(Json::Value& target);

    void HeaderToJson(Json::Value& target, 
                      DicomToJsonFormat format);

    bool HasTag(const DicomTag& tag) const;

    void EmbedPdf(const std::string& pdf);

    bool ExtractPdf(std::string& pdf);

    void GetRawFrame(std::string& target, // OUT
                     std::string& mime,   // OUT
                     unsigned int frameId);  // IN

    unsigned int GetFramesCount() const;

    static ParsedDicomFile* CreateFromJson(const Json::Value& value,
                                           DicomFromJsonFlags flags);

    void ChangeEncoding(Encoding target);

    void ExtractDicomSummary(DicomMap& target) const;

    bool LookupTransferSyntax(std::string& result);

    bool LookupPhotometricInterpretation(PhotometricInterpretation& result) const;
  };
}
