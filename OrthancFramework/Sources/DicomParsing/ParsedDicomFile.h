/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

    ParsedDicomFile(const ParsedDicomFile& other,
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

    // For internal use only, in order to provide const-correctness on
    // the top of DCMTK API
    DcmFileFormat& GetDcmtkObjectConst() const;

    explicit ParsedDicomFile(DcmFileFormat* dicom);  // This takes ownership (no clone)

#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
    // Alias for binary compatibility with Orthanc Framework 1.7.2 => don't use it anymore
    void DatasetToJson(Json::Value& target, 
                       DicomToJsonFormat format,
                       DicomToJsonFlags flags,
                       unsigned int maxStringLength);    
    DcmFileFormat& GetDcmtkObject() const;
    void Apply(ITagVisitor& visitor);
    ParsedDicomFile* Clone(bool keepSopInstanceUid);
    bool LookupTransferSyntax(std::string& result);
    bool LookupTransferSyntax(std::string& result) const;
    bool GetTagValue(std::string& value,
                     const DicomTag& tag);
#endif

  public:
    explicit ParsedDicomFile(bool createIdentifiers);  // Create a minimal DICOM instance

    ParsedDicomFile(const DicomMap& map,
                    Encoding defaultEncoding,
                    bool permissive);

    ParsedDicomFile(const DicomMap& map,
                    Encoding defaultEncoding,
                    bool permissive,
                    const std::string& defaultPrivateCreator,
                    const std::map<uint16_t, std::string>& privateCreators);

    ParsedDicomFile(const void* content,
                    size_t size);

    explicit ParsedDicomFile(const std::string& content);

    explicit ParsedDicomFile(DcmDataset& dicom);  // This clones the DCMTK object

    explicit ParsedDicomFile(DcmFileFormat& dicom);  // This clones the DCMTK object

    static ParsedDicomFile* AcquireDcmtkObject(DcmFileFormat* dicom);

    DcmFileFormat& GetDcmtkObject();

    // The "ParsedDicomFile" object cannot be used after calling this method
    DcmFileFormat* ReleaseDcmtkObject();

    ParsedDicomFile* Clone(bool keepSopInstanceUid) const;

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
    void SendPathValue(RestApiOutput& output,
                       const UriComponents& uri) const;

    void Answer(RestApiOutput& output) const;
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

    void RemovePrivateTags();

    void RemovePrivateTags(const std::set<DicomTag>& toKeep);

    // WARNING: This function handles the decoding of strings to UTF8
    bool GetTagValue(std::string& value,
                     const DicomTag& tag) const;

    DicomInstanceHasher GetHasher() const;

    // The "Save" methods are not tagged as "const", as the internal
    // representation might be changed after serialization
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
                       unsigned int maxStringLength) const;

    void DatasetToJson(Json::Value& target, 
                       DicomToJsonFormat format,
                       DicomToJsonFlags flags,
                       unsigned int maxStringLength,
                       const std::set<DicomTag>& ignoreTagLength) const;
      
    void HeaderToJson(Json::Value& target, 
                      DicomToJsonFormat format) const;

    bool HasTag(const DicomTag& tag) const;

    void EmbedPdf(const std::string& pdf);

    bool ExtractPdf(std::string& pdf) const;

    void GetRawFrame(std::string& target, // OUT
                     MimeType& mime,   // OUT
                     unsigned int frameId) const;  // IN

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

    bool LookupTransferSyntax(DicomTransferSyntax& result) const;

    bool LookupPhotometricInterpretation(PhotometricInterpretation& result) const;

    void Apply(ITagVisitor& visitor) const;

    // Decode the given frame, using the built-in DICOM decoder of Orthanc
    ImageAccessor* DecodeFrame(unsigned int frame) const;
  };
}
