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
       customData TEXT,       -- New in Orthanc 1.12.8 (added in InstallRevisionAndCustomData.sql)
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
CREATE INDEX DicomIdentifiersIndex2 ON DicomIdentifiers(tagGroup, tagElement);
CREATE INDEX DicomIdentifiersIndexValues ON DicomIdentifiers(value COLLATE BINARY);

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

CREATE TABLE GlobalIntegers(
       key INTEGER PRIMARY KEY,
       value INTEGER);

INSERT INTO GlobalProperties VALUES (6, 1);  -- GlobalProperty_GetTotalSizeIsFast

INSERT INTO GlobalIntegers SELECT 0, IFNULL(SUM(compressedSize), 0) FROM AttachedFiles;
INSERT INTO GlobalIntegers SELECT 1, IFNULL(SUM(uncompressedSize), 0) FROM AttachedFiles;

CREATE TRIGGER AttachedFileIncrementSize
AFTER INSERT ON AttachedFiles
BEGIN
  UPDATE GlobalIntegers SET value = value + new.compressedSize WHERE key = 0;
  UPDATE GlobalIntegers SET value = value + new.uncompressedSize WHERE key = 1;
END;

CREATE TRIGGER AttachedFileDecrementSize
AFTER DELETE ON AttachedFiles
BEGIN
  UPDATE GlobalIntegers SET value = value - old.compressedSize WHERE key = 0;
  UPDATE GlobalIntegers SET value = value - old.uncompressedSize WHERE key = 1;
END;

--------------------------------------------------


-- new in Orthanc 1.12.0 ------------------------- equivalent to InstallLabelsTable.sql

CREATE TABLE Labels(
       id INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       label TEXT NOT NULL,
       PRIMARY KEY(id, label)  -- Prevents duplicates
       );

CREATE INDEX LabelsIndex1 ON Labels(id);
CREATE INDEX LabelsIndex2 ON Labels(label);  -- This index allows efficient lookups

--------------------------------------------------


-- new in Orthanc 1.12.8 ------------------------- equivalent to InstallRevisionAndCustomData.sql

CREATE TABLE DeletedFiles(
       uuid TEXT NOT NULL,        -- 0
       customData TEXT            -- 1
);

CREATE TRIGGER AttachedFileDeleted
AFTER DELETE ON AttachedFiles
BEGIN
  INSERT INTO DeletedFiles VALUES(old.uuid, old.customData);
  SELECT SignalFileDeleted(old.uuid, old.fileType, old.uncompressedSize, 
                           old.compressionType, old.compressedSize,
                           old.uncompressedMD5, old.compressedMD5
                           );
END;

-- Record that this upgrade has been performed

INSERT INTO GlobalProperties VALUES (7, 1);  -- GlobalProperty_SQLiteHasCustomDataAndRevision

---------------------------------------------------


-- new in Orthanc 1.12.8 ------------------------- equivalent to InstallKeyValueStoresAndQueues.sql
CREATE TABLE KeyValueStores(
       storeId TEXT NOT NULL,
       key TEXT NOT NULL,
       value TEXT NOT NULL,
       PRIMARY KEY(storeId, key)  -- Prevents duplicates
       );

CREATE TABLE Queues (
       id INTEGER PRIMARY KEY AUTOINCREMENT,
       queueId TEXT NOT NULL,
       value TEXT
);

CREATE INDEX QueuesIndex ON Queues (queueId, id);

---------------------------------------------------

-- Set the version of the database schema
-- The "1" corresponds to the "GlobalProperty_DatabaseSchemaVersion" enumeration
INSERT INTO GlobalProperties VALUES (1, "6");
