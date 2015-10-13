-- This SQLite script updates the version of the Orthanc database from 5 to 6.


-- Add a new table to enable full-text indexed search over studies

CREATE TABLE SearchableStudies(
       id INTEGER REFERENCES Resources(internalId) ON DELETE CASCADE,
       tagGroup INTEGER,
       tagElement INTEGER,
       value TEXT,  -- assumed to be in upper case
       PRIMARY KEY(id, tagGroup, tagElement)
       );

CREATE INDEX SearchableStudiesIndex1 ON SearchableStudies(id);
CREATE INDEX SearchableStudiesIndexValues ON SearchableStudies(value COLLATE BINARY);


-- Change the database version
-- The "1" corresponds to the "GlobalProperty_DatabaseSchemaVersion" enumeration

UPDATE GlobalProperties SET value="6" WHERE property=1;
