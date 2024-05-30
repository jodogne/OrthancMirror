#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
# Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.


#
# This sample Python script illustrates how to DICOM-ize a micro-CT
# acquisition, then to upload it to Orthanc.
#
# This sample assumes that the slices of the micro-CT are encoded as
# TIFF files, that are all stored inside the same ZIP archive. Make
# sure to adapt the parameters of the DICOM-ization below.
#
# The following command-line will install the required libraries:
#
#  $ sudo pip3 install libtiff numpy pydicom requests
#

import datetime
import io
import os
import tempfile
import zipfile

import libtiff
import numpy
import pydicom
import pydicom._storage_sopclass_uids
import pydicom.datadict
import pydicom.tag
import requests
import requests.auth


########################################
##  Parameters for the DICOM-ization  ##
########################################

ZIP = os.path.join(os.getenv('HOME'), 'Downloads', 'SpyII_mb.zip')

URL = 'http://localhost:8042/'
USERNAME = 'orthanc'
PASSWORD = 'orthanc'

VOXEL_WIDTH = 1
VOXEL_HEIGHT = 1
VOXEL_DEPTH = 1

TAGS = {
    'PatientID' : 'Test',
    'PatientName' : 'Hello^World',
    'StudyDate' : datetime.datetime.now().strftime('%Y%m%d'),
    'StudyTime' : datetime.datetime.now().strftime('%H%M%S'),

    'AccessionNumber' : None,
    'AcquisitionNumber' : None,
    'KVP' : None,
    'Laterality' : None,
    'Manufacturer' : None,
    'PatientBirthDate' : '',
    'PatientPosition' : None,
    'PatientSex' : 'O',
    'PositionReferenceIndicator' : None,
    'ReferringPhysicianName' : None,
    'SeriesNumber' : 1,
    'StudyID' : 'Test',
    }



########################################
##  Application of the DICOM-ization  ##
########################################

# Add the DICOM unique identifiers
for tag in [ 'StudyInstanceUID',
             'SeriesInstanceUID',
             'FrameOfReferenceUID' ]:
    if not tag in TAGS:
        TAGS[tag] = pydicom.uid.generate_uid()


def CreateDicomDataset(tif, sliceIndex):
    image = tif.read_image().astype(numpy.uint16)

    meta = pydicom.Dataset()
    meta.MediaStorageSOPClassUID = pydicom._storage_sopclass_uids.CTImageStorage
    meta.MediaStorageSOPInstanceUID = pydicom.uid.generate_uid()
    meta.TransferSyntaxUID = pydicom.uid.ImplicitVRLittleEndian

    dataset = pydicom.Dataset()
    dataset.file_meta = meta

    dataset.is_little_endian = True
    dataset.is_implicit_VR = True
    dataset.SOPClassUID = meta.MediaStorageSOPClassUID
    dataset.SOPInstanceUID = meta.MediaStorageSOPInstanceUID
    dataset.Modality = 'CT'

    for (key, value) in TAGS.items():
        tag = pydicom.tag.Tag(key)
        vr = pydicom.datadict.dictionary_VR(tag)
        dataset.add_new(tag, vr, value)

    assert(image.dtype == numpy.uint16)
    dataset.BitsStored = 16
    dataset.BitsAllocated = 16
    dataset.SamplesPerPixel = 1
    dataset.HighBit = 15

    dataset.Rows = image.shape[0]
    dataset.Columns = image.shape[1]
    dataset.InstanceNumber = (sliceIndex + 1)
    dataset.ImagePositionPatient = r'0\0\%f' % (-float(sliceIndex) * VOXEL_DEPTH)
    dataset.ImageOrientationPatient = r'1\0\0\0\-1\0'
    dataset.SliceThickness = VOXEL_DEPTH
    dataset.ImageType = r'ORIGINAL\PRIMARY\AXIAL'
    dataset.RescaleIntercept = '0'
    dataset.RescaleSlope = '1'
    dataset.PixelSpacing = r'%f\%f' % (VOXEL_HEIGHT, VOXEL_WIDTH)
    dataset.PhotometricInterpretation = 'MONOCHROME2'
    dataset.PixelRepresentation = 1

    minValue = numpy.min(image)
    maxValue = numpy.max(image)
    dataset.WindowWidth = maxValue - minValue
    dataset.WindowCenter = (minValue + maxValue) / 2.0

    pydicom.dataset.validate_file_meta(dataset.file_meta, enforce_standard=True)
    dataset.PixelData = image.tobytes()

    return dataset


# Create a temporary file, as libtiff is not able to read from BytesIO()
with tempfile.NamedTemporaryFile() as tmp:
    sliceIndex = 0

    # Loop over the files in the ZIP archive, after having sorted them
    with zipfile.ZipFile(ZIP, 'r') as z:
        for path in sorted(z.namelist()):

            # Ignore folders in the ZIP archive
            info = z.getinfo(path)
            if info.is_dir():
                continue

            # Extract the current file from the ZIP archive, into the temporary file
            print('DICOM-izing: %s' % path)
            data = z.read(path)

            with open(tmp.name, 'wb') as f:
                f.write(data)

            # Try and decode the TIFF file
            try:
                tif = libtiff.TIFF.open(tmp.name)
            except:
                # Not a TIFF file, ignore
                continue

            # Create a DICOM dataset from the TIFF
            dataset = CreateDicomDataset(tif, sliceIndex)
            b = io.BytesIO()
            dataset.save_as(b, False)

            # Upload the DICOM dataset to Orthanc
            r = requests.post('%s/instances' % URL, b.getvalue(),
                              auth = requests.auth.HTTPBasicAuth(USERNAME, PASSWORD))
            r.raise_for_status()

            sliceIndex += 1
