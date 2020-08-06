/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "../OrthancFramework.h"

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

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if !defined(ORTHANC_ENABLE_DCMTK)
#  error The macro ORTHANC_ENABLE_DCMTK must be defined
#endif

#if ORTHANC_ENABLE_DCMTK != 1
#  error The macro ORTHANC_ENABLE_DCMTK must be set to 1 to use this file
#endif

#include "ITagVisitor.h"
#include "../DicomFormat/DicomInstanceHasher.h"
#include "../Images/ImageAccessor.h"
#include "../IDynamicObject.h"
#include "../Toolbox.h"

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
#  include "../RestApi/RestApiOutput.h"
#endif

#include <boost/shared_ptr.hpp>


class DcmDataset;
class DcmFileFormat;

namespace Orthanc
{
  class ORTHANC_PUBLIC ParsedDicomFile : public IDynamicObject
  {
  private:
    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    ParsedDicomFile(ParsedDicomFile& other,
                    bool keepSopInstanceUid);

    void CreateFromDicomMap(const DicomMap& source,
                            Encoding defaultEncoding,
                            bool permissive,
                            const std::string& defaultPrivateCreator,
                            const std::map<uint16_t, std::string>& privateCreators);

    void RemovePrivateTagsInternal(const std::set<DicomTag>* toKeep);

    void UpdateStorageUid(const DicomTag& tag,
                          const std::string& value,
                          bool decodeDataUriScheme);

    void InvalidateCache();

    bool EmbedContentInternal(const std::string& dataUriScheme);

    ParsedDicomFile(DcmFileFormat* dicom);  // This takes ownership (no clone)

  public:
    ParsedDicomFile(bool createIdentifiers);  // Create a minimal DICOM instance

    ParsedDicomFile(const DicomMap& map,
                    Encoding defaultEncoding,
                    bool permissive
                    );

    ParsedDicomFile(const DicomMap& map,
                    Encoding defaultEncoding,
                    bool permissive,
                    const std::string& defaultPrivateCreator,
                    const std::map<uint16_t, std::string>& privateCreators);

    ParsedDicomFile(const void* content,
                    size_t size);

    ParsedDicomFile(const std::string& content);

    ParsedDicomFile(DcmDataset& dicom);  // This clones the DCMTK object

    ParsedDicomFile(DcmFileFormat& dicom);  // This clones the DCMTK object

    static ParsedDicomFile* AcquireDcmtkObject(DcmFileFormat* dicom)  // No clone here
    {
      return new ParsedDicomFile(dicom);
    }

    DcmFileFormat& GetDcmtkObject() const;

    // The "ParsedDicomFile" object cannot be used after calling this method
    DcmFileFormat* ReleaseDcmtkObject();

    ParsedDicomFile* Clone(bool keepSopInstanceUid);

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
                 DicomReplaceMode mode,
                 const std::string& privateCreator /* used only for private tags */);

    void Replace(const DicomTag& tag,
                 const Json::Value& value,  // Assumed to be encoded with UTF-8
                 bool decodeDataUriScheme,
                 DicomReplaceMode mode,
                 const std::string& privateCreator /* used only for private tags */);

    void Insert(const DicomTag& tag,
                const Json::Value& value,   // Assumed to be encoded with UTF-8
                bool decodeDataUriScheme,
                const std::string& privateCreator /* used only for private tags */);

    // Cannot be applied to private tags
    void ReplacePlainString(const DicomTag& tag,
                            const std::string& utf8Value);

    // Cannot be applied to private tags
    void SetIfAbsent(const DicomTag& tag,
                     const std::string& utf8Value);

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

#if ORTHANC_SANDBOXED == 0
    void SaveToFile(const std::string& path);
#endif

    void EmbedContent(const std::string& dataUriScheme);

    void EmbedImage(const ImageAccessor& accessor);

    void EmbedImage(MimeType mime,
                    const std::string& content);

    Encoding DetectEncoding(bool& hasCodeExtensions) const;

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
                     MimeType& mime,   // OUT
                     unsigned int frameId);  // IN

    unsigned int GetFramesCount() const;

    static ParsedDicomFile* CreateFromJson(const Json::Value& value,
                                           DicomFromJsonFlags flags,
                                           const std::string& privateCreator);

    void ChangeEncoding(Encoding target);

    /**
     * The DICOM tags with a string whose size is greater than
     * "maxTagLength", are replaced by a DicomValue whose type is
     * "DicomValue_Null". If "maxTagLength" is zero, all the leaf tags
     * are included, independently of their length.
     **/
    void ExtractDicomSummary(DicomMap& target,
                             unsigned int maxTagLength) const;

    /**
     * This flavor can be used to bypass the "maxTagLength" limitation
     * on a selected set of DICOM tags.
     **/
    void ExtractDicomSummary(DicomMap& target,
                             unsigned int maxTagLength,
                             const std::set<DicomTag>& ignoreTagLength) const;

    bool LookupTransferSyntax(std::string& result);

    bool LookupPhotometricInterpretation(PhotometricInterpretation& result) const;

    void Apply(ITagVisitor& visitor);
  };
}
