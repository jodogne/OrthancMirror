/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#include "../PrecompiledHeadersServer.h"
#include "HierarchicalMatcher.h"

#include "../../Core/Logging.h"
#include "../../Core/OrthancException.h"
#include "../../Core/DicomParsing/FromDcmtkBridge.h"
#include "../../Core/DicomParsing/ToDcmtkBridge.h"
#include "../OrthancConfiguration.h"

#include <dcmtk/dcmdata/dcfilefo.h>

namespace Orthanc
{
  HierarchicalMatcher::HierarchicalMatcher(ParsedDicomFile& query)
  {
    bool caseSensitivePN;

    {
      OrthancConfiguration::ReaderLock lock;
      caseSensitivePN = lock.GetConfiguration().GetBooleanParameter("CaseSensitivePN", false);
    }

    bool hasCodeExtensions;
    Encoding encoding = query.DetectEncoding(hasCodeExtensions);
    Setup(*query.GetDcmtkObject().getDataset(), caseSensitivePN, encoding, hasCodeExtensions);
  }


  HierarchicalMatcher::~HierarchicalMatcher()
  {
    for (Sequences::iterator it = sequences_.begin();
         it != sequences_.end(); ++it)
    {
      if (it->second != NULL)
      {
        delete it->second;
      }
    }
  }


