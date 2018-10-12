-- Answer to:
-- https://groups.google.com/d/msg/orthanc-users/0ymHe1cDBCQ/YfD0NoOTn0wJ
-- Applicable starting with Orthanc 0.9.5

function OnStoredInstance(instanceId, tags, metadata, origin)
   -- Do not modify twice the same file
   if origin['RequestOrigin'] ~= 'Lua' then
      local replace = {}
      replace['0010,1002'] = {}
      replace['0010,1002'][1] = {}
      replace['0010,1002'][1]['PatientID'] = 'Hello'
      replace['0010,1002'][2] = {}
      replace['0010,1002'][2]['PatientID'] = 'World'

      local request = {}
      request['Replace'] = replace

      -- Create the modified instance
      local modified = RestApiPost('/instances/' .. instanceId .. '/modify',
                                   DumpJson(request, true))

      -- Upload the modified instance to the Orthanc store
      RestApiPost('/instances/', modified)

      -- Delete the original instance
      RestApiDelete('/instances/' .. instanceId)
   end
end
