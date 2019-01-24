-- This SQLite script updates the version of the Orthanc database from 3 to 4.

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
