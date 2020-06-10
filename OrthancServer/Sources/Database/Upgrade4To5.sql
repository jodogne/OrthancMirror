-- This SQLite script updates the version of the Orthanc database from 4 to 5.


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
