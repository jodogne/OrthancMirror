CREATE TABLE GlobalProperties(
       name TEXT PRIMARY KEY,
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

CREATE TABLE Metadata(
       id INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       type INTEGER,
       value TEXT,
       PRIMARY KEY(id, type)
       );

CREATE TABLE AttachedFiles(
       id INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       contentName TEXT,
       uuid TEXT,
       compressedSize INTEGER,
       uncompressedSize INTEGER,
       compressionType INTEGER,
       PRIMARY KEY(id, contentName)
       );              

CREATE TABLE Changes(
       seq INTEGER PRIMARY KEY AUTOINCREMENT,
       changeType INTEGER,
       internalId INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       resourceType INTEGER,
       date TEXT
       );

CREATE TABLE ExportedInstances(
       seq INTEGER PRIMARY KEY AUTOINCREMENT,
       remoteModality TEXT,
       publicId TEXT,
       patientId TEXT,
       studyInstanceUid TEXT,
       seriesInstanceUid TEXT,
       sopInstanceUid TEXT,
       date TEXT
       ); 

CREATE INDEX ChildrenIndex ON Resources(parentId);
CREATE INDEX PublicIndex ON Resources(publicId);
CREATE INDEX ResourceTypeIndex ON Resources(resourceType);

CREATE INDEX MainDicomTagsIndex1 ON MainDicomTags(id);
CREATE INDEX MainDicomTagsIndex2 ON MainDicomTags(tagGroup, tagElement);
CREATE INDEX MainDicomTagsIndexValues ON MainDicomTags(value COLLATE BINARY);

CREATE INDEX ChangesIndex ON Changes(internalId);

CREATE TRIGGER AttachedFileDeleted
AFTER DELETE ON AttachedFiles
BEGIN
  SELECT SignalFileDeleted(old.uuid);
END;

CREATE TRIGGER ResourceDeleted
AFTER DELETE ON Resources
BEGIN
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
