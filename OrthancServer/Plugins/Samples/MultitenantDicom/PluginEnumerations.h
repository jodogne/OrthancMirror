/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


enum LabelsConstraint
{
  LabelsConstraint_All,
  LabelsConstraint_Any,
  LabelsConstraint_None
};


#define KEY_AET                      "AET"
#define KEY_ALL                      "All"
#define KEY_ANY                      "Any"
#define KEY_LABELS                   "Labels"
#define KEY_LABELS_CONSTRAINT        "LabelsConstraint"
#define KEY_LABELS_STORE_LEVELS      "LabelsStoreLevels"
#define KEY_MAIN_DICOM_TAGS          "MainDicomTags"
#define KEY_MULTITENANT_DICOM        "MultitenantDicom"
#define KEY_NONE                     "None"
#define KEY_PATIENT_MAIN_DICOM_TAGS  "PatientMainDicomTags"
#define KEY_QUERY                    "Query"
#define KEY_SERVERS                  "Servers"
#define KEY_STRICT_AET_COMPARISON    "StrictAetComparison"
#define KEY_SYNCHRONOUS_C_MOVE       "SynchronousCMove"
