-- This sample shows how to call a remote Web service whenever an
-- instance is received by Orthanc. For this sample to work, you have
-- to start the "CallWebService.js" script next to this file using
-- NodeJs.

-- Download and install the JSON module for Lua by Jeffrey Friedl
-- http://regex.info/blog/lua/json
JSON = (loadstring(HttpGet('http://regex.info/code/JSON.lua'))) ()

SetHttpCredentials('alice', 'alicePassword')

function OnStoredInstance(instanceId, tags, metadata)
   -- Build the POST body
   local info = {}
   info['InstanceID'] = instanceId
   info['PatientName'] = tags['PatientName']
   info['PatientID'] = tags['PatientID']

   -- Send the POST request
   local answer = HttpPost('http://127.0.0.1:8000/', JSON:encode(info))

   -- The answer equals "ERROR" in case of an error
   print('Web service called, answer received: ' .. answer)
end