  void HierarchicalMatcher::Setup(DcmItem& dataset,
                                  bool caseSensitivePN,
                                  Encoding encoding,
                                  bool hasCodeExtensions)
  {
    for (unsigned long i = 0; i < dataset.card(); i++)
    {
      DcmElement* element = dataset.getElement(i);
      if (element == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      DicomTag tag(FromDcmtkBridge::Convert(element->getTag()));
      if (tag == DICOM_TAG_SPECIFIC_CHARACTER_SET ||   // Ignore encoding
          tag.GetElement() == 0x0000)  // Ignore all "Group Length" tags
      {
        continue;
      }

      if (flatTags_.find(tag) != flatTags_.end() ||
          sequences_.find(tag) != sequences_.end())
      {
        // A constraint already exists on this tag
        throw OrthancException(ErrorCode_BadRequest);        
      }

      if (FromDcmtkBridge::LookupValueRepresentation(tag) == ValueRepresentation_Sequence)
      {
        DcmSequenceOfItems& sequence = dynamic_cast<DcmSequenceOfItems&>(*element);

        if (sequence.card() == 0 ||
            (sequence.card() == 1 && sequence.getItem(0)->card() == 0))
        {
          // Universal matching of a sequence
          sequences_[tag] = NULL;
        }
        else if (sequence.card() == 1)
        {
          sequences_[tag] = new HierarchicalMatcher(*sequence.getItem(0), caseSensitivePN, encoding, hasCodeExtensions);
        }
        else
        {
          throw OrthancException(ErrorCode_BadRequest);        
        }
      }
      else
      {
        flatTags_.insert(tag);

        std::set<DicomTag> ignoreTagLength;
        std::unique_ptr<DicomValue> value(FromDcmtkBridge::ConvertLeafElement
                                          (*element, DicomToJsonFlags_None, 
                                           0, encoding, hasCodeExtensions, ignoreTagLength));

        // WARNING: Also modify "DatabaseLookup::IsMatch()" if modifying this code
        if (value.get() == NULL ||
            value->IsNull())
        {
          // This is an universal constraint
        }
        else if (value->IsBinary())
        {
          if (!value->GetContent().empty())
          {
            LOG(WARNING) << "This C-Find modality worklist query contains a non-empty tag ("
                         << tag.Format() << ") with UN (unknown) value representation. "
                         << "It will be ignored.";
          }
        }
        else if (value->GetContent().empty())
        {
          // This is an universal matcher
        }
        else
        {
          flatConstraints_.AddDicomConstraint
            (tag, value->GetContent(), caseSensitivePN, true /* mandatory */);
        }
      }
    }
  }


  std::string HierarchicalMatcher::Format(const std::string& prefix) const
  {
    std::string s;

    std::set<DicomTag> tags;
    for (size_t i = 0; i < flatConstraints_.GetConstraintsCount(); i++)
    {
      const DicomTagConstraint& c = flatConstraints_.GetConstraint(i);

      s += c.Format() + "\n";
      tags.insert(c.GetTag());
    }
    
    // Loop over the universal constraints
    for (std::set<DicomTag>::const_iterator it = flatTags_.begin();
         it != flatTags_.end(); ++it)
    {
      if (tags.find(*it) == tags.end())
      {
        s += prefix + it->Format() + " == *\n";
      }
    }

    for (Sequences::const_iterator it = sequences_.begin();
         it != sequences_.end(); ++it)
    {
      s += prefix + it->first.Format() + " ";

      if (it->second == NULL)
      {
        s += "*\n";
      }
      else
      {
        s += "Sequence:\n" + it->second->Format(prefix + "  ");
      }
    }

    return s;
  }


  bool HierarchicalMatcher::Match(ParsedDicomFile& dicom) const
  {
    bool hasCodeExtensions;
    Encoding encoding = dicom.DetectEncoding(hasCodeExtensions);
    
    return MatchInternal(*dicom.GetDcmtkObject().getDataset(),
                         encoding, hasCodeExtensions);
  }


  bool HierarchicalMatcher::MatchInternal(DcmItem& item,
                                          Encoding encoding,
                                          bool hasCodeExtensions) const
  {
    if (!flatConstraints_.IsMatch(item, encoding, hasCodeExtensions))
    {
      return false;
    }
    
    for (Sequences::const_iterator it = sequences_.begin();
         it != sequences_.end(); ++it)
    {
      if (it->second != NULL)
      {
        DcmTagKey tag = ToDcmtkBridge::Convert(it->first);

        DcmSequenceOfItems* sequence = NULL;
        if (!item.findAndGetSequence(tag, sequence).good() ||
            sequence == NULL)
        {
          continue;
        }

        bool match = false;

        for (unsigned long i = 0; i < sequence->card(); i++)
        {
          if (it->second->MatchInternal(*sequence->getItem(i), encoding, hasCodeExtensions))
          {
            match = true;
            break;
          }
        }

        if (!match)
        {
          return false;
        }
      }
    }

    return true;
  }


  DcmDataset* HierarchicalMatcher::ExtractInternal(DcmItem& source,
                                                   Encoding encoding,
                                                   bool hasCodeExtensions) const
  {
    std::unique_ptr<DcmDataset> target(new DcmDataset);

    for (std::set<DicomTag>::const_iterator it = flatTags_.begin();
         it != flatTags_.end(); ++it)
    {
      DcmTagKey tag = ToDcmtkBridge::Convert(*it);
      
      DcmElement* element = NULL;
      if (source.findAndGetElement(tag, element).good() &&
          element != NULL)
      {
        if (it->IsPrivate())
        {
          throw OrthancException(ErrorCode_NotImplemented,
                                 "Not applicable to private tags: " + it->Format());
        }
        
        std::unique_ptr<DcmElement> cloned(FromDcmtkBridge::CreateElementForTag(*it, "" /* no private creator */));
        cloned->copyFrom(*element);
        target->insert(cloned.release());
      }
    }

    for (Sequences::const_iterator it = sequences_.begin();
         it != sequences_.end(); ++it)
    {
      DcmTagKey tag = ToDcmtkBridge::Convert(it->first);

      DcmSequenceOfItems* sequence = NULL;
      if (source.findAndGetSequence(tag, sequence).good() &&
          sequence != NULL)
      {
        std::unique_ptr<DcmSequenceOfItems> cloned(new DcmSequenceOfItems(tag));

        for (unsigned long i = 0; i < sequence->card(); i++)
        {
          if (it->second == NULL)
          {
            cloned->append(new DcmItem(*sequence->getItem(i)));
          }
          else if (it->second->MatchInternal(*sequence->getItem(i), encoding, hasCodeExtensions))  // TODO Might be optimized
          {
            // It is necessary to encapsulate the child dataset into a
            // "DcmItem" object before it can be included in a
            // sequence. Otherwise, "dciodvfy" reports an error "Bad
            // tag in sequence - Expecting Item or Sequence Delimiter."
            std::unique_ptr<DcmDataset> child(it->second->ExtractInternal(*sequence->getItem(i), encoding, hasCodeExtensions));
            cloned->append(new DcmItem(*child));
          }
        }

        target->insert(cloned.release());
      }
    }

    return target.release();
  }


  ParsedDicomFile* HierarchicalMatcher::Extract(ParsedDicomFile& dicom) const
  {
    bool hasCodeExtensions;
    Encoding encoding = dicom.DetectEncoding(hasCodeExtensions);
    
    std::unique_ptr<DcmDataset> dataset(ExtractInternal(*dicom.GetDcmtkObject().getDataset(),
                                                        encoding, hasCodeExtensions));

    std::unique_ptr<ParsedDicomFile> result(new ParsedDicomFile(*dataset));
    result->SetEncoding(encoding);

    return result.release();
  }
}
