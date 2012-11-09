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
       name TEXT,
       uuid TEXT,
       uncompressedSize INTEGER,
       compressionType INTEGER,
       PRIMARY KEY(id, name)
       );              

CREATE INDEX ChildrenIndex ON Resources(parentId);
CREATE INDEX PublicIndex ON Resources(publicId);


CREATE TRIGGER AttachedFileDeleted
AFTER DELETE ON AttachedFiles
BEGIN
  SELECT SignalFileDeleted(old.uuid);
END;

CREATE TRIGGER ResourceDeleted
AFTER DELETE ON Resources
BEGIN
  SELECT SignalResourceDeleted(old.resourceType, old.parentId);
END;


-- -- Delete a resource when its unique child is deleted  TODO TODO
-- CREATE TRIGGER ResourceRemovedUpward
-- AFTER DELETE ON Resources
-- FOR EACH ROW
--   WHEN (SELECT COUNT(*) FROM ParentRelationship WHERE parent = old.
-- END;
