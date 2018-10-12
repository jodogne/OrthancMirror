function OnStoredInstance(instanceId, tags, metadata, origin)
   -- Ignore the instances that result from the present Lua script to
   -- avoid infinite loops
   if origin['RequestOrigin'] ~= 'Lua' then

      -- The tags to be replaced
      local replace = {}
      replace['StationName'] = 'My Medical Device'
      replace['0031-1020'] = 'Some private tag'

      -- The tags to be removed
      local remove = { 'MilitaryRank' }

      -- Modify the instance
      local command = {}
      command['Replace'] = replace
      command['Remove'] = remove
      local modifiedFile = RestApiPost('/instances/' .. instanceId .. '/modify', DumpJson(command, true))

      -- Upload the modified instance to the Orthanc database so that
      -- it can be sent by Orthanc to other modalities
      local modifiedId = ParseJson(RestApiPost('/instances/', modifiedFile)) ['ID']

      -- Send the modified instance to another modality
      RestApiPost('/modalities/sample/store', modifiedId)

      -- Delete the original and the modified instances
      RestApiDelete('/instances/' .. instanceId)
      RestApiDelete('/instances/' .. modifiedId)
   end
end
