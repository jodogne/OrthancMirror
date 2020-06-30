function OnStoredInstance(instanceId, tags, metadata, origin)
   -- The "origin" is only available since Orthanc 0.9.4
   PrintRecursive(origin)

   -- Extract the value of the "PatientName" DICOM tag
   local patientName = string.lower(tags['PatientName'])

   if string.find(patientName, 'david') ~= nil then
      -- Only send patients whose name contains "David"
      Delete(SendToModality(instanceId, 'sample'))

   else
      -- Delete the patients that are not called "David"
      Delete(instanceId)
   end
end
