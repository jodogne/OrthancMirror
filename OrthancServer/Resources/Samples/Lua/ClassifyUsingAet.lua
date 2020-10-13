-- Write the received DICOM instances to an external directory, and
-- group the patients by the source AET that has sent the DICOM
-- files. This sample is similar to "WriteToDisk.lua".


TARGET = '/tmp/lua'


function ToAscii(s)
   -- http://www.lua.org/manual/5.1/manual.html#pdf-string.gsub
   -- https://groups.google.com/d/msg/orthanc-users/qMLgkEmwwPI/6jRpCrlgBwAJ
   return s:gsub('[^a-zA-Z0-9-/-: ]', '_')
end


function GetFromTable(t, key, defaultValue)
   local result = t[key]
   if result == nil or result == '' then
      return defaultValue
   else
      return result
   end
end


function OnStoredInstance(instanceId, tags, metadata, origin)
   local dicom = RestApiGet('/instances/' .. instanceId .. '/file')
   local tags = ParseJson(RestApiGet('/instances/' .. instanceId .. '/tags?simplify'))
   local metadata = ParseJson(RestApiGet('/instances/' .. instanceId .. '/metadata?expand'))

   local path = ToAscii(
      TARGET .. '/' ..
         GetFromTable(metadata, 'RemoteAET', 'None') .. '/'..
         GetFromTable(tags, 'PatientID', '') .. ' - ' .. GetFromTable(tags, 'PatientName', '') .. '/' ..
         GetFromTable(tags, 'StudyDate', '') .. ' - ' .. GetFromTable(tags, 'StudyDescription', '') .. '/' ..
         GetFromTable(tags, 'SeriesDescription', ''))

   -- Create the subdirectory (CAUTION: For Linux demo only, this is insecure!)
   -- http://stackoverflow.com/a/16029744/881731
   os.execute('mkdir -p "' .. path .. '"')
   
   -- Write to the file
   local target = assert(io.open(path .. '/' .. GetFromTable(tags, 'SOPInstanceUID', 'none') .. '.dcm', 'wb'))
   target:write(dicom)
   target:close()

   -- Optional step: Remove the source file
   RestApiDelete('/instances/' .. instanceId)
end
