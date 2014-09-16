-- This SQLite script updates the version of the Orthanc database from 4 to 5.


CREATE TABLE MainResourcesTags(
       id INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       tagGroup INTEGER,
       tagElement INTEGER,
       value TEXT,
       PRIMARY KEY(id, tagGroup, tagElement)
       );

CREATE TABLE MainInstancesTags(
       id INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       tagGroup INTEGER,
       tagElement INTEGER,
       value TEXT,
       PRIMARY KEY(id, tagGroup, tagElement)
       );

CREATE INDEX MainResourcesTagsIndex1 ON MainResourcesTags(id);
CREATE INDEX MainResourcesTagsIndex2 ON MainResourcesTags(tagGroup, tagElement);
CREATE INDEX MainResourcesTagsIndexValues ON MainResourcesTags(value COLLATE BINARY);
CREATE INDEX MainInstancesTagsIndex ON MainInstancesTags(id);


-- Migrate data from MainDicomTags to MainResourcesTags and MainInstancesTags
-- Below, the value "4" corresponds to "ResourceType_Instance".
-- The "8" and "24" correspond to SOPInstanceUID (0x0008, 0x0018)

INSERT INTO MainResourcesTags SELECT MainDicomTags.* FROM MainDicomTags
       INNER JOIN Resources ON Resources.internalId = MainDicomTags.id
       WHERE (Resources.resourceType != 4 OR
              (MainDicomTags.tagGroup = 8 AND
               MainDicomTags.tagElement = 24));

INSERT INTO MainInstancesTags SELECT MainDicomTags.* FROM MainDicomTags
       INNER JOIN Resources ON Resources.internalId = MainDicomTags.id
       WHERE (Resources.resourceType = 4 AND
              (MainDicomTags.tagGroup != 8 OR
               MainDicomTags.tagElement != 24));

-- Remove the MainDicomTags table

DROP INDEX MainDicomTagsIndex1;
DROP INDEX MainDicomTagsIndex2;
DROP INDEX MainDicomTagsIndexValues;
DROP TABLE MainDicomTags;


-- Upgrade the "ResourceDeleted" trigger

DROP TRIGGER ResourceDeleted;

CREATE TRIGGER ResourceDeleted
AFTER DELETE ON Resources
BEGIN
  SELECT SignalResourceDeleted(old.publicId, old.resourceType);
  SELECT SignalRemainingAncestor(parent.publicId, parent.resourceType) 
    FROM Resources AS parent WHERE internalId = old.parentId;
END;


-- Change the database version
-- The "1" corresponds to the "GlobalProperty_DatabaseSchemaVersion" enumeration

UPDATE GlobalProperties SET value="5" WHERE property=1;
