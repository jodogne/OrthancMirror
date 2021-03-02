  /**
   * This is a manual implementation by Alain Mazy. Only kept for reference.
   * https://bitbucket.org/sjodogne/orthanc/commits/c6defdc4c611fca2ab528ba2c6937a742e0329a8?at=issue-46-anonymization
   **/
  
  void DicomModification::SetupAnonymization2011()
  {
    // This is Table E.1-1 from PS 3.15-2011 - DICOM Part 15: Security and System Management Profiles
    // https://raw.githubusercontent.com/jodogne/dicom-specification/master/2011/11_15pu.pdf
    
    removals_.insert(DicomTag(0x0000, 0x1000));  // Affected SOP Instance UID
    removals_.insert(DicomTag(0x0000, 0x1001));  // Requested SOP Instance UID
    removals_.insert(DicomTag(0x0002, 0x0003));  // Media Storage SOP Instance UID => TODO: replace with a non-zero length UID that is internally consistent within a set of Instances
    removals_.insert(DicomTag(0x0004, 0x1511));  // Referenced SOP Instance UID in File
    removals_.insert(DicomTag(0x0008, 0x0010));  // Irradiation Event UID
    removals_.insert(DicomTag(0x0008, 0x0014));  // Instance Creator UID
    //removals_.insert(DicomTag(0x0008, 0x0018));  // SOP Instance UID => set in Apply()
    clearings_.insert(DicomTag(0x0008, 0x0020)); // Study Date
    clearings_.insert(DicomTag(0x0008, 0x0021)); // Series Date
    clearings_.insert(DicomTag(0x0008, 0x0030)); // Study Time
    clearings_.insert(DicomTag(0x0008, 0x0031)); // Series Time
    removals_.insert(DicomTag(0x0008, 0x0022));  // Acquisition Date
    removals_.insert(DicomTag(0x0008, 0x0023));  // Content Date
    removals_.insert(DicomTag(0x0008, 0x0024));  // Overlay Date
    removals_.insert(DicomTag(0x0008, 0x0025));  // Curve Date
    removals_.insert(DicomTag(0x0008, 0x002a));  // Acquisition DateTime
    removals_.insert(DicomTag(0x0008, 0x0032));  // Acquisition Time
    removals_.insert(DicomTag(0x0008, 0x0033));  // Content Time
    removals_.insert(DicomTag(0x0008, 0x0034));  // Overlay Time
    removals_.insert(DicomTag(0x0008, 0x0035));  // Curve Time
    removals_.insert(DicomTag(0x0008, 0x0050));  // Accession Number
    removals_.insert(DicomTag(0x0008, 0x0058));  // Failed SOP Instance UID List
    removals_.insert(DicomTag(0x0008, 0x0080));  // Institution Name
    removals_.insert(DicomTag(0x0008, 0x0081));  // Institution Address
    removals_.insert(DicomTag(0x0008, 0x0082));  // Institution Code Sequence
    removals_.insert(DicomTag(0x0008, 0x0090));  // Referring Physician's Name
    removals_.insert(DicomTag(0x0008, 0x0092));  // Referring Physician's Address 
    removals_.insert(DicomTag(0x0008, 0x0094));  // Referring Physician's Telephone Numbers 
    removals_.insert(DicomTag(0x0008, 0x0096));  // Referring Physician's Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x010d));  // Context Group Extension Creator UID
    removals_.insert(DicomTag(0x0008, 0x0201));  // Timezone Offset From UTC
    removals_.insert(DicomTag(0x0008, 0x0300));  // Current Patient Location
    removals_.insert(DicomTag(0x0008, 0x1010));  // Station Name
    removals_.insert(DicomTag(0x0008, 0x1030));  // Study Description 
    removals_.insert(DicomTag(0x0008, 0x103e));  // Series Description 
    removals_.insert(DicomTag(0x0008, 0x1040));  // Institutional Department Name 
    removals_.insert(DicomTag(0x0008, 0x1048));  // Physician(s) of Record 
    removals_.insert(DicomTag(0x0008, 0x1049));  // Physician(s) of Record Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x1050));  // Performing Physicians' Name
    removals_.insert(DicomTag(0x0008, 0x1052));  // Performing Physicians Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x1060));  // Name of Physician(s) Reading Study
    removals_.insert(DicomTag(0x0008, 0x1062));  // Physician Reading Study Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x1070));  // Operators' Name
    removals_.insert(DicomTag(0x0008, 0x1072));  // Operators' Identification Sequence
    removals_.insert(DicomTag(0x0008, 0x1080));  // Admitting Diagnoses Description
    removals_.insert(DicomTag(0x0008, 0x1084));  // Admitting Diagnoses Code Sequence
    removals_.insert(DicomTag(0x0008, 0x1110));  // Referenced Study Sequence
    removals_.insert(DicomTag(0x0008, 0x1111));  // Referenced Performed Procedure Step Sequence
    removals_.insert(DicomTag(0x0008, 0x1120));  // Referenced Patient Sequence
    removals_.insert(DicomTag(0x0008, 0x1140));  // Referenced Image Sequence
    removals_.insert(DicomTag(0x0008, 0x1155));  // Referenced SOP Instance UID
    removals_.insert(DicomTag(0x0008, 0x1195));  // Transaction UID
    removals_.insert(DicomTag(0x0008, 0x2111));  // Derivation Description
    removals_.insert(DicomTag(0x0008, 0x2112));  // Source Image Sequence
    removals_.insert(DicomTag(0x0008, 0x4000));  // Identifying Comments
    removals_.insert(DicomTag(0x0008, 0x9123));  // Creator Version UID
    //removals_.insert(DicomTag(0x0010, 0x0010));  // Patient's Name => cf. below (*)
    //removals_.insert(DicomTag(0x0010, 0x0020));  // Patient ID => cf. below (*)
    removals_.insert(DicomTag(0x0010, 0x0030));  // Patient's Birth Date 
    removals_.insert(DicomTag(0x0010, 0x0032));  // Patient's Birth Time 
    clearings_.insert(DicomTag(0x0010, 0x0040)); // Patient's Sex
    removals_.insert(DicomTag(0x0010, 0x0050));  // Patient's Insurance Plan Code Sequence
    removals_.insert(DicomTag(0x0010, 0x0101));  // Patient's Primary Language Code Sequence
    removals_.insert(DicomTag(0x0010, 0x0102));  // Patient's Primary Language Modifier Code Sequence
    removals_.insert(DicomTag(0x0010, 0x1000));  // Other Patient Ids
    removals_.insert(DicomTag(0x0010, 0x1001));  // Other Patient Names 
    removals_.insert(DicomTag(0x0010, 0x1002));  // Other Patient IDs Sequence
    removals_.insert(DicomTag(0x0010, 0x1005));  // Patient's Birth Name
    removals_.insert(DicomTag(0x0010, 0x1010));  // Patient's Age
    removals_.insert(DicomTag(0x0010, 0x1020));  // Patient's Size 
    removals_.insert(DicomTag(0x0010, 0x1030));  // Patient's Weight 
    removals_.insert(DicomTag(0x0010, 0x1040));  // Patient's Address
    removals_.insert(DicomTag(0x0010, 0x1050));  // Insurance Plan Identification
    removals_.insert(DicomTag(0x0010, 0x1060));  // Patient's Mother's Birth Name
    removals_.insert(DicomTag(0x0010, 0x1080));  // Military Rank
    removals_.insert(DicomTag(0x0010, 0x1081));  // Branch of Service
    removals_.insert(DicomTag(0x0010, 0x1090));  // Medical Record Locator
    removals_.insert(DicomTag(0x0010, 0x2000));  // Medical Alerts
    removals_.insert(DicomTag(0x0010, 0x2110));  // Allergies
    removals_.insert(DicomTag(0x0010, 0x2150));  // Country of Residence
    removals_.insert(DicomTag(0x0010, 0x2152));  // Region of Residence
    removals_.insert(DicomTag(0x0010, 0x2154));  // PatientTelephoneNumbers
    removals_.insert(DicomTag(0x0010, 0x2160));  // Ethnic Group
    removals_.insert(DicomTag(0x0010, 0x2180));  // Occupation 
    removals_.insert(DicomTag(0x0010, 0x21a0));  // Smoking Status
    removals_.insert(DicomTag(0x0010, 0x21b0));  // Additional Patient's History
    removals_.insert(DicomTag(0x0010, 0x21c0));  // Pregnancy Status
    removals_.insert(DicomTag(0x0010, 0x21d0));  // Last Menstrual Date
    removals_.insert(DicomTag(0x0010, 0x21f0));  // Patient's Religious Preference
    removals_.insert(DicomTag(0x0010, 0x2203));  // Patient's Sex Neutered
    removals_.insert(DicomTag(0x0010, 0x2297));  // Responsible Person
    removals_.insert(DicomTag(0x0010, 0x2299));  // Responsible Organization
    removals_.insert(DicomTag(0x0010, 0x4000));  // Patient Comments
    removals_.insert(DicomTag(0x0018, 0x0010));  // Contrast Bolus Agent
    removals_.insert(DicomTag(0x0018, 0x1000));  // Device Serial Number
    removals_.insert(DicomTag(0x0018, 0x1002));  // Device UID
    removals_.insert(DicomTag(0x0018, 0x1004));  // Plate ID
    removals_.insert(DicomTag(0x0018, 0x1005));  // Generator ID
    removals_.insert(DicomTag(0x0018, 0x1007));  // Cassette ID
    removals_.insert(DicomTag(0x0018, 0x1008));  // Gantry ID
    removals_.insert(DicomTag(0x0018, 0x1030));  // Protocol Name
    removals_.insert(DicomTag(0x0018, 0x1400));  // Acquisition Device Processing Description
    removals_.insert(DicomTag(0x0018, 0x4000));  // Acquisition Comments
    removals_.insert(DicomTag(0x0018, 0x700a));  // Detector ID
    removals_.insert(DicomTag(0x0018, 0xa003));  // Contribution Description
    removals_.insert(DicomTag(0x0018, 0x9424));  // Acquisition Protocol Description
    //removals_.insert(DicomTag(0x0020, 0x000d));  // Study Instance UID => set in Apply()
    //removals_.insert(DicomTag(0x0020, 0x000e));  // Series Instance UID => set in Apply()
    removals_.insert(DicomTag(0x0020, 0x0010));  // Study ID
    removals_.insert(DicomTag(0x0020, 0x0052));  // Frame of Reference UID 
    removals_.insert(DicomTag(0x0020, 0x0200));  // Synchronization Frame of Reference UID 
    removals_.insert(DicomTag(0x0020, 0x3401));  // Modifying Device ID
    removals_.insert(DicomTag(0x0020, 0x3404));  // Modifying Device Manufacturer
    removals_.insert(DicomTag(0x0020, 0x3406));  // Modified Image Description
    removals_.insert(DicomTag(0x0020, 0x4000));  // Image Comments
    removals_.insert(DicomTag(0x0020, 0x9158));  // Frame Comments
    removals_.insert(DicomTag(0x0020, 0x9161));  // Concatenation UID
    removals_.insert(DicomTag(0x0020, 0x9164));  // Dimension Organization UID
    //removals_.insert(DicomTag(0x0028, 0x1199));  // Palette Color Lookup Table UID => TODO: replace with a non-zero length UID that is internally consistent within a set of Instances
    //removals_.insert(DicomTag(0x0028, 0x1214));  // Large Palette Color Lookup Table UID => TODO: replace with a non-zero length UID that is internally consistent within a set of Instances
    removals_.insert(DicomTag(0x0028, 0x4000));  // Image Presentation Comments
    removals_.insert(DicomTag(0x0032, 0x0012));  // Study ID Issuer
    removals_.insert(DicomTag(0x0032, 0x1020));  // Scheduled Study Location
    removals_.insert(DicomTag(0x0032, 0x1021));  // Scheduled Study Location AE Title
    removals_.insert(DicomTag(0x0032, 0x1030));  // Reason for Study
    removals_.insert(DicomTag(0x0032, 0x1032));  // Requesting Physician
    removals_.insert(DicomTag(0x0032, 0x1033));  // Requesting Service
    removals_.insert(DicomTag(0x0032, 0x1060));  // Requesting Procedure Description
    removals_.insert(DicomTag(0x0032, 0x1070));  // Requested Contrast Agent
    removals_.insert(DicomTag(0x0032, 0x4000));  // Study Comments
    removals_.insert(DicomTag(0x0038, 0x0010));  // Admission ID
    removals_.insert(DicomTag(0x0038, 0x0011));  // Issuer of Admission ID
    removals_.insert(DicomTag(0x0038, 0x001e));  // Scheduled Patient Institution Residence
    removals_.insert(DicomTag(0x0038, 0x0020));  // Admitting Date
    removals_.insert(DicomTag(0x0038, 0x0021));  // Admitting Time
    removals_.insert(DicomTag(0x0038, 0x0040));  // Discharge Diagnosis Description
    removals_.insert(DicomTag(0x0038, 0x0050));  // Special Needs
    removals_.insert(DicomTag(0x0038, 0x0060));  // Service Episode ID
    removals_.insert(DicomTag(0x0038, 0x0061));  // Issuer of Service Episode ID
    removals_.insert(DicomTag(0x0038, 0x0062));  // Service Episode Description
    removals_.insert(DicomTag(0x0038, 0x0400));  // Patient's Institution Residence
    removals_.insert(DicomTag(0x0038, 0x0500));  // Patient State
    removals_.insert(DicomTag(0x0038, 0x4000));  // Visit Comments
    removals_.insert(DicomTag(0x0038, 0x1234));  // Referenced Patient Alias Sequence
    removals_.insert(DicomTag(0x0040, 0x0001));  // Scheduled Station AE Title
    removals_.insert(DicomTag(0x0040, 0x0002));  // Scheduled Procedure Step Start Date
    removals_.insert(DicomTag(0x0040, 0x0003));  // Scheduled Procedure Step Start Time
    removals_.insert(DicomTag(0x0040, 0x0004));  // Scheduled Procedure Step End Date
    removals_.insert(DicomTag(0x0040, 0x0005));  // Scheduled Procedure Step End Time
    removals_.insert(DicomTag(0x0040, 0x0006));  // Scheduled Performing Physician Name
    removals_.insert(DicomTag(0x0040, 0x0007));  // Scheduled Procedure Step Description
    removals_.insert(DicomTag(0x0040, 0x000b));  // Scheduled Performing Physician Identification Sequence
    removals_.insert(DicomTag(0x0040, 0x0010));  // Scheduled Station Name
    removals_.insert(DicomTag(0x0040, 0x0011));  // Scheduled Procedure Step Location
    removals_.insert(DicomTag(0x0040, 0x0012));  // Pre-Medication
    removals_.insert(DicomTag(0x0040, 0x0241));  // Performed Station AE Title
    removals_.insert(DicomTag(0x0040, 0x0242));  // Performed Station Name
    removals_.insert(DicomTag(0x0040, 0x0243));  // Performed Location
    removals_.insert(DicomTag(0x0040, 0x0244));  // Performed Procedure Step Start Date
    removals_.insert(DicomTag(0x0040, 0x0245));  // Performed Procedure Step Start Time
    removals_.insert(DicomTag(0x0040, 0x0248));  // Performed Station Name Code Sequence
    removals_.insert(DicomTag(0x0040, 0x0253));  // Performed Procedure Step ID
    removals_.insert(DicomTag(0x0040, 0x0254));  // Performed Procedure Step Description
    removals_.insert(DicomTag(0x0040, 0x0275));  // Request Attributes Sequence
    removals_.insert(DicomTag(0x0040, 0x0280));  // Comments on Performed Procedure Step
    removals_.insert(DicomTag(0x0040, 0x0555));  // Acquisition Context Sequence
    removals_.insert(DicomTag(0x0040, 0x1001));  // Requested Procedure ID
    removals_.insert(DicomTag(0x0040, 0x1010));  // Names of Intended Recipient of Results
    removals_.insert(DicomTag(0x0040, 0x1011));  // Intended Recipient of Results Identification Sequence
    removals_.insert(DicomTag(0x0040, 0x1004));  // Patient Transport Arrangements
    removals_.insert(DicomTag(0x0040, 0x1005));  // Requested Procedure Location
    removals_.insert(DicomTag(0x0040, 0x1101));  // Person Identification Code Sequence
    removals_.insert(DicomTag(0x0040, 0x1102));  // Person Address
    removals_.insert(DicomTag(0x0040, 0x1103));  // Person Telephone Numbers
    removals_.insert(DicomTag(0x0040, 0x1400));  // Requested Procedure Comments
    removals_.insert(DicomTag(0x0040, 0x2001));  // Reason for Imaging Service Request
    removals_.insert(DicomTag(0x0040, 0x2008));  // Order Entered By
    removals_.insert(DicomTag(0x0040, 0x2009));  // Order Enterer Location
    removals_.insert(DicomTag(0x0040, 0x2010));  // Order Callback Phone Number
    removals_.insert(DicomTag(0x0040, 0x2016));  // Placer Order Number of Imaging Service Request
    removals_.insert(DicomTag(0x0040, 0x2017));  // Filler Order Number of Imaging Service Request
    removals_.insert(DicomTag(0x0040, 0x2400));  // Imaging Service Request Comments
    removals_.insert(DicomTag(0x0040, 0x4023));  // Referenced General Purpose Scheduled Procedure Step Transaction UID
    removals_.insert(DicomTag(0x0040, 0x4025));  // Scheduled Station Name Code Sequence
    removals_.insert(DicomTag(0x0040, 0x4027));  // Scheduled Station Geographic Location Code Sequence
    removals_.insert(DicomTag(0x0040, 0x4030));  // Performed Station Geographic Location Code Sequence
    removals_.insert(DicomTag(0x0040, 0x4034));  // Scheduled Human Performers Sequence
    removals_.insert(DicomTag(0x0040, 0x4035));  // Actual Human Performers Sequence
    removals_.insert(DicomTag(0x0040, 0x4036));  // Human Performers Organization
    removals_.insert(DicomTag(0x0040, 0x4037));  // Human Performers Name
    removals_.insert(DicomTag(0x0040, 0xa027));  // Verifying Organization
    removals_.insert(DicomTag(0x0040, 0xa073));  // Verifying Observer Sequence
    removals_.insert(DicomTag(0x0040, 0xa075));  // Verifying Observer Name
    removals_.insert(DicomTag(0x0040, 0xa078));  // Author Observer Sequence
    removals_.insert(DicomTag(0x0040, 0xa07a));  // Participant Sequence
    removals_.insert(DicomTag(0x0040, 0xa07c));  // Custodial Organization Sequence
    removals_.insert(DicomTag(0x0040, 0xa088));  // Verifying Observer Identification Code Sequence
    removals_.insert(DicomTag(0x0040, 0xa123));  // Person Name
    removals_.insert(DicomTag(0x0040, 0xa124));  // UID
    removals_.insert(DicomTag(0x0040, 0xa730));  // Content Sequence 
    removals_.insert(DicomTag(0x0040, 0x3001));  // Confidentiality Constraint on Patient Data Description
    removals_.insert(DicomTag(0x0040, 0xdb0c));  // Template Extension Organization UID
    removals_.insert(DicomTag(0x0040, 0xdb0d));  // Template Extension Creator UID
    removals_.insert(DicomTag(0x0070, 0x0001));  // Graphic Annotation Sequence
    removals_.insert(DicomTag(0x0070, 0x0084));  // Content Creator's Name
    removals_.insert(DicomTag(0x0070, 0x0086));  // Content Creator's Identification Code Sequence
    removals_.insert(DicomTag(0x0070, 0x031a));  // Fiducial UID
    removals_.insert(DicomTag(0x0088, 0x0140));  // Storage Media File-set UID
    removals_.insert(DicomTag(0x0088, 0x0200));  // Icon Image Sequence
    removals_.insert(DicomTag(0x0088, 0x0904));  // Topic Title
    removals_.insert(DicomTag(0x0088, 0x0906));  // Topic Subject
    removals_.insert(DicomTag(0x0088, 0x0910));  // Topic Author
    removals_.insert(DicomTag(0x0088, 0x0912));  // Topic Key Words
    removals_.insert(DicomTag(0x0400, 0x0100));  // Digital Signature UID
    removals_.insert(DicomTag(0x0400, 0x0402));  // Referenced Digital Signature Sequence
    removals_.insert(DicomTag(0x0400, 0x0403));  // Referenced SOP Instance MAC Sequence
    removals_.insert(DicomTag(0x0400, 0x0404));  // MAC
    removals_.insert(DicomTag(0x0400, 0x0550));  // Modified Attributes Sequence
    removals_.insert(DicomTag(0x0400, 0x0561));  // Original Attributes Sequence
    removals_.insert(DicomTag(0x2030, 0x0020));  // Text String
    removals_.insert(DicomTag(0x3006, 0x0024));  // Referenced Frame of Reference UID
    removals_.insert(DicomTag(0x3006, 0x00c2));  // Related Frame of Reference UID 
    removals_.insert(DicomTag(0x300a, 0x0013));  // Dose Reference UID
    removals_.insert(DicomTag(0x300e, 0x0008));  // Reviewer Name
    removals_.insert(DicomTag(0x4000, 0x0010));  // Arbitrary
    removals_.insert(DicomTag(0x4000, 0x4000));  // Text Comments
    removals_.insert(DicomTag(0x4008, 0x0042));  // Results ID Issuer
    removals_.insert(DicomTag(0x4008, 0x0102));  // Interpretation Recorder
    removals_.insert(DicomTag(0x4008, 0x010a));  // Interpretation Transcriber
    removals_.insert(DicomTag(0x4008, 0x010b));  // Interpretation Text
    removals_.insert(DicomTag(0x4008, 0x010c));  // Interpretation Author
    removals_.insert(DicomTag(0x4008, 0x0111));  // Interpretation Approver Sequence
    removals_.insert(DicomTag(0x4008, 0x0114));  // Physician Approving Interpretation
    removals_.insert(DicomTag(0x4008, 0x0115));  // Interpretation Diagnosis Description
    removals_.insert(DicomTag(0x4008, 0x0118));  // Results Distribution List Sequence
    removals_.insert(DicomTag(0x4008, 0x0119));  // Distribution Name
    removals_.insert(DicomTag(0x4008, 0x011a));  // Distribution Address
    removals_.insert(DicomTag(0x4008, 0x0202));  // Interpretation ID Issuer
    removals_.insert(DicomTag(0x4008, 0x0300));  // Impressions
    removals_.insert(DicomTag(0x4008, 0x4000));  // Results Comments
    removals_.insert(DicomTag(0xfffa, 0xfffa));  // Digital Signature Sequence
    removals_.insert(DicomTag(0xfffc, 0xfffc));  // Data Set Trailing Padding
    //removals_.insert(DicomTag(0x60xx, 0x4000));  // Overlay Comments => TODO
    //removals_.insert(DicomTag(0x60xx, 0x3000));  // Overlay Data => TODO

    // Set the DeidentificationMethod tag
    ReplaceInternal(DICOM_TAG_DEIDENTIFICATION_METHOD, ORTHANC_DEIDENTIFICATION_METHOD_2011);
  }
