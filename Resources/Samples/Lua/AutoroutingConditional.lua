function OnStoredInstance(instanceId, tags, metadata, remoteAet, calledAet)
   -- The "remoteAet" and "calledAet" arguments are only available
   -- since Orthanc 0.8.6
   if remoteAet ~=nil and calledAet ~= nil then
      print ("Source AET: " .. remoteAet .. " => Called AET: " .. calledAet)
   end

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
