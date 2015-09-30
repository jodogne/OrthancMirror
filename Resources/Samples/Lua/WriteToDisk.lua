TARGET = '/tmp/lua'

function ToAscii(s)
   -- http://www.lua.org/manual/5.1/manual.html#pdf-string.gsub
   return s:gsub('[^a-zA-Z0-9-/ ]', '_')
end

function OnStableSeries(seriesId, tags, metadata)
   print('This series is now stable, writing its instances on the disk: ' .. seriesId)

   local instances = ParseJson(RestApiGet('/series/' .. seriesId)) ['Instances']
   local patient = ParseJson(RestApiGet('/series/' .. seriesId .. '/patient')) ['MainDicomTags']
   local study = ParseJson(RestApiGet('/series/' .. seriesId .. '/study')) ['MainDicomTags']
   local series = ParseJson(RestApiGet('/series/' .. seriesId)) ['MainDicomTags']

   for i, instance in pairs(instances) do
      local path = ToAscii(TARGET .. '/' .. 
                              patient['PatientID'] .. ' - ' .. patient['PatientName'] .. '/' ..
                              study['StudyDate'] .. ' - ' .. study['StudyDescription'] .. '/' ..
                              series['SeriesDescription'])

      -- Retrieve the DICOM file from Orthanc
      local dicom = RestApiGet('/instances/' .. instance .. '/file')

      -- Create the subdirectory (CAUTION: For Linux demo only, this is insecure!)
      -- http://stackoverflow.com/a/16029744/881731
      os.execute('mkdir -p "' .. path .. '"')

      -- Write to the file
      local target = assert(io.open(path .. '/' .. instance .. '.dcm', 'wb'))
      target:write(dicom)
      target:close()
   end
end
