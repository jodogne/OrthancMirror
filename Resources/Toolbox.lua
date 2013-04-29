--[[ rPrint(struct, [limit], [indent])   Recursively print arbitrary data. 
Set limit (default 100) to stanch infinite loops.
Indents tables as [KEY] VALUE, nested tables as [KEY] [KEY]...[KEY] VALUE
Set indent ("") to prefix each line:    Mytable [KEY] [KEY]...[KEY] VALUE
https://gist.github.com/stuby/5445834#file-rprint-lua
--]]
function rPrint(s, l, i) -- recursive Print (structure, limit, indent)
   l = (l) or 100; i = i or "";	-- default item limit, indent string
   if (l<1) then print "ERROR: Item limit reached."; return l-1 end;
   local ts = type(s);
   if (ts ~= "table") then print (i,ts,s); return l-1 end
   print (i,ts);           -- print "table"
   for k,v in pairs(s) do  -- print "[KEY] VALUE"
      l = rPrint(v, l, i.."\t["..tostring(k).."]");
      if (l < 0) then break end
   end
   return l
end	
