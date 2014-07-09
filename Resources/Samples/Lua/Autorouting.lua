function OnStoredInstance(instanceId, tags, metadata)
   --PrintRecursive(tags)
   --PrintRecursive(metadata)
   --print(metadata['RemoteAET'])

   if (metadata['ModifiedFrom'] == nil and
       metadata['AnonymizedFrom'] == nil) then
      local patientName = string.lower(tags['PatientName'])
      if string.find(patientName, 'david') ~= nil then
         --Delete(SendToModality(instanceId, 'sample'))
         --Delete(SendToPeer(instanceId, 'peer'))
         local replace = {}
         replace['StationName'] = 'My Medical Device'

         local remove = { 'MilitaryRank' }

         Delete(SendToModality(ModifyInstance(instanceId, replace, remove, true), 'sample'))
         Delete(instanceId)
      else
         Delete(instanceId)
      end
   end
end
