function IsDeflatedTransferSyntaxAccepted(aet, ip)
   return true
end

function IsJpegTransferSyntaxAccepted(aet, ip)
   return true
end

function IsJpeg2000TransferSyntaxAccepted(aet, ip)
   return true
end

function IsJpegLosslessTransferSyntaxAccepted(aet, ip)
   return true
end

function IsJpipTransferSyntaxAccepted(aet, ip)
   return true
end

function IsMpeg2TransferSyntaxAccepted(aet, ip)
   return true
end

function IsRleTransferSyntaxAccepted(aet, ip)
   return true
end

function IsUnknownSopClassAccepted(aet, ip)
   return true
end

print('All special transfer syntaxes are now accepted')
