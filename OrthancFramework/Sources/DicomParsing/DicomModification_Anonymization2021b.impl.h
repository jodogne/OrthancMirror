// RelationshipsVisitor handles (0x0008, 0x1140)  /* X/Z/U* */   // Referenced Image Sequence
// RelationshipsVisitor handles (0x0008, 0x2112)  /* X/Z/U* */   // Source Image Sequence
// Tag (0x0008, 0x0018) is set in Apply()         /* U */        // SOP Instance UID
// Tag (0x0010, 0x0010) is set below (*)          /* Z */        // Patient's Name
// Tag (0x0010, 0x0020) is set below (*)          /* Z */        // Patient ID
// Tag (0x0020, 0x000d) is set in Apply()         /* U */        // Study Instance UID
// Tag (0x0020, 0x000e) is set in Apply()         /* U */        // Series Instance UID
clearings_.insert(DicomTag(0x0008, 0x0020));                     // Study Date
clearings_.insert(DicomTag(0x0008, 0x0023));  /* Z/D */          // Content Date
clearings_.insert(DicomTag(0x0008, 0x0030));                     // Study Time
clearings_.insert(DicomTag(0x0008, 0x0033));  /* Z/D */          // Content Time
clearings_.insert(DicomTag(0x0008, 0x0050));                     // Accession Number
clearings_.insert(DicomTag(0x0008, 0x0090));                     // Referring Physician's Name
clearings_.insert(DicomTag(0x0008, 0x009c));                     // Consulting Physician's Name
clearings_.insert(DicomTag(0x0010, 0x0030));                     // Patient's Birth Date
clearings_.insert(DicomTag(0x0010, 0x0040));                     // Patient's Sex
clearings_.insert(DicomTag(0x0012, 0x0010));  /* D */            // Clinical Trial Sponsor Name
clearings_.insert(DicomTag(0x0012, 0x0020));  /* D */            // Clinical Trial Protocol ID
clearings_.insert(DicomTag(0x0012, 0x0021));                     // Clinical Trial Protocol Name
clearings_.insert(DicomTag(0x0012, 0x0030));                     // Clinical Trial Site ID
clearings_.insert(DicomTag(0x0012, 0x0031));                     // Clinical Trial Site Name
clearings_.insert(DicomTag(0x0012, 0x0040));  /* D */            // Clinical Trial Subject ID
clearings_.insert(DicomTag(0x0012, 0x0042));  /* D */            // Clinical Trial Subject Reading ID
clearings_.insert(DicomTag(0x0012, 0x0050));                     // Clinical Trial Time Point ID
clearings_.insert(DicomTag(0x0012, 0x0060));                     // Clinical Trial Coordinating Center Name
clearings_.insert(DicomTag(0x0012, 0x0081));  /* D */            // Clinical Trial Protocol Ethics Committee Name
clearings_.insert(DicomTag(0x0018, 0x0010));  /* Z/D */          // Contrast/Bolus Agent
clearings_.insert(DicomTag(0x0018, 0x11bb));  /* D */            // Acquisition Field Of View Label
clearings_.insert(DicomTag(0x0018, 0x9367));  /* D */            // X-Ray Source ID
clearings_.insert(DicomTag(0x0018, 0x9369));  /* D */            // Source Start DateTime
clearings_.insert(DicomTag(0x0018, 0x936a));  /* D */            // Source End DateTime
clearings_.insert(DicomTag(0x0018, 0x9371));  /* D */            // X-Ray Detector ID
clearings_.insert(DicomTag(0x0020, 0x0010));                     // Study ID
clearings_.insert(DicomTag(0x0034, 0x0001));  /* D */            // Flow Identifier Sequence
clearings_.insert(DicomTag(0x0034, 0x0002));  /* D */            // Flow Identifier
clearings_.insert(DicomTag(0x0034, 0x0005));  /* D */            // Source Identifier
clearings_.insert(DicomTag(0x0034, 0x0007));  /* D */            // Frame Origin Timestamp
clearings_.insert(DicomTag(0x003a, 0x0314));  /* D */            // Impedance Measurement DateTime
clearings_.insert(DicomTag(0x0040, 0x0512));  /* D */            // Container Identifier
clearings_.insert(DicomTag(0x0040, 0x0513));                     // Issuer of the Container Identifier Sequence
clearings_.insert(DicomTag(0x0040, 0x0551));  /* D */            // Specimen Identifier
clearings_.insert(DicomTag(0x0040, 0x0562));                     // Issuer of the Specimen Identifier Sequence
clearings_.insert(DicomTag(0x0040, 0x0610));                     // Specimen Preparation Sequence
clearings_.insert(DicomTag(0x0040, 0x1101));  /* D */            // Person Identification Code Sequence
clearings_.insert(DicomTag(0x0040, 0x2016));                     // Placer Order Number / Imaging Service Request
clearings_.insert(DicomTag(0x0040, 0x2017));                     // Filler Order Number / Imaging Service Request
clearings_.insert(DicomTag(0x0040, 0xa027));  /* D */            // Verifying Organization
clearings_.insert(DicomTag(0x0040, 0xa073));  /* D */            // Verifying Observer Sequence
clearings_.insert(DicomTag(0x0040, 0xa075));  /* D */            // Verifying Observer Name
clearings_.insert(DicomTag(0x0040, 0xa088));                     // Verifying Observer Identification Code Sequence
clearings_.insert(DicomTag(0x0040, 0xa123));  /* D */            // Person Name
clearings_.insert(DicomTag(0x0040, 0xa730));  /* D */            // Content Sequence
clearings_.insert(DicomTag(0x0070, 0x0001));  /* D */            // Graphic Annotation Sequence
clearings_.insert(DicomTag(0x0070, 0x0084));  /* Z/D */          // Content Creator's Name
clearings_.insert(DicomTag(0x3006, 0x0002));  /* D */            // Structure Set Label
clearings_.insert(DicomTag(0x3006, 0x0008));                     // Structure Set Date
clearings_.insert(DicomTag(0x3006, 0x0009));                     // Structure Set Time
clearings_.insert(DicomTag(0x3006, 0x0026));                     // ROI Name
clearings_.insert(DicomTag(0x3006, 0x00a6));                     // ROI Interpreter
clearings_.insert(DicomTag(0x300a, 0x0002));  /* D */            // RT Plan Label
clearings_.insert(DicomTag(0x300a, 0x0608));  /* D */            // Treatment Position Group Label
clearings_.insert(DicomTag(0x300a, 0x0611));                     // RT Accessory Holder Slot ID
clearings_.insert(DicomTag(0x300a, 0x0615));                     // RT Accessory Device Slot ID
clearings_.insert(DicomTag(0x300a, 0x0619));  /* D */            // Radiation Dose Identification Label
clearings_.insert(DicomTag(0x300a, 0x0623));  /* D */            // Radiation Dose In-Vivo Measurement Label
clearings_.insert(DicomTag(0x300a, 0x062a));  /* D */            // RT Tolerance Set Label
clearings_.insert(DicomTag(0x300a, 0x067c));  /* D */            // Radiation Generation Mode Label
clearings_.insert(DicomTag(0x300a, 0x067d));                     // Radiation Generation Mode Description
clearings_.insert(DicomTag(0x300a, 0x0734));  /* D */            // Treatment Tolerance Violation Description
clearings_.insert(DicomTag(0x300a, 0x0736));  /* D */            // Treatment Tolerance Violation DateTime
clearings_.insert(DicomTag(0x300a, 0x073a));  /* D */            // Recorded RT Control Point DateTime
clearings_.insert(DicomTag(0x300a, 0x0741));  /* D */            // Interlock DateTime
clearings_.insert(DicomTag(0x300a, 0x0742));  /* D */            // Interlock Description
clearings_.insert(DicomTag(0x300a, 0x0760));  /* D */            // Override DateTime
clearings_.insert(DicomTag(0x300a, 0x0783));  /* D */            // Interlock Origin Description
clearings_.insert(DicomTag(0x3010, 0x000f));                     // Conceptual Volume Combination Description
clearings_.insert(DicomTag(0x3010, 0x0017));                     // Conceptual Volume Description
clearings_.insert(DicomTag(0x3010, 0x001b));                     // Device Alternate Identifier
clearings_.insert(DicomTag(0x3010, 0x002d));  /* D */            // Device Label
clearings_.insert(DicomTag(0x3010, 0x0033));  /* D */            // User Content Label
clearings_.insert(DicomTag(0x3010, 0x0034));  /* D */            // User Content Long Label
clearings_.insert(DicomTag(0x3010, 0x0035));  /* D */            // Entity Label
clearings_.insert(DicomTag(0x3010, 0x0038));  /* D */            // Entity Long Label
clearings_.insert(DicomTag(0x3010, 0x0043));                     // Manufacturer's Device Identifier
clearings_.insert(DicomTag(0x3010, 0x0054));  /* D */            // RT Prescription Label
clearings_.insert(DicomTag(0x3010, 0x005a));                     // RT Physician Intent Narrative
clearings_.insert(DicomTag(0x3010, 0x005c));                     // Reason for Superseding
clearings_.insert(DicomTag(0x3010, 0x0077));  /* D */            // Treatment Site
clearings_.insert(DicomTag(0x3010, 0x007a));                     // Treatment Technique Notes
clearings_.insert(DicomTag(0x3010, 0x007b));                     // Prescription Notes
clearings_.insert(DicomTag(0x3010, 0x007f));                     // Fractionation Notes
clearings_.insert(DicomTag(0x3010, 0x0081));                     // Prescription Notes Sequence
removals_.insert(DicomTag(0x0000, 0x1000));                      // Affected SOP Instance UID
removals_.insert(DicomTag(0x0008, 0x0015));                      // Instance Coercion DateTime
removals_.insert(DicomTag(0x0008, 0x0021));   /* X/D */          // Series Date
removals_.insert(DicomTag(0x0008, 0x0022));   /* X/Z */          // Acquisition Date
removals_.insert(DicomTag(0x0008, 0x0024));                      // Overlay Date
removals_.insert(DicomTag(0x0008, 0x0025));                      // Curve Date
removals_.insert(DicomTag(0x0008, 0x002a));   /* X/Z/D */        // Acquisition DateTime
removals_.insert(DicomTag(0x0008, 0x0031));   /* X/D */          // Series Time
removals_.insert(DicomTag(0x0008, 0x0032));   /* X/Z */          // Acquisition Time
removals_.insert(DicomTag(0x0008, 0x0034));                      // Overlay Time
removals_.insert(DicomTag(0x0008, 0x0035));                      // Curve Time
removals_.insert(DicomTag(0x0008, 0x0080));   /* X/Z/D */        // Institution Name
removals_.insert(DicomTag(0x0008, 0x0081));                      // Institution Address
removals_.insert(DicomTag(0x0008, 0x0082));   /* X/Z/D */        // Institution Code Sequence
removals_.insert(DicomTag(0x0008, 0x0092));                      // Referring Physician's Address
removals_.insert(DicomTag(0x0008, 0x0094));                      // Referring Physician's Telephone Numbers
removals_.insert(DicomTag(0x0008, 0x0096));                      // Referring Physician Identification Sequence
removals_.insert(DicomTag(0x0008, 0x009d));                      // Consulting Physician Identification Sequence
removals_.insert(DicomTag(0x0008, 0x0201));                      // Timezone Offset From UTC
removals_.insert(DicomTag(0x0008, 0x1010));   /* X/Z/D */        // Station Name
removals_.insert(DicomTag(0x0008, 0x1030));                      // Study Description
removals_.insert(DicomTag(0x0008, 0x103e));                      // Series Description
removals_.insert(DicomTag(0x0008, 0x1040));                      // Institutional Department Name
removals_.insert(DicomTag(0x0008, 0x1041));                      // Institutional Department Type Code Sequence
removals_.insert(DicomTag(0x0008, 0x1048));                      // Physician(s) of Record
removals_.insert(DicomTag(0x0008, 0x1049));                      // Physician(s) of Record Identification Sequence
removals_.insert(DicomTag(0x0008, 0x1050));                      // Performing Physician's Name
removals_.insert(DicomTag(0x0008, 0x1052));                      // Performing Physician Identification Sequence
removals_.insert(DicomTag(0x0008, 0x1060));                      // Name of Physician(s) Reading Study
removals_.insert(DicomTag(0x0008, 0x1062));                      // Physician(s) Reading Study Identification Sequence
removals_.insert(DicomTag(0x0008, 0x1070));   /* X/Z/D */        // Operators' Name
removals_.insert(DicomTag(0x0008, 0x1072));   /* X/D */          // Operator Identification Sequence
removals_.insert(DicomTag(0x0008, 0x1080));                      // Admitting Diagnoses Description
removals_.insert(DicomTag(0x0008, 0x1084));                      // Admitting Diagnoses Code Sequence
removals_.insert(DicomTag(0x0008, 0x1110));   /* X/Z */          // Referenced Study Sequence
removals_.insert(DicomTag(0x0008, 0x1111));   /* X/Z/D */        // Referenced Performed Procedure Step Sequence
removals_.insert(DicomTag(0x0008, 0x1120));                      // Referenced Patient Sequence
removals_.insert(DicomTag(0x0008, 0x2111));                      // Derivation Description
removals_.insert(DicomTag(0x0008, 0x4000));                      // Identifying Comments
removals_.insert(DicomTag(0x0010, 0x0021));                      // Issuer of Patient ID
removals_.insert(DicomTag(0x0010, 0x0032));                      // Patient's Birth Time
removals_.insert(DicomTag(0x0010, 0x0050));                      // Patient's Insurance Plan Code Sequence
removals_.insert(DicomTag(0x0010, 0x0101));                      // Patient's Primary Language Code Sequence
removals_.insert(DicomTag(0x0010, 0x0102));                      // Patient's Primary Language Modifier Code Sequence
removals_.insert(DicomTag(0x0010, 0x1000));                      // Other Patient IDs
removals_.insert(DicomTag(0x0010, 0x1001));                      // Other Patient Names
removals_.insert(DicomTag(0x0010, 0x1002));                      // Other Patient IDs Sequence
removals_.insert(DicomTag(0x0010, 0x1005));                      // Patient's Birth Name
removals_.insert(DicomTag(0x0010, 0x1010));                      // Patient's Age
removals_.insert(DicomTag(0x0010, 0x1020));                      // Patient's Size
removals_.insert(DicomTag(0x0010, 0x1030));                      // Patient's Weight
removals_.insert(DicomTag(0x0010, 0x1040));                      // Patient's Address
removals_.insert(DicomTag(0x0010, 0x1050));                      // Insurance Plan Identification
removals_.insert(DicomTag(0x0010, 0x1060));                      // Patient's Mother's Birth Name
removals_.insert(DicomTag(0x0010, 0x1080));                      // Military Rank
removals_.insert(DicomTag(0x0010, 0x1081));                      // Branch of Service
removals_.insert(DicomTag(0x0010, 0x1090));                      // Medical Record Locator
removals_.insert(DicomTag(0x0010, 0x1100));                      // Referenced Patient Photo Sequence
removals_.insert(DicomTag(0x0010, 0x2000));                      // Medical Alerts
removals_.insert(DicomTag(0x0010, 0x2110));                      // Allergies
removals_.insert(DicomTag(0x0010, 0x2150));                      // Country of Residence
removals_.insert(DicomTag(0x0010, 0x2152));                      // Region of Residence
removals_.insert(DicomTag(0x0010, 0x2154));                      // Patient's Telephone Numbers
removals_.insert(DicomTag(0x0010, 0x2155));                      // Patient's Telecom Information
removals_.insert(DicomTag(0x0010, 0x2160));                      // Ethnic Group
removals_.insert(DicomTag(0x0010, 0x2180));                      // Occupation
removals_.insert(DicomTag(0x0010, 0x21a0));                      // Smoking Status
removals_.insert(DicomTag(0x0010, 0x21b0));                      // Additional Patient History
removals_.insert(DicomTag(0x0010, 0x21c0));                      // Pregnancy Status
removals_.insert(DicomTag(0x0010, 0x21d0));                      // Last Menstrual Date
removals_.insert(DicomTag(0x0010, 0x21f0));                      // Patient's Religious Preference
removals_.insert(DicomTag(0x0010, 0x2203));   /* X/Z */          // Patient's Sex Neutered
removals_.insert(DicomTag(0x0010, 0x2297));                      // Responsible Person
removals_.insert(DicomTag(0x0010, 0x2299));                      // Responsible Organization
removals_.insert(DicomTag(0x0010, 0x4000));                      // Patient Comments
removals_.insert(DicomTag(0x0012, 0x0051));                      // Clinical Trial Time Point Description
removals_.insert(DicomTag(0x0012, 0x0071));                      // Clinical Trial Series ID
removals_.insert(DicomTag(0x0012, 0x0072));                      // Clinical Trial Series Description
removals_.insert(DicomTag(0x0012, 0x0082));                      // Clinical Trial Protocol Ethics Committee Approval Number
removals_.insert(DicomTag(0x0016, 0x002b));                      // Maker Note
removals_.insert(DicomTag(0x0016, 0x004b));                      // Device Setting Description
removals_.insert(DicomTag(0x0016, 0x004d));                      // Camera Owner Name
removals_.insert(DicomTag(0x0016, 0x004e));                      // Lens Specification
removals_.insert(DicomTag(0x0016, 0x004f));                      // Lens Make
removals_.insert(DicomTag(0x0016, 0x0050));                      // Lens Model
removals_.insert(DicomTag(0x0016, 0x0051));                      // Lens Serial Number
removals_.insert(DicomTag(0x0016, 0x0070));                      // GPS Version ID
removals_.insert(DicomTag(0x0016, 0x0071));                      // GPS Latitude Ref
removals_.insert(DicomTag(0x0016, 0x0072));                      // GPS Latitude
removals_.insert(DicomTag(0x0016, 0x0073));                      // GPS Longitude Ref
removals_.insert(DicomTag(0x0016, 0x0074));                      // GPS Longitude
removals_.insert(DicomTag(0x0016, 0x0075));                      // GPS Altitude Ref
removals_.insert(DicomTag(0x0016, 0x0076));                      // GPS Altitude
removals_.insert(DicomTag(0x0016, 0x0077));                      // GPS Time Stamp
removals_.insert(DicomTag(0x0016, 0x0078));                      // GPS Satellites
removals_.insert(DicomTag(0x0016, 0x0079));                      // GPS Status
removals_.insert(DicomTag(0x0016, 0x007a));                      // GPS Measure Mode
removals_.insert(DicomTag(0x0016, 0x007b));                      // GPS DOP
removals_.insert(DicomTag(0x0016, 0x007c));                      // GPS Speed Ref
removals_.insert(DicomTag(0x0016, 0x007d));                      // GPS Speed
removals_.insert(DicomTag(0x0016, 0x007e));                      // GPS Track Ref
removals_.insert(DicomTag(0x0016, 0x007f));                      // GPS Track
removals_.insert(DicomTag(0x0016, 0x0080));                      // GPS Img Direction Ref
removals_.insert(DicomTag(0x0016, 0x0081));                      // GPS Img Direction
removals_.insert(DicomTag(0x0016, 0x0082));                      // GPS Map Datum
removals_.insert(DicomTag(0x0016, 0x0083));                      // GPS Dest Latitude Ref
removals_.insert(DicomTag(0x0016, 0x0084));                      // GPS Dest Latitude
removals_.insert(DicomTag(0x0016, 0x0085));                      // GPS Dest Longitude Ref
removals_.insert(DicomTag(0x0016, 0x0086));                      // GPS Dest Longitude
removals_.insert(DicomTag(0x0016, 0x0087));                      // GPS Dest Bearing Ref
removals_.insert(DicomTag(0x0016, 0x0088));                      // GPS Dest Bearing
removals_.insert(DicomTag(0x0016, 0x0089));                      // GPS Dest Distance Ref
removals_.insert(DicomTag(0x0016, 0x008a));                      // GPS Dest Distance
removals_.insert(DicomTag(0x0016, 0x008b));                      // GPS Processing Method
removals_.insert(DicomTag(0x0016, 0x008c));                      // GPS Area Information
removals_.insert(DicomTag(0x0016, 0x008d));                      // GPS Date Stamp
removals_.insert(DicomTag(0x0016, 0x008e));                      // GPS Differential
removals_.insert(DicomTag(0x0018, 0x1000));   /* X/Z/D */        // Device Serial Number
removals_.insert(DicomTag(0x0018, 0x1004));                      // Plate ID
removals_.insert(DicomTag(0x0018, 0x1005));                      // Generator ID
removals_.insert(DicomTag(0x0018, 0x1007));                      // Cassette ID
removals_.insert(DicomTag(0x0018, 0x1008));                      // Gantry ID
removals_.insert(DicomTag(0x0018, 0x1009));                      // Unique Device Identifier
removals_.insert(DicomTag(0x0018, 0x100a));                      // UDI Sequence
removals_.insert(DicomTag(0x0018, 0x1030));   /* X/D */          // Protocol Name
removals_.insert(DicomTag(0x0018, 0x1400));   /* X/D */          // Acquisition Device Processing Description
removals_.insert(DicomTag(0x0018, 0x4000));                      // Acquisition Comments
removals_.insert(DicomTag(0x0018, 0x5011));                      // Transducer Identification Sequence
removals_.insert(DicomTag(0x0018, 0x700a));   /* X/D */          // Detector ID
removals_.insert(DicomTag(0x0018, 0x9185));                      // Respiratory Motion Compensation Technique Description
removals_.insert(DicomTag(0x0018, 0x9373));                      // X-Ray Detector Label
removals_.insert(DicomTag(0x0018, 0x937b));                      // Multi-energy Acquisition Description
removals_.insert(DicomTag(0x0018, 0x937f));                      // Decomposition Description
removals_.insert(DicomTag(0x0018, 0x9424));                      // Acquisition Protocol Description
removals_.insert(DicomTag(0x0018, 0x9516));   /* X/D */          // Start Acquisition DateTime
removals_.insert(DicomTag(0x0018, 0x9517));   /* X/D */          // End Acquisition DateTime
removals_.insert(DicomTag(0x0018, 0x9937));                      // Requested Series Description
removals_.insert(DicomTag(0x0018, 0xa003));                      // Contribution Description
removals_.insert(DicomTag(0x0020, 0x3401));                      // Modifying Device ID
removals_.insert(DicomTag(0x0020, 0x3406));                      // Modified Image Description
removals_.insert(DicomTag(0x0020, 0x4000));                      // Image Comments
removals_.insert(DicomTag(0x0020, 0x9158));                      // Frame Comments
removals_.insert(DicomTag(0x0028, 0x4000));                      // Image Presentation Comments
removals_.insert(DicomTag(0x0032, 0x0012));                      // Study ID Issuer
removals_.insert(DicomTag(0x0032, 0x1020));                      // Scheduled Study Location
removals_.insert(DicomTag(0x0032, 0x1021));                      // Scheduled Study Location AE Title
removals_.insert(DicomTag(0x0032, 0x1030));                      // Reason for Study
removals_.insert(DicomTag(0x0032, 0x1032));                      // Requesting Physician
removals_.insert(DicomTag(0x0032, 0x1033));                      // Requesting Service
removals_.insert(DicomTag(0x0032, 0x1060));   /* X/Z */          // Requested Procedure Description
removals_.insert(DicomTag(0x0032, 0x1066));                      // Reason for Visit
removals_.insert(DicomTag(0x0032, 0x1067));                      // Reason for Visit Code Sequence
removals_.insert(DicomTag(0x0032, 0x1070));                      // Requested Contrast Agent
removals_.insert(DicomTag(0x0032, 0x4000));                      // Study Comments
removals_.insert(DicomTag(0x0038, 0x0004));                      // Referenced Patient Alias Sequence
removals_.insert(DicomTag(0x0038, 0x0010));                      // Admission ID
removals_.insert(DicomTag(0x0038, 0x0011));                      // Issuer of Admission ID
removals_.insert(DicomTag(0x0038, 0x0014));                      // Issuer of Admission ID Sequence
removals_.insert(DicomTag(0x0038, 0x001e));                      // Scheduled Patient Institution Residence
removals_.insert(DicomTag(0x0038, 0x0020));                      // Admitting Date
removals_.insert(DicomTag(0x0038, 0x0021));                      // Admitting Time
removals_.insert(DicomTag(0x0038, 0x0040));                      // Discharge Diagnosis Description
removals_.insert(DicomTag(0x0038, 0x0050));                      // Special Needs
removals_.insert(DicomTag(0x0038, 0x0060));                      // Service Episode ID
removals_.insert(DicomTag(0x0038, 0x0061));                      // Issuer of Service Episode ID
removals_.insert(DicomTag(0x0038, 0x0062));                      // Service Episode Description
removals_.insert(DicomTag(0x0038, 0x0064));                      // Issuer of Service Episode ID Sequence
removals_.insert(DicomTag(0x0038, 0x0300));                      // Current Patient Location
removals_.insert(DicomTag(0x0038, 0x0400));                      // Patient's Institution Residence
removals_.insert(DicomTag(0x0038, 0x0500));                      // Patient State
removals_.insert(DicomTag(0x0038, 0x4000));                      // Visit Comments
removals_.insert(DicomTag(0x0040, 0x0001));                      // Scheduled Station AE Title
removals_.insert(DicomTag(0x0040, 0x0002));                      // Scheduled Procedure Step Start Date
removals_.insert(DicomTag(0x0040, 0x0003));                      // Scheduled Procedure Step Start Time
removals_.insert(DicomTag(0x0040, 0x0004));                      // Scheduled Procedure Step End Date
removals_.insert(DicomTag(0x0040, 0x0005));                      // Scheduled Procedure Step End Time
removals_.insert(DicomTag(0x0040, 0x0006));                      // Scheduled Performing Physician's Name
removals_.insert(DicomTag(0x0040, 0x0007));                      // Scheduled Procedure Step Description
removals_.insert(DicomTag(0x0040, 0x0009));                      // Scheduled Procedure Step ID
removals_.insert(DicomTag(0x0040, 0x000b));                      // Scheduled Performing Physician Identification Sequence
removals_.insert(DicomTag(0x0040, 0x0010));                      // Scheduled Station Name
removals_.insert(DicomTag(0x0040, 0x0011));                      // Scheduled Procedure Step Location
removals_.insert(DicomTag(0x0040, 0x0012));                      // Pre-Medication
removals_.insert(DicomTag(0x0040, 0x0241));                      // Performed Station AE Title
removals_.insert(DicomTag(0x0040, 0x0242));                      // Performed Station Name
removals_.insert(DicomTag(0x0040, 0x0243));                      // Performed Location
removals_.insert(DicomTag(0x0040, 0x0244));                      // Performed Procedure Step Start Date
removals_.insert(DicomTag(0x0040, 0x0245));                      // Performed Procedure Step Start Time
removals_.insert(DicomTag(0x0040, 0x0250));                      // Performed Procedure Step End Date
removals_.insert(DicomTag(0x0040, 0x0251));                      // Performed Procedure Step End Time
removals_.insert(DicomTag(0x0040, 0x0253));                      // Performed Procedure Step ID
removals_.insert(DicomTag(0x0040, 0x0254));                      // Performed Procedure Step Description
removals_.insert(DicomTag(0x0040, 0x0275));                      // Request Attributes Sequence
removals_.insert(DicomTag(0x0040, 0x0280));                      // Comments on the Performed Procedure Step
removals_.insert(DicomTag(0x0040, 0x0310));                      // Comments on Radiation Dose
removals_.insert(DicomTag(0x0040, 0x050a));                      // Specimen Accession Number
removals_.insert(DicomTag(0x0040, 0x051a));                      // Container Description
removals_.insert(DicomTag(0x0040, 0x0555));   /* X/Z */          // Acquisition Context Sequence
removals_.insert(DicomTag(0x0040, 0x0600));                      // Specimen Short Description
removals_.insert(DicomTag(0x0040, 0x0602));                      // Specimen Detailed Description
removals_.insert(DicomTag(0x0040, 0x06fa));                      // Slide Identifier
removals_.insert(DicomTag(0x0040, 0x1001));                      // Requested Procedure ID
removals_.insert(DicomTag(0x0040, 0x1002));                      // Reason for the Requested Procedure
removals_.insert(DicomTag(0x0040, 0x1004));                      // Patient Transport Arrangements
removals_.insert(DicomTag(0x0040, 0x1005));                      // Requested Procedure Location
removals_.insert(DicomTag(0x0040, 0x100a));                      // Reason for Requested Procedure Code Sequence
removals_.insert(DicomTag(0x0040, 0x1010));                      // Names of Intended Recipients of Results
removals_.insert(DicomTag(0x0040, 0x1011));                      // Intended Recipients of Results Identification Sequence
removals_.insert(DicomTag(0x0040, 0x1102));                      // Person's Address
removals_.insert(DicomTag(0x0040, 0x1103));                      // Person's Telephone Numbers
removals_.insert(DicomTag(0x0040, 0x1104));                      // Person's Telecom Information
removals_.insert(DicomTag(0x0040, 0x1400));                      // Requested Procedure Comments
removals_.insert(DicomTag(0x0040, 0x2001));                      // Reason for the Imaging Service Request
removals_.insert(DicomTag(0x0040, 0x2008));                      // Order Entered By
removals_.insert(DicomTag(0x0040, 0x2009));                      // Order Enterer's Location
removals_.insert(DicomTag(0x0040, 0x2010));                      // Order Callback Phone Number
removals_.insert(DicomTag(0x0040, 0x2011));                      // Order Callback Telecom Information
removals_.insert(DicomTag(0x0040, 0x2400));                      // Imaging Service Request Comments
removals_.insert(DicomTag(0x0040, 0x3001));                      // Confidentiality Constraint on Patient Data Description
removals_.insert(DicomTag(0x0040, 0x4005));                      // Scheduled Procedure Step Start DateTime
removals_.insert(DicomTag(0x0040, 0x4008));                      // Scheduled Procedure Step Expiration DateTime
removals_.insert(DicomTag(0x0040, 0x4010));                      // Scheduled Procedure Step Modification DateTime
removals_.insert(DicomTag(0x0040, 0x4011));                      // Expected Completion DateTime
removals_.insert(DicomTag(0x0040, 0x4025));                      // Scheduled Station Name Code Sequence
removals_.insert(DicomTag(0x0040, 0x4027));                      // Scheduled Station Geographic Location Code Sequence
removals_.insert(DicomTag(0x0040, 0x4028));                      // Performed Station Name Code Sequence
removals_.insert(DicomTag(0x0040, 0x4030));                      // Performed Station Geographic Location Code Sequence
removals_.insert(DicomTag(0x0040, 0x4034));                      // Scheduled Human Performers Sequence
removals_.insert(DicomTag(0x0040, 0x4035));                      // Actual Human Performers Sequence
removals_.insert(DicomTag(0x0040, 0x4036));                      // Human Performer's Organization
removals_.insert(DicomTag(0x0040, 0x4037));                      // Human Performer's Name
removals_.insert(DicomTag(0x0040, 0x4050));                      // Performed Procedure Step Start DateTime
removals_.insert(DicomTag(0x0040, 0x4051));                      // Performed Procedure Step End DateTime
removals_.insert(DicomTag(0x0040, 0x4052));                      // Procedure Step Cancellation DateTime
removals_.insert(DicomTag(0x0040, 0xa078));                      // Author Observer Sequence
removals_.insert(DicomTag(0x0040, 0xa07a));                      // Participant Sequence
removals_.insert(DicomTag(0x0040, 0xa07c));                      // Custodial Organization Sequence
removals_.insert(DicomTag(0x0040, 0xa192));                      // Observation Date (Trial)
removals_.insert(DicomTag(0x0040, 0xa193));                      // Observation Time (Trial)
removals_.insert(DicomTag(0x0040, 0xa307));                      // Current Observer (Trial)
removals_.insert(DicomTag(0x0040, 0xa352));                      // Verbal Source (Trial)
removals_.insert(DicomTag(0x0040, 0xa353));                      // Address (Trial)
removals_.insert(DicomTag(0x0040, 0xa354));                      // Telephone Number (Trial)
removals_.insert(DicomTag(0x0040, 0xa358));                      // Verbal Source Identifier Code Sequence (Trial)
removals_.insert(DicomTag(0x0050, 0x001b));                      // Container Component ID
removals_.insert(DicomTag(0x0050, 0x0020));                      // Device Description
removals_.insert(DicomTag(0x0050, 0x0021));                      // Long Device Description
removals_.insert(DicomTag(0x0070, 0x0086));                      // Content Creator's Identification Code Sequence
removals_.insert(DicomTag(0x0088, 0x0200));                      // Icon Image Sequence
removals_.insert(DicomTag(0x0088, 0x0904));                      // Topic Title
removals_.insert(DicomTag(0x0088, 0x0906));                      // Topic Subject
removals_.insert(DicomTag(0x0088, 0x0910));                      // Topic Author
removals_.insert(DicomTag(0x0088, 0x0912));                      // Topic Keywords
removals_.insert(DicomTag(0x0400, 0x0402));                      // Referenced Digital Signature Sequence
removals_.insert(DicomTag(0x0400, 0x0403));                      // Referenced SOP Instance MAC Sequence
removals_.insert(DicomTag(0x0400, 0x0404));                      // MAC
removals_.insert(DicomTag(0x0400, 0x0550));                      // Modified Attributes Sequence
removals_.insert(DicomTag(0x0400, 0x0551));                      // Nonconforming Modified Attributes Sequence
removals_.insert(DicomTag(0x0400, 0x0552));                      // Nonconforming Data Element Value
removals_.insert(DicomTag(0x0400, 0x0561));                      // Original Attributes Sequence
removals_.insert(DicomTag(0x0400, 0x0600));                      // Instance Origin Status
removals_.insert(DicomTag(0x2030, 0x0020));                      // Text String
removals_.insert(DicomTag(0x2200, 0x0002));   /* X/Z */          // Label Text
removals_.insert(DicomTag(0x2200, 0x0005));   /* X/Z */          // Barcode Value
removals_.insert(DicomTag(0x3006, 0x0004));                      // Structure Set Name
removals_.insert(DicomTag(0x3006, 0x0006));                      // Structure Set Description
removals_.insert(DicomTag(0x3006, 0x0028));                      // ROI Description
removals_.insert(DicomTag(0x3006, 0x0038));                      // ROI Generation Description
removals_.insert(DicomTag(0x3006, 0x0085));                      // ROI Observation Label
removals_.insert(DicomTag(0x3006, 0x0088));                      // ROI Observation Description
removals_.insert(DicomTag(0x3008, 0x0054));   /* X/D */          // First Treatment Date
removals_.insert(DicomTag(0x3008, 0x0056));   /* X/D */          // Most Recent Treatment Date
removals_.insert(DicomTag(0x3008, 0x0105));   /* X/Z */          // Source Serial Number
removals_.insert(DicomTag(0x3008, 0x0250));   /* X/D */          // Treatment Date
removals_.insert(DicomTag(0x3008, 0x0251));   /* X/D */          // Treatment Time
removals_.insert(DicomTag(0x300a, 0x0003));                      // RT Plan Name
removals_.insert(DicomTag(0x300a, 0x0004));                      // RT Plan Description
removals_.insert(DicomTag(0x300a, 0x0006));   /* X/D */          // RT Plan Date
removals_.insert(DicomTag(0x300a, 0x0007));   /* X/D */          // RT Plan Time
removals_.insert(DicomTag(0x300a, 0x000e));                      // Prescription Description
removals_.insert(DicomTag(0x300a, 0x0016));                      // Dose Reference Description
removals_.insert(DicomTag(0x300a, 0x0072));                      // Fraction Group Description
removals_.insert(DicomTag(0x300a, 0x00b2));   /* X/Z */          // Treatment Machine Name
removals_.insert(DicomTag(0x300a, 0x00c3));                      // Beam Description
removals_.insert(DicomTag(0x300a, 0x00dd));                      // Bolus Description
removals_.insert(DicomTag(0x300a, 0x0196));                      // Fixation Device Description
removals_.insert(DicomTag(0x300a, 0x01a6));                      // Shielding Device Description
removals_.insert(DicomTag(0x300a, 0x01b2));                      // Setup Technique Description
removals_.insert(DicomTag(0x300a, 0x0216));                      // Source Manufacturer
removals_.insert(DicomTag(0x300a, 0x02eb));                      // Compensator Description
removals_.insert(DicomTag(0x300a, 0x0676));                      // Equipment Frame of Reference Description
removals_.insert(DicomTag(0x300c, 0x0113));                      // Reason for Omission Description
removals_.insert(DicomTag(0x300e, 0x0008));   /* X/Z */          // Reviewer Name
removals_.insert(DicomTag(0x3010, 0x0036));                      // Entity Name
removals_.insert(DicomTag(0x3010, 0x0037));                      // Entity Description
removals_.insert(DicomTag(0x3010, 0x004c));   /* X/D */          // Intended Phase Start Date
removals_.insert(DicomTag(0x3010, 0x004d));   /* X/D */          // Intended Phase End Date
removals_.insert(DicomTag(0x3010, 0x0056));   /* X/D */          // RT Treatment Approach Label
removals_.insert(DicomTag(0x3010, 0x0061));                      // Prior Treatment Dose Description
removals_.insert(DicomTag(0x4000, 0x0010));                      // Arbitrary
removals_.insert(DicomTag(0x4000, 0x4000));                      // Text Comments
removals_.insert(DicomTag(0x4008, 0x0042));                      // Results ID Issuer
removals_.insert(DicomTag(0x4008, 0x0102));                      // Interpretation Recorder
removals_.insert(DicomTag(0x4008, 0x010a));                      // Interpretation Transcriber
removals_.insert(DicomTag(0x4008, 0x010b));                      // Interpretation Text
removals_.insert(DicomTag(0x4008, 0x010c));                      // Interpretation Author
removals_.insert(DicomTag(0x4008, 0x0111));                      // Interpretation Approver Sequence
removals_.insert(DicomTag(0x4008, 0x0114));                      // Physician Approving Interpretation
removals_.insert(DicomTag(0x4008, 0x0115));                      // Interpretation Diagnosis Description
removals_.insert(DicomTag(0x4008, 0x0118));                      // Results Distribution List Sequence
removals_.insert(DicomTag(0x4008, 0x0119));                      // Distribution Name
removals_.insert(DicomTag(0x4008, 0x011a));                      // Distribution Address
removals_.insert(DicomTag(0x4008, 0x0202));                      // Interpretation ID Issuer
removals_.insert(DicomTag(0x4008, 0x0300));                      // Impressions
removals_.insert(DicomTag(0x4008, 0x4000));                      // Results Comments
removals_.insert(DicomTag(0xfffa, 0xfffa));                      // Digital Signatures Sequence
removals_.insert(DicomTag(0xfffc, 0xfffc));                      // Data Set Trailing Padding
removedRanges_.push_back(DicomTagRange(0x5000, 0x50ff, 0x0000, 0xffff));  // Curve Data
removedRanges_.push_back(DicomTagRange(0x6000, 0x60ff, 0x3000, 0x3000));  // Overlay Data
removedRanges_.push_back(DicomTagRange(0x6000, 0x60ff, 0x4000, 0x4000));  // Overlay Comments
uids_.insert(DicomTag(0x0000, 0x1001));                          // Requested SOP Instance UID
uids_.insert(DicomTag(0x0002, 0x0003));                          // Media Storage SOP Instance UID
uids_.insert(DicomTag(0x0004, 0x1511));                          // Referenced SOP Instance UID in File
uids_.insert(DicomTag(0x0008, 0x0014));                          // Instance Creator UID
uids_.insert(DicomTag(0x0008, 0x0058));                          // Failed SOP Instance UID List
uids_.insert(DicomTag(0x0008, 0x1155));                          // Referenced SOP Instance UID
uids_.insert(DicomTag(0x0008, 0x1195));                          // Transaction UID
uids_.insert(DicomTag(0x0008, 0x3010));                          // Irradiation Event UID
uids_.insert(DicomTag(0x0018, 0x1002));                          // Device UID
uids_.insert(DicomTag(0x0018, 0x100b));                          // Manufacturer's Device Class UID
uids_.insert(DicomTag(0x0018, 0x2042));                          // Target UID
uids_.insert(DicomTag(0x0020, 0x0052));                          // Frame of Reference UID
uids_.insert(DicomTag(0x0020, 0x0200));                          // Synchronization Frame of Reference UID
uids_.insert(DicomTag(0x0020, 0x9161));                          // Concatenation UID
uids_.insert(DicomTag(0x0020, 0x9164));                          // Dimension Organization UID
uids_.insert(DicomTag(0x0028, 0x1199));                          // Palette Color Lookup Table UID
uids_.insert(DicomTag(0x0028, 0x1214));                          // Large Palette Color Lookup Table UID
uids_.insert(DicomTag(0x003a, 0x0310));                          // Multiplex Group UID
uids_.insert(DicomTag(0x0040, 0x0554));                          // Specimen UID
uids_.insert(DicomTag(0x0040, 0x4023));                          // Referenced General Purpose Scheduled Procedure Step Transaction UID
uids_.insert(DicomTag(0x0040, 0xa124));                          // UID
uids_.insert(DicomTag(0x0040, 0xa171));                          // Observation UID
uids_.insert(DicomTag(0x0040, 0xa172));                          // Referenced Observation UID (Trial)
uids_.insert(DicomTag(0x0040, 0xa402));                          // Observation Subject UID (Trial)
uids_.insert(DicomTag(0x0040, 0xdb0c));                          // Template Extension Organization UID
uids_.insert(DicomTag(0x0040, 0xdb0d));                          // Template Extension Creator UID
uids_.insert(DicomTag(0x0062, 0x0021));                          // Tracking UID
uids_.insert(DicomTag(0x0070, 0x031a));                          // Fiducial UID
uids_.insert(DicomTag(0x0070, 0x1101));                          // Presentation Display Collection UID
uids_.insert(DicomTag(0x0070, 0x1102));                          // Presentation Sequence Collection UID
uids_.insert(DicomTag(0x0088, 0x0140));                          // Storage Media File-set UID
uids_.insert(DicomTag(0x0400, 0x0100));                          // Digital Signature UID
uids_.insert(DicomTag(0x3006, 0x0024));                          // Referenced Frame of Reference UID
uids_.insert(DicomTag(0x3006, 0x00c2));                          // Related Frame of Reference UID
uids_.insert(DicomTag(0x300a, 0x0013));                          // Dose Reference UID
uids_.insert(DicomTag(0x300a, 0x0083));                          // Referenced Dose Reference UID
uids_.insert(DicomTag(0x300a, 0x0609));                          // Treatment Position Group UID
uids_.insert(DicomTag(0x300a, 0x0650));                          // Patient Setup UID
uids_.insert(DicomTag(0x300a, 0x0700));                          // Treatment Session UID
uids_.insert(DicomTag(0x3010, 0x0006));                          // Conceptual Volume UID
uids_.insert(DicomTag(0x3010, 0x000b));                          // Referenced Conceptual Volume UID
uids_.insert(DicomTag(0x3010, 0x0013));                          // Constituent Conceptual Volume UID
uids_.insert(DicomTag(0x3010, 0x0015));                          // Source Conceptual Volume UID
uids_.insert(DicomTag(0x3010, 0x0031));                          // Referenced Fiducials UID
uids_.insert(DicomTag(0x3010, 0x003b));                          // RT Treatment Phase UID
uids_.insert(DicomTag(0x3010, 0x006e));                          // Dosimetric Objective UID
uids_.insert(DicomTag(0x3010, 0x006f));                          // Referenced Dosimetric Objective UID
