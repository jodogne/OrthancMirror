function _InitializeJob()
   _job = {}
end

function _AccessJob()
   return _job
end

function SendToModality(instanceId, modality)
   if instanceId == nil then
      error('Cannot send an nonexistent instance')
   end

   table.insert(_job, { 
                   operation = 'store-scu', 
                   instance = instanceId,
                   modality = modality 
                })
   return instanceId
end

function SendToPeer(instanceId, peer)
   if instanceId == nil then
      error('Cannot send an nonexistent instance')
   end

   table.insert(_job, { 
                   operation = 'store-peer', 
                   instance = instanceId,
                   peer = peer
                })
   return instanceId
end

function Delete(instanceId)
   if instanceId == nil then
      error('Cannot delete an nonexistent instance')
   end

   table.insert(_job, { 
                   operation = 'delete', 
                   instance = instanceId
                })
   return nil  -- Forbid chaining
end

function Modify(instanceId, replacements, removals, removePrivateTags)
   if instanceId == nil then
      error('Cannot modify an nonexistent instance')
   end

   if instanceId == '' then
      error('Cannot modify twice an instance');
   end

   table.insert(_job, { 
                   operation = 'modify', 
                   instance = instanceId,
                   replacements = replacements, 
                   removals = removals,
                   removePrivateTags = removePrivateTags 
                })
   return ''  -- Chain with another operation
end


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
         SendToModality(Modify(instanceId, { PatientName = 'Hello^World' }), 'sample')
         Delete(instanceId)
      else
         Delete(instanceId)
      end
   end
end
