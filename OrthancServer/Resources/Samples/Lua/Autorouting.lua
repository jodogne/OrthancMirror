function OnStoredInstance(instanceId, tags, metadata)
   Delete(SendToModality(instanceId, 'sample'))
end
