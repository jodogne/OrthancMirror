-- Orthanc - A Lightweight, RESTful DICOM Store
-- Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
-- Department, University Hospital of Liege, Belgium
-- Copyright (C) 2017-2022 Osimis S.A., Belgium
-- Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


--
-- This SQLite script updates the version of the Orthanc database from 6 to 7.
--

-- Add new columns for revision
ALTER TABLE Metadata ADD COLUMN revision INTEGER;
ALTER TABLE AttachedFiles ADD COLUMN revision INTEGER;

-- Add new column for customData
ALTER TABLE AttachedFiles ADD COLUMN customData TEXT;


-- add another AttachedFileDeleted trigger 
-- We want to keep backward compatibility and avoid changing the database version number (which would force
-- users to upgrade the DB).  By keeping backward compatibility, we mean "allow a user to run a previous Orthanc
-- version after it has run this update script".
-- We must keep the signature of the initial trigger (it is impossible to have 2 triggers on the same event).
-- We tried adding a trigger on "BEFORE DELETE" but then it is being called when running the previous Orthanc
-- which makes it fail.
-- But, we need the customData in the C++ function that is called when a AttachedFiles is deleted.
-- The trick is then to save the customData in a DeletedFiles table.
-- The SignalFileDeleted C++ function will then get the customData from this table and delete the entry.
-- Drawback: if you downgrade Orthanc, the DeletedFiles table will remain and will be populated by the trigger
-- but not consumed by the C++ function -> we consider this is an acceptable drawback for a few people compared 
-- to the burden of upgrading the DB.

CREATE TABLE DeletedFiles(
       uuid TEXT NOT NULL,        -- 0
       customData TEXT            -- 1
);

DROP TRIGGER AttachedFileDeleted;

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
