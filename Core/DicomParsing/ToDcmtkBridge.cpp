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


#include "../PrecompiledHeaders.h"
#include "ToDcmtkBridge.h"

#include <memory>
#include <dcmtk/dcmnet/diutil.h>

#include "../OrthancException.h"


namespace Orthanc
{
  DcmEVR ToDcmtkBridge::Convert(ValueRepresentation vr)
  {
    switch (vr)
    {
      case ValueRepresentation_ApplicationEntity:
        return EVR_AE;

      case ValueRepresentation_AgeString:
        return EVR_AS;

      case ValueRepresentation_AttributeTag:
        return EVR_AT;

      case ValueRepresentation_CodeString:
        return EVR_CS;

      case ValueRepresentation_Date:
        return EVR_DA;

      case ValueRepresentation_DecimalString:
        return EVR_DS;

      case ValueRepresentation_DateTime:
        return EVR_DT;

      case ValueRepresentation_FloatingPointSingle:
        return EVR_FL;

      case ValueRepresentation_FloatingPointDouble:
        return EVR_FD;

      case ValueRepresentation_IntegerString:
        return EVR_IS;

      case ValueRepresentation_LongString:
        return EVR_LO;

      case ValueRepresentation_LongText:
        return EVR_LT;

      case ValueRepresentation_OtherByte:
        return EVR_OB;

        // Not supported as of DCMTK 3.6.0
        /*case ValueRepresentation_OtherDouble:
          return EVR_OD;*/

      case ValueRepresentation_OtherFloat:
        return EVR_OF;

        // Not supported as of DCMTK 3.6.0
        /*case ValueRepresentation_OtherLong:
          return EVR_OL;*/

      case ValueRepresentation_OtherWord:
        return EVR_OW;

      case ValueRepresentation_PersonName:
        return EVR_PN;

      case ValueRepresentation_ShortString:
        return EVR_SH;

      case ValueRepresentation_SignedLong:
        return EVR_SL;

      case ValueRepresentation_Sequence:
        return EVR_SQ;

      case ValueRepresentation_SignedShort:
        return EVR_SS;

      case ValueRepresentation_ShortText:
        return EVR_ST;

      case ValueRepresentation_Time:
        return EVR_TM;

        // Not supported as of DCMTK 3.6.0
        /*case ValueRepresentation_UnlimitedCharacters:
          return EVR_UC;*/

      case ValueRepresentation_UniqueIdentifier:
        return EVR_UI;

      case ValueRepresentation_UnsignedLong:
        return EVR_UL;

      case ValueRepresentation_Unknown:
        return EVR_UN;

        // Not supported as of DCMTK 3.6.0
        /*case ValueRepresentation_UniversalResource:
          return EVR_UR;*/

      case ValueRepresentation_UnsignedShort:
        return EVR_US;

      case ValueRepresentation_UnlimitedText:
        return EVR_UT;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }
}
