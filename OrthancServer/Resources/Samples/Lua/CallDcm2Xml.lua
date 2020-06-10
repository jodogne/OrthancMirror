function OnStoredInstance(instanceId, tags, metadata)
   -- Assume Latin1 encoding in dcm2xml
   local args = {}
   table.insert(args, '+Ca')
   table.insert(args, 'latin-1')

   Delete(CallSystem(instanceId, 'dcm2xml', args))
end
