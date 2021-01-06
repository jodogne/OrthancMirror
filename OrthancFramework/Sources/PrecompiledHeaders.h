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

#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#if ORTHANC_USE_PRECOMPILED_HEADERS == 1

#include "OrthancFramework.h"  // Must be the first one

//#include <boost/date_time/posix_time/posix_time.hpp>
//#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
//#include <boost/locale.hpp>
//#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <json/value.h>

#if ORTHANC_ENABLE_PUGIXML == 1
#  include <pugixml.hpp>
#endif

#include "Compatibility.h"
#include "Enumerations.h"
#include "Logging.h"
#include "OrthancException.h"

#if ORTHANC_ENABLE_DCMTK == 1
// Headers from DCMTK used in Orthanc headers 
#  include <dcmtk/dcmdata/dcdatset.h>
#  include <dcmtk/dcmdata/dcfilefo.h>
#  include <dcmtk/dcmdata/dcmetinf.h>
#  include <dcmtk/dcmdata/dcpixseq.h>
#endif

#if ORTHANC_ENABLE_DCMTK_NETWORKING == 1
// Headers from DCMTK used in Orthanc headers 
#  include <dcmtk/dcmnet/dimse.h>
#endif

#endif
