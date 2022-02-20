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
-- This SQLite script updates the version of the Orthanc database from 3 to 4.
--


-- Add 2 new columns at "AttachedFiles"

ALTER TABLE AttachedFiles ADD COLUMN uncompressedMD5 TEXT;
ALTER TABLE AttachedFiles ADD COLUMN compressedMD5 TEXT;

-- Update the "AttachedFileDeleted" trigger

DROP TRIGGER AttachedFileDeleted;

CREATE TRIGGER AttachedFileDeleted
AFTER DELETE ON AttachedFiles
BEGIN
  SELECT SignalFileDeleted(old.uuid, old.fileType, old.uncompressedSize, 
                           old.compressionType, old.compressedSize,
                           -- These 2 arguments are new in Orthanc 0.7.3 (database v4)
                           old.uncompressedMD5, old.compressedMD5);
END;

-- Change the database version
-- The "1" corresponds to the "GlobalProperty_DatabaseSchemaVersion" enumeration

UPDATE GlobalProperties SET value="4" WHERE property=1;
