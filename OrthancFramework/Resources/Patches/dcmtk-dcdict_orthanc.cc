// Function by the Orthanc project to load a dictionary from a memory
// buffer, which is necessary in sandboxed environments. This is an
// adapted version of DcmDataDictionary::loadDictionary().

#include <string>
#include <boost/noncopyable.hpp>

struct OrthancLinesIterator;

// This plain old C class is implemented in "../../Core/Toolbox.h"
OrthancLinesIterator* OrthancLinesIterator_Create(const std::string& content);

bool OrthancLinesIterator_GetLine(std::string& target,
                                  const OrthancLinesIterator* iterator);

void OrthancLinesIterator_Next(OrthancLinesIterator* iterator);

void OrthancLinesIterator_Free(OrthancLinesIterator* iterator);


class LinesIterator : public boost::noncopyable
{
private:
  OrthancLinesIterator* iterator_;
  
public:
  LinesIterator(const std::string& content) :
    iterator_(NULL)
  {
    iterator_ = OrthancLinesIterator_Create(content);
  }

  ~LinesIterator()
  {
    if (iterator_ != NULL)
    {
      OrthancLinesIterator_Free(iterator_);
      iterator_ = NULL;
    }
  }
  
  bool GetLine(std::string& target) const
  {
    if (iterator_ != NULL)
    {
      return OrthancLinesIterator_GetLine(target, iterator_);
    }
    else
    {
      return false;
    }
  }

  void Next()
  {
    if (iterator_ != NULL)
    {
      OrthancLinesIterator_Next(iterator_);
    }
  }
};



OFBool
DcmDataDictionary::loadFromMemory(const std::string& content, OFBool errorIfAbsent)
{
  int lineNumber = 0;
  char* lineFields[DCM_MAXDICTFIELDS + 1];
  int fieldsPresent;
  DcmDictEntry* e;
  int errorsEncountered = 0;
  OFBool errorOnThisLine = OFFalse;
  int i;

  DcmTagKey key, upperKey;
  DcmDictRangeRestriction groupRestriction = DcmDictRange_Unspecified;
  DcmDictRangeRestriction elementRestriction = DcmDictRange_Unspecified;
  DcmVR vr;
  char* vrName;
  char* tagName;
  char* privCreator;
  int vmMin, vmMax = 1;
  const char* standardVersion;

  LinesIterator iterator(content);

  std::string line;
  while (iterator.GetLine(line)) {
    iterator.Next();

    if (line.size() >= DCM_MAXDICTLINESIZE) {
      DCMDATA_ERROR("DcmDataDictionary: Too long line: " << line);
      continue;
    }

    lineNumber++;

    if (onlyWhitespace(line.c_str())) {
      continue; /* ignore this line */
    }
    if (isaCommentLine(line.c_str())) {
      continue; /* ignore this line */
    }

    errorOnThisLine = OFFalse;

    /* fields are tab separated */
    fieldsPresent = splitFields(line.c_str(), lineFields,
                                DCM_MAXDICTFIELDS,
                                DCM_DICT_FIELD_SEPARATOR_CHAR);

    /* initialize dict entry fields */
    vrName = NULL;
    tagName = NULL;
    privCreator = NULL;
    vmMin = vmMax = 1;
    standardVersion = "DICOM";

    switch (fieldsPresent) {
      case 0:
      case 1:
      case 2:
        DCMDATA_ERROR("DcmDataDictionary: "
                      << "too few fields (line " << lineNumber << ")");
        errorOnThisLine = OFTrue;
        break;
      default:
        DCMDATA_ERROR("DcmDataDictionary: "
                      << "too many fields (line " << lineNumber << "): ");
        errorOnThisLine = OFTrue;
        break;
      case 5:
        stripWhitespace(lineFields[4]);
        standardVersion = lineFields[4];
        /* drop through to next case label */
      case 4:
        /* the VM field is present */
        if (!parseVMField(lineFields[3], vmMin, vmMax)) {
          DCMDATA_ERROR("DcmDataDictionary: "
                        << "bad VM field (line " << lineNumber << "): " << lineFields[3]);
          errorOnThisLine = OFTrue;
        }
        /* drop through to next case label */
      case 3:
        if (!parseWholeTagField(lineFields[0], key, upperKey,
                                groupRestriction, elementRestriction, privCreator))
        {
          DCMDATA_ERROR("DcmDataDictionary: "
                        << "bad Tag field (line " << lineNumber << "): " << lineFields[0]);
          errorOnThisLine = OFTrue;
        } else {
          /* all is OK */
          vrName = lineFields[1];
          stripWhitespace(vrName);

          tagName = lineFields[2];
          stripWhitespace(tagName);
        }
    }

    if (!errorOnThisLine) {
      /* check the VR Field */
      vr.setVR(vrName);
      if (vr.getEVR() == EVR_UNKNOWN) {
        DCMDATA_ERROR("DcmDataDictionary: "
                      << "bad VR field (line " << lineNumber << "): " << vrName);
        errorOnThisLine = OFTrue;
      }
    }

    if (!errorOnThisLine) {
      e = new DcmDictEntry(
        key.getGroup(), key.getElement(),
        upperKey.getGroup(), upperKey.getElement(),
        vr, tagName, vmMin, vmMax, standardVersion, OFTrue,
        privCreator);

      e->setGroupRangeRestriction(groupRestriction);
      e->setElementRangeRestriction(elementRestriction);
      addEntry(e);
    }

    for (i = 0; i < fieldsPresent; i++) {
      free(lineFields[i]);
      lineFields[i] = NULL;
    }

    delete[] privCreator;

    if (errorOnThisLine) {
      errorsEncountered++;
    }
  }

  /* return OFFalse in case of errors and set internal state accordingly */
  if (errorsEncountered == 0) {
    dictionaryLoaded = OFTrue;
    return OFTrue;
  }
  else {
    dictionaryLoaded = OFFalse;
    return OFFalse;
  }
}
