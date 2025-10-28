-- Orthanc - A Lightweight, RESTful DICOM Store
-- Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
-- Department, University Hospital of Liege, Belgium
-- Copyright (C) 2017-2023 Osimis S.A., Belgium
-- Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
-- Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
--
-- This program is free software: you can redistribute it and/or
-- modify it under the terms of the GNU General Public License as
-- published by the Free Software Foundation, either version 3 of the
-- License, or (at your option) any later version.
-- 
-- This program is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
-- General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program. If not, see <http://www.gnu.org/licenses/>.


CREATE TABLE GlobalProperties(
       property INTEGER PRIMARY KEY,
       value TEXT
       );

CREATE TABLE Resources(
       internalId INTEGER PRIMARY KEY AUTOINCREMENT,
       resourceType INTEGER,
       publicId TEXT,
       parentId INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE
       );

CREATE TABLE MainDicomTags(
       id INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       tagGroup INTEGER,
       tagElement INTEGER,
       value TEXT,
       PRIMARY KEY(id, tagGroup, tagElement)
       );

-- The following table was added in Orthanc 0.8.5 (database v5)
-- It contains only the DICOM Tags that are commonly used for searches.
-- All these tags are converted to UPPERCASE !
-- These tags are also stored in the MainDicomTags table without casing modificiation.
CREATE TABLE DicomIdentifiers(
       id INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       tagGroup INTEGER,
       tagElement INTEGER,
       value TEXT,
       PRIMARY KEY(id, tagGroup, tagElement)
       );

CREATE TABLE Metadata(
       id INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       type INTEGER,
       value TEXT,
       revision INTEGER,      -- New in Orthanc 1.12.8 (added in InstallRevisionAndCustomData.sql)
       PRIMARY KEY(id, type)
       );

CREATE TABLE AttachedFiles(
       id INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       fileType INTEGER,
       uuid TEXT,
       compressedSize INTEGER,
       uncompressedSize INTEGER,
       compressionType INTEGER,
       uncompressedMD5 TEXT,  -- New in Orthanc 0.7.3 (database v4)
       compressedMD5 TEXT,    -- New in Orthanc 0.7.3 (database v4)
       revision INTEGER,      -- New in Orthanc 1.12.8 (added in InstallRevisionAndCustomData.sql)
       customData BLOB,       -- New in Orthanc 1.12.8 (added in InstallDeletedFiles.sql)
       PRIMARY KEY(id, fileType)
       );              

CREATE TABLE Changes(
       seq INTEGER PRIMARY KEY AUTOINCREMENT,
       changeType INTEGER,
       internalId INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       resourceType INTEGER,
       date TEXT
       );

CREATE TABLE ExportedResources(
       seq INTEGER PRIMARY KEY AUTOINCREMENT,
       resourceType INTEGER,
       publicId TEXT,
       remoteModality TEXT,
       patientId TEXT,
       studyInstanceUid TEXT,
       seriesInstanceUid TEXT,
       sopInstanceUid TEXT,
       date TEXT
       ); 

CREATE TABLE PatientRecyclingOrder(
       seq INTEGER PRIMARY KEY AUTOINCREMENT,
       patientId INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE
       );

CREATE INDEX ChildrenIndex ON Resources(parentId);
CREATE INDEX PublicIndex ON Resources(publicId);
CREATE INDEX ResourceTypeIndex ON Resources(resourceType);
CREATE INDEX PatientRecyclingIndex ON PatientRecyclingOrder(patientId);

CREATE INDEX MainDicomTagsIndex1 ON MainDicomTags(id);

-- The 3 following indexes were added in Orthanc 0.8.5 (database v5)
CREATE INDEX DicomIdentifiersIndex1 ON DicomIdentifiers(id);
-- CREATE INDEX DicomIdentifiersIndex2 ON DicomIdentifiers(tagGroup, tagElement);  -- disabled in 1.12.10
CREATE INDEX DicomIdentifiersIndexValues ON DicomIdentifiers(value COLLATE BINARY);

CREATE INDEX DicomIdentifiersIndexGroupElementValue ON DicomIdentifiers(tagGroup, tagElement, value); -- added in 1.12.10

CREATE INDEX ChangesIndex ON Changes(internalId);


CREATE TRIGGER ResourceDeleted
AFTER DELETE ON Resources
BEGIN
  SELECT SignalResourceDeleted(old.publicId, old.resourceType);  -- New in Orthanc 0.8.5 (db v5)
  SELECT SignalRemainingAncestor(parent.publicId, parent.resourceType) 
    FROM Resources AS parent WHERE internalId = old.parentId;
END;

-- Delete a parent resource when its unique child is deleted 
CREATE TRIGGER ResourceDeletedParentCleaning
AFTER DELETE ON Resources
FOR EACH ROW WHEN (SELECT COUNT(*) FROM Resources WHERE parentId = old.parentId) = 0
BEGIN
  DELETE FROM Resources WHERE internalId = old.parentId;
END;

CREATE TRIGGER PatientAdded
AFTER INSERT ON Resources
FOR EACH ROW WHEN new.resourceType = 1  -- "1" corresponds to "ResourceType_Patient" in C++
BEGIN
  INSERT INTO PatientRecyclingOrder VALUES (NULL, new.internalId);
END;


-- new in Orthanc 1.5.1 -------------------------- equivalent to InstallTrackAttachmentsSize.sql
${INSTALL_TRACK_ATTACHMENTS_SIZE}


-- new in Orthanc 1.12.0 ------------------------- equivalent to InstallLabelsTable.sql
${INSTALL_LABELS_TABLE}


-- new in Orthanc 1.12.8 ------------------------- equivalent to InstallDeletedFiles.sql
${INSTALL_DELETED_FILES}


-- new in Orthanc 1.12.8 ------------------------- equivalent to InstallKeyValueStoresAndQueues.sql
${INSTALL_KEY_VALUE_STORES_AND_QUEUES}

-- new in Orthanc 1.12.10 ------------------------- equivalent to AddTimeoutToQueues.sql
${ADD_TIMEOUT_TO_QUEUES}


-- Track the fact that the "revision" column exists in the "Metadata" and "AttachedFiles"
-- tables, and that the "customData" column exists in the "AttachedFiles" table
INSERT INTO GlobalProperties VALUES (7, 1);  -- GlobalProperty_SQLiteHasCustomDataAndRevision


-- Set the version of the database schema
-- The "1" corresponds to the "GlobalProperty_DatabaseSchemaVersion" enumeration
INSERT INTO GlobalProperties VALUES (1, "6");
