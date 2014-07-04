function OnStoredInstance(instance, tags, metadata)
   --PrintRecursive(tags)
  PrintRecursive(metadata)
  return { 
    { "store", instance, "pacs" }, 
    { "delete", instance } 
  }
end
