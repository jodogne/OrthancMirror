CREATE TABLE GlobalIntegers(
       key INTEGER PRIMARY KEY,
       value INTEGER);

INSERT INTO GlobalProperties VALUES (6, 1);  -- GlobalProperty_GetTotalSizeIsFast

INSERT INTO GlobalIntegers SELECT 0, IFNULL(SUM(compressedSize), 0) FROM AttachedFiles;
INSERT INTO GlobalIntegers SELECT 1, IFNULL(SUM(uncompressedSize), 0) FROM AttachedFiles;

CREATE TRIGGER AttachedFileIncrementSize
AFTER INSERT ON AttachedFiles
BEGIN
  UPDATE GlobalIntegers SET value = value + new.compressedSize WHERE key = 0;
  UPDATE GlobalIntegers SET value = value + new.uncompressedSize WHERE key = 1;
END;

CREATE TRIGGER AttachedFileDecrementSize
AFTER DELETE ON AttachedFiles
BEGIN
  UPDATE GlobalIntegers SET value = value - old.compressedSize WHERE key = 0;
  UPDATE GlobalIntegers SET value = value - old.uncompressedSize WHERE key = 1;
END;
