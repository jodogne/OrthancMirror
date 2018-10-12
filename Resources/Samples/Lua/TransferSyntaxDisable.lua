function IsDeflatedTransferSyntaxAccepted(aet, ip)
   return false
end

function IsJpegTransferSyntaxAccepted(aet, ip)
   return false
end

function IsJpeg2000TransferSyntaxAccepted(aet, ip)
   return false
end

function IsJpegLosslessTransferSyntaxAccepted(aet, ip)
   return false
end

function IsJpipTransferSyntaxAccepted(aet, ip)
   return false
end

function IsMpeg2TransferSyntaxAccepted(aet, ip)
   return false
end

function IsRleTransferSyntaxAccepted(aet, ip)
   return false
end

function IsUnknownSopClassAccepted(aet, ip)
   return false
end

print('All special transfer syntaxes are now disallowed')
