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
