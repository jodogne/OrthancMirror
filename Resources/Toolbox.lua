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


function SendToModality(instanceId, modality)
   if instanceId == nil then
      error('Cannot send a nonexistent instance')
   end

   table.insert(_job, { 
                   Operation = 'store-scu', 
                   Instance = instanceId,
                   Modality = modality 
                })
   return instanceId
end


function SendToPeer(instanceId, peer)
   if instanceId == nil then
      error('Cannot send a nonexistent instance')
   end

   table.insert(_job, { 
                   Operation = 'store-peer', 
                   Instance = instanceId,
                   Peer = peer
                })
   return instanceId
end


function Delete(instanceId)
   if instanceId == nil then
      error('Cannot delete a nonexistent instance')
   end

   table.insert(_job, { 
                   Operation = 'delete', 
                   Instance = instanceId
                })
   return nil  -- Forbid chaining
end


function ModifyInstance(instanceId, replacements, removals, removePrivateTags)
   if instanceId == nil then
      error('Cannot modify a nonexistent instance')
   end

   if instanceId == '' then
      error('Cannot modify twice an instance');
   end

   table.insert(_job, { 
                   Operation = 'modify', 
                   Instance = instanceId,
                   Replace = replacements, 
                   Remove = removals,
                   RemovePrivateTags = removePrivateTags 
                })
   return ''  -- Chain with another operation
end



print('Lua toolbox installed')
