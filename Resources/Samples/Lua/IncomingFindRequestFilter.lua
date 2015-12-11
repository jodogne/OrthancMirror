-- This example solves the following problem:
-- https://groups.google.com/d/msg/orthanc-users/PLWKqVVaXLs/n_0x4vKhAgAJ

function IncomingFindRequestFilter(source, origin)
   -- First display the content of the C-Find query
   PrintRecursive(source)
   PrintRecursive(origin)

   -- Remove the "PrivateCreator" tag from the query
   local v = source
   v['5555,0010'] = nil

   return v
end
