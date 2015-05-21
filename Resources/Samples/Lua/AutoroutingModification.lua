function OnStoredInstance(instanceId, tags, metadata)
   -- Ignore the instances that result from a modification to avoid
   -- infinite loops
   if (metadata['ModifiedFrom'] == nil and
       metadata['AnonymizedFrom'] == nil) then

      -- The tags to be replaced
      local replace = {}
      replace['StationName'] = 'My Medical Device'
      replace['0031-1020'] = 'Some private tag'

      -- The tags to be removed
      local remove = { 'MilitaryRank' }

      -- Modify the instance, send it, then delete the modified instance
      Delete(SendToModality(ModifyInstance(instanceId, replace, remove, true), 'sample'))

      -- Delete the original instance
      Delete(instanceId)
   end
end
