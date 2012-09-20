CREATE TABLE GlobalProperties(
       name TEXT PRIMARY KEY,
       value TEXT
       );

CREATE TABLE Patients(
       uuid TEXT PRIMARY KEY,
       dicomPatientId TEXT
       );

CREATE TABLE Studies(
       uuid TEXT PRIMARY KEY,
       parentPatient TEXT REFERENCES Patients(uuid) ON DELETE CASCADE,
       dicomStudy TEXT
       );

CREATE TABLE Series(
       uuid TEXT PRIMARY KEY,
       parentStudy TEXT REFERENCES Studies(uuid) ON DELETE CASCADE,
       dicomSeries TEXT,
       numberOfInstances INTEGER
       );

CREATE TABLE Instances(
       uuid TEXT PRIMARY KEY,
       parentSeries TEXT REFERENCES Series(uuid) ON DELETE CASCADE,
       dicomInstance TEXT,
       fileUuid TEXT,
       fileSize INTEGER,
       jsonUuid TEXT,
       distantAet TEXT,
       instanceIndex INTEGER
       );

CREATE TABLE MainDicomTags(
       uuid TEXT,
       tagGroup INTEGER,
       tagElement INTEGER,
       value TEXT,
       PRIMARY KEY(uuid, tagGroup, tagElement)
       );

CREATE TABLE Changes(
       seq INTEGER PRIMARY KEY AUTOINCREMENT,
       basePath TEXT,
       uuid TEXT
       );


CREATE INDEX PatientToStudies ON Studies(parentPatient);
CREATE INDEX StudyToSeries ON Series(parentStudy);
CREATE INDEX SeriesToInstances ON Instances(parentSeries);

CREATE INDEX DicomPatientIndex ON Patients(dicomPatientId);
CREATE INDEX DicomStudyIndex ON Studies(dicomStudy);
CREATE INDEX DicomSeriesIndex ON Series(dicomSeries);
CREATE INDEX DicomInstanceIndex ON Instances(dicomInstance);

CREATE INDEX MainDicomTagsIndex ON MainDicomTags(uuid);
CREATE INDEX MainDicomTagsGroupElement ON MainDicomTags(tagGroup, tagElement);
CREATE INDEX MainDicomTagsValues ON MainDicomTags(value COLLATE BINARY);

CREATE INDEX ChangesIndex ON Changes(uuid);

CREATE TRIGGER InstanceRemoved
AFTER DELETE ON Instances
FOR EACH ROW BEGIN
  DELETE FROM MainDicomTags WHERE uuid = old.uuid;
  DELETE FROM Changes WHERE uuid = old.uuid;
  SELECT DeleteFromFileStorage(old.fileUuid);
  SELECT DeleteFromFileStorage(old.jsonUuid);
  SELECT SignalDeletedLevel(3, old.parentSeries);
END;

CREATE TRIGGER SeriesRemoved
AFTER DELETE ON Series
FOR EACH ROW BEGIN
  DELETE FROM MainDicomTags WHERE uuid = old.uuid;
  DELETE FROM Changes WHERE uuid = old.uuid;
  SELECT SignalDeletedLevel(2, old.parentStudy);
END;

CREATE TRIGGER StudyRemoved
AFTER DELETE ON Studies
FOR EACH ROW BEGIN
  DELETE FROM MainDicomTags WHERE uuid = old.uuid;
  DELETE FROM Changes WHERE uuid = old.uuid;
  SELECT SignalDeletedLevel(1, old.parentPatient);
END;

CREATE TRIGGER PatientRemoved
AFTER DELETE ON Patients
FOR EACH ROW BEGIN
  DELETE FROM MainDicomTags WHERE uuid = old.uuid;
  DELETE FROM Changes WHERE uuid = old.uuid;
  SELECT SignalDeletedLevel(0, "");
END;




CREATE TRIGGER InstanceRemovedUpwardCleaning
AFTER DELETE ON Instances
FOR EACH ROW 
  WHEN (SELECT COUNT(*) FROM Instances WHERE parentSeries = old.parentSeries) = 0
  BEGIN
    SELECT DeleteFromFileStorage("deleting parent series");  -- TODO REMOVE THIS
    DELETE FROM Series WHERE uuid = old.parentSeries;
  END;

CREATE TRIGGER SeriesRemovedUpwardCleaning
AFTER DELETE ON Series
FOR EACH ROW 
  WHEN (SELECT COUNT(*) FROM Series WHERE parentStudy = old.parentStudy) = 0
  BEGIN
    SELECT DeleteFromFileStorage("deleting parent study");  -- TODO REMOVE THIS
    DELETE FROM Studies WHERE uuid = old.parentStudy;
  END;

CREATE TRIGGER StudyRemovedUpwardCleaning
AFTER DELETE ON Studies
FOR EACH ROW 
  WHEN (SELECT COUNT(*) FROM Studies WHERE parentPatient = old.parentPatient) = 0
  BEGIN
    SELECT DeleteFromFileStorage("deleting parent patient");  -- TODO REMOVE THIS
    DELETE FROM Patients WHERE uuid = old.parentPatient;
  END;
