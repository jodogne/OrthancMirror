function OnStoredInstance(instance, tags, metadata)
   --PrintRecursive(tags)
   PrintRecursive(metadata)
   print(metadata['RemoteAET'])
   return { 
      { "store", instance, "pacs" }, 
      { "delete", instance } 
   }
end
