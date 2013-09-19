-- New in database version 4
CREATE TABLE AvailableTags(
       tagGroup INTEGER,
       tagElement INTEGER,
       PRIMARY KEY(tagGroup, tagElement)
       );

-- Until database version 4, the following index was set to "COLLATE
-- BINARY". This implies case-sensitive searches, but DICOM C-Find
-- requires case-insensitive searches.
-- http://www.sqlite.org/optoverview.html#like_opt
CREATE INDEX MainDicomTagsIndexValues ON MainDicomTags(value COLLATE NOCASE);
