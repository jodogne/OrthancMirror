function OnStoredInstance(tags, instance)
  PrintRecursive(tags)
  return { 
    { "store", instance, "pacs" }, 
    { "delete", instance } 
  }
end
