-- Sample Lua script that demonstrates how to extract DICOM tags
-- related to dose reports. In this example, the value of the DLP
-- (Dose Length Product), the value of the mean CTDI volume (Computed
-- Tomography Dose Index), and the number of "IntervalsRejected" are
-- extracted and printed. Furthermore, these values are saved as
-- metadata that is attached to their parent DICOM instance for
-- further processing by external software.


function ExploreContentSequence(instanceId, tags)
   if tags then
      for key, value in pairs(tags) do
         -- Recursive exploration
         ExploreContentSequence(instanceId, value['ContentSequence'])

         local concept = value['ConceptNameCodeSequence'] 
         local measure = value['MeasuredValueSequence']
         if concept and measure then

            local value = measure[1]['NumericValue']
            local code = concept[1]['CodeValue']

            if type(value) == 'string' and type(code) == 'string' then
               -- If the field contains the DLP, stores it as a metadata.
               -- "DLP" is associated with CodeValue 113838.
               -- ftp://medical.nema.org/medical/dicom/final/sup127_ft.pdf
               if code == '113838' then
                  print('DLP = ' .. value)
                  RestApiPut('/instances/' .. instanceId .. '/metadata/2001', value)
               end

               -- Extract the mean CTDI volume
               if code == '113830' then
                  print('CTDI = ' .. value)
                  RestApiPut('/instances/' .. instanceId .. '/metadata/2002', value)
               end

               -- Other values can be extracted here
            end
         end
      end
   end
end


function StoreTagToMetadata(instanceId, tags, name, metadata)
   if tags then
      for key, value in pairs(tags) do
         if type(value) ~= 'string' then
            -- Recursive exploration
            StoreTagToMetadata(instanceId, value, name, metadata)
         elseif key == name then
            print(name .. ' = ' .. value)
            if metadata then
               RestApiPut('/instances/' .. instanceId .. '/metadata/' .. metadata, value)
            end
         end
      end
   end
end


function OnStoredInstance(instanceId, tags)
   StoreTagToMetadata(instanceId, tags, 'IntervalsRejected', 2000)
   ExploreContentSequence(instanceId, tags['ContentSequence'])
end
