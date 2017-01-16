function Initialize()
   print('Number of stored studies at initialization: ' ..
            table.getn(ParseJson(RestApiGet('/studies'))))
end


function Finalize()
   print('Number of stored studies at finalization: ' ..
            table.getn(ParseJson(RestApiGet('/studies'))))
end


function OnStoredInstance(instanceId, tags, metadata)
   patient = ParseJson(RestApiGet('/instances/' .. instanceId .. '/patient'))
   print('Received an instance for patient: ' .. 
            patient['MainDicomTags']['PatientID'] .. ' - ' .. 
            patient['MainDicomTags']['PatientName'])
end


function OnStableStudy(studyId, tags, metadata)
   if (metadata['ModifiedFrom'] == nil and
       metadata['AnonymizedFrom'] == nil) then

      print('This study is now stable: ' .. studyId)
      
      -- The tags to be replaced
      local replace = {}
      replace['StudyDescription'] = 'Modified study'
      replace['StationName'] = 'My Medical Device'
      replace['0031-1020'] = 'Some private tag'

      -- The tags to be removed
      local remove = { 'MilitaryRank' }

      -- The modification command
      local command = {}
      command['Remove'] = remove
      command['Replace'] = replace

      -- Modify the entire study in one single call
      local m = RestApiPost('/studies/' .. studyId .. '/modify',
                            DumpJson(command, true))
      print('Modified study: ' .. m)
   end
end
