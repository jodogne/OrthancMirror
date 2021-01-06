-- Orthanc - A Lightweight, RESTful DICOM Store
-- Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
-- Department, University Hospital of Liege, Belgium
-- Copyright (C) 2017-2021 Osimis S.A., Belgium
--
-- This program is free software: you can redistribute it and/or
-- modify it under the terms of the GNU General Public License as
-- published by the Free Software Foundation, either version 3 of the
-- License, or (at your option) any later version.
--
-- In addition, as a special exception, the copyright holders of this
-- program give permission to link the code of its release with the
-- OpenSSL project's "OpenSSL" library (or with modified versions of it
-- that use the same license as the "OpenSSL" library), and distribute
-- the linked executables. You must obey the GNU General Public License
-- in all respects for all of the code used other than "OpenSSL". If you
-- modify file(s) with this exception, you may extend this exception to
-- your version of the file(s), but you are not obligated to do so. If
-- you do not wish to do so, delete this exception statement from your
-- version. If you delete this exception statement from all source files
-- in the program, then also delete it here.
-- 
-- This program is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
-- General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program. If not, see <http://www.gnu.org/licenses/>.


--
-- This SQLite script updates the version of the Orthanc database from 4 to 5.
--


-- Remove 2 indexes to speed up

DROP INDEX MainDicomTagsIndex2;
DROP INDEX MainDicomTagsIndexValues;


-- Add a new table to index the DICOM identifiers

CREATE TABLE DicomIdentifiers(
       id INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       tagGroup INTEGER,
       tagElement INTEGER,
       value TEXT,
       PRIMARY KEY(id, tagGroup, tagElement)
       );

CREATE INDEX DicomIdentifiersIndex1 ON DicomIdentifiers(id);
CREATE INDEX DicomIdentifiersIndex2 ON DicomIdentifiers(tagGroup, tagElement);
CREATE INDEX DicomIdentifiersIndexValues ON DicomIdentifiers(value COLLATE BINARY);


-- Migrate data from MainDicomTags to MainResourcesTags and MainInstancesTags

INSERT INTO DicomIdentifiers SELECT * FROM MainDicomTags
       WHERE ((tagGroup = 16 AND tagElement = 32) OR  -- PatientID (0x0010, 0x0020)
              (tagGroup = 32 AND tagElement = 13) OR  -- StudyInstanceUID (0x0020, 0x000d)
              (tagGroup = 8  AND tagElement = 80) OR  -- AccessionNumber (0x0008, 0x0050)
              (tagGroup = 32 AND tagElement = 14) OR  -- SeriesInstanceUID (0x0020, 0x000e)
              (tagGroup = 8  AND tagElement = 24));   -- SOPInstanceUID (0x0008, 0x0018)

DELETE FROM MainDicomTags
       WHERE ((tagGroup = 16 AND tagElement = 32) OR  -- PatientID (0x0010, 0x0020)
              (tagGroup = 32 AND tagElement = 13) OR  -- StudyInstanceUID (0x0020, 0x000d)
              (tagGroup = 8  AND tagElement = 80) OR  -- AccessionNumber (0x0008, 0x0050)
              (tagGroup = 32 AND tagElement = 14) OR  -- SeriesInstanceUID (0x0020, 0x000e)
              (tagGroup = 8  AND tagElement = 24));   -- SOPInstanceUID (0x0008, 0x0018)


-- Upgrade the "ResourceDeleted" trigger

DROP TRIGGER ResourceDeleted;
DROP TRIGGER ResourceDeletedParentCleaning;

CREATE TRIGGER ResourceDeleted
AFTER DELETE ON Resources
BEGIN
  SELECT SignalResourceDeleted(old.publicId, old.resourceType);
  SELECT SignalRemainingAncestor(parent.publicId, parent.resourceType) 
    FROM Resources AS parent WHERE internalId = old.parentId;
END;

CREATE TRIGGER ResourceDeletedParentCleaning
AFTER DELETE ON Resources
FOR EACH ROW WHEN (SELECT COUNT(*) FROM Resources WHERE parentId = old.parentId) = 0
BEGIN
  DELETE FROM Resources WHERE internalId = old.parentId;
END;


-- Change the database version
-- The "1" corresponds to the "GlobalProperty_DatabaseSchemaVersion" enumeration

UPDATE GlobalProperties SET value="5" WHERE property=1;
