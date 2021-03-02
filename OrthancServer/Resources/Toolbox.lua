--[[ PrintRecursive(struct, [limit], [indent])   Recursively print arbitrary data. 
Set limit (default 100) to stanch infinite loops.
Indents tables as [KEY] VALUE, nested tables as [KEY] [KEY]...[KEY] VALUE
Set indent ("") to prefix each line:    Mytable [KEY] [KEY]...[KEY] VALUE
Source: https://gist.github.com/stuby/5445834#file-rprint-lua
--]]

function PrintRecursive(s, l, i) -- recursive Print (structure, limit, indent)
   l = (l) or 100;  -- default item limit
   i = i or "";     -- indent string
   if (l<1) then print "ERROR: Item limit reached."; return l-1 end;
   local ts = type(s);
   if (ts ~= "table") then print (i,ts,s); return l-1 end
   print (i,ts);           -- print "table"
   for k,v in pairs(s) do  -- print "[KEY] VALUE"
      l = PrintRecursive(v, l, i.."\t["..tostring(k).."]");
      if (l < 0) then break end
   end
   return l
end	




function _InitializeJob()
   _job = {}
end


function _AccessJob()
   return _job
end


function SendToModality(resourceId, modality, localAet)
   if resourceId == nil then
      error('Cannot send a nonexistent resource')
   end

   table.insert(_job, { 
                   Operation = 'store-scu', 
                   Resource = resourceId,
                   Modality = modality,
                   LocalAet = localAet
                })
   return resourceId
end


function SendToPeer(resourceId, peer)
   if resourceId == nil then
      error('Cannot send a nonexistent resource')
   end

   table.insert(_job, { 
                   Operation = 'store-peer', 
                   Resource = resourceId,
                   Peer = peer
                })
   return resourceId
end


function Delete(resourceId)
   if resourceId == nil then
      error('Cannot delete a nonexistent resource')
   end

   table.insert(_job, { 
                   Operation = 'delete', 
                   Resource = resourceId
                })
   return nil  -- Forbid chaining
end


function ModifyResource(resourceId, replacements, removals, removePrivateTags)
   if resourceId == nil then
      error('Cannot modify a nonexistent resource')
   end

   if resourceId == '' then
      error('Cannot modify twice an resource');
   end

   table.insert(_job, { 
                   Operation = 'modify', 
                   Resource = resourceId,
                   Replace = replacements, 
                   Remove = removals,
                   RemovePrivateTags = removePrivateTags 
                })

   return ''  -- Chain with another operation
end


function ModifyInstance(resourceId, replacements, removals, removePrivateTags)
   return ModifyResource(resourceId, replacements, removals, removePrivateTags)
end


-- This function is only applicable to individual instances
function CallSystem(resourceId, command, args)
   if resourceId == nil then
      error('Cannot execute a system call on a nonexistent resource')
   end

   if command == nil then
      error('No command was specified for system call')
   end

   table.insert(_job, { 
                   Operation = 'call-system', 
                   Resource = resourceId,
                   Command = command,
                   Arguments = args
                })

   return resourceId
end


print('Lua toolbox installed')
