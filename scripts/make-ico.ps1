$source = "c:\dev\loudness-leveler\icon.png"
$dest = "c:\dev\loudness-leveler\icon.ico"

Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($source)
$bmp = New-Object System.Drawing.Bitmap(256, 256)
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$gfx.DrawImage($img, 0, 0, 256, 256)
$stream = New-Object System.IO.MemoryStream
$bmp.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
$pngBytes = $stream.ToArray()

$icoStream = [System.IO.File]::Create($dest)
$writer = New-Object System.IO.BinaryWriter($icoStream)

# ICONDIR
$writer.Write([Int16]0)
$writer.Write([Int16]1) 
$writer.Write([Int16]1)

# ICONDIRENTRY
$writer.Write([byte]0)
$writer.Write([byte]0)
$writer.Write([byte]0)
$writer.Write([byte]0)
$writer.Write([Int16]1)
$writer.Write([Int16]32)
$writer.Write([int]$pngBytes.Length)
$writer.Write([int]22)

$writer.Write($pngBytes)
$writer.Close()
$gfx.Dispose()
$bmp.Dispose()
$img.Dispose()
