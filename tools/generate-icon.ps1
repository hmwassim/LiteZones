# Generates src/litezones/icon.ico with 16/32/48px 32-bit entries.
# Design: transparent background, blue rounded-rect, white "zones" motif (left column + top/bottom right).
param(
    [string]$OutFile = (Join-Path $PSScriptRoot "..\src\litezones\icon.ico")
)

function TestZoneMotif([double]$x, [double]$y) {
    # Normalized coordinates, 0..1. Returns 1 (white zone), 0 (blue bg), or -1 (outside rounded rect).
    $m = 0.10
    $left = 0.12; $right = 0.42
    $top = 0.12;  $bottom = 0.88
    $midGap = 0.46
    $rightEdge = 0.88

    $inRounded = $false
    $hw = 0.5 - $m
    $hh = 0.5 - $m
    $cx = 0.5; $cy = 0.5
    $r = 0.12
    $dx = [Math]::Max([Math]::Abs($x - $cx) - ($hw - $r), 0.0)
    $dy = [Math]::Max([Math]::Abs($y - $cy) - ($hh - $r), 0.0)
    $dist = [Math]::Sqrt($dx * $dx + $dy * $dy) - $r
    if ($dist -le 0.0) { $inRounded = $true }

    if (-not $inRounded) { return -1 }

    if (($x -ge $left -and $x -le $right -and $y -ge $top -and $y -le $bottom) -or
        ($x -ge $midGap -and $x -le $rightEdge -and $y -ge $top -and $y -le 0.48) -or
        ($x -ge $midGap -and $x -le $rightEdge -and $y -ge 0.52 -and $y -le $bottom)) {
        return 1
    }
    return 0
}

function New-IconImageBytes([int]$size) {
    # Returns byte[] of BITMAPINFOHEADER + XOR (bottom-up BGRA) + AND mask.
    $ss = 4  # supersampling factor
    $xr = New-Object 'System.Collections.Generic.List[byte]'

    $xor = New-Object 'byte[]' ($size * $size * 4)
    $idx = 0
    for ($py = $size - 1; $py -ge 0; $py--) {
        for ($px = 0; $px -lt $size; $px++) {
            $r = 0; $g = 0; $b = 0; $a = 0
            for ($sy = 0; $sy -lt $ss; $sy++) {
                for ($sx = 0; $sx -lt $ss; $sx++) {
                    $x = ($px + ($sx + 0.5) / $ss) / $size
                    $y = ($py + ($sy + 0.5) / $ss) / $size
                    $v = TestZoneMotif $x $y
                    if ($v -gt 0) {
                        $r += 255; $g += 255; $b += 255; $a += 255
                    }
                    elseif ($v -eq 0) {
                        $r += 0x2D; $g += 0x7D; $b += 0xD2; $a += 255
                    }
                }
            }
            $n = $ss * $ss
            $xor[$idx++] = [byte][Math]::Round($b / $n)
            $xor[$idx++] = [byte][Math]::Round($g / $n)
            $xor[$idx++] = [byte][Math]::Round($r / $n)
            $xor[$idx++] = [byte][Math]::Round($a / $n)
        }
    }

    # BITMAPINFOHEADER (40 bytes)
    $bh = New-Object 'System.Collections.Generic.List[byte]'
    $bh.AddRange([BitConverter]::GetBytes([int32]40))
    $bh.AddRange([BitConverter]::GetBytes([int32]$size))
    $bh.AddRange([BitConverter]::GetBytes([int32]($size * 2)))
    $bh.AddRange([BitConverter]::GetBytes([int16]1))
    $bh.AddRange([BitConverter]::GetBytes([int16]32))
    $bh.AddRange([BitConverter]::GetBytes([int32]0))
    $bh.AddRange([BitConverter]::GetBytes([int32]($size * $size * 4)))
    $bh.AddRange([BitConverter]::GetBytes([int32]0))
    $bh.AddRange([BitConverter]::GetBytes([int32]0))
    $bh.AddRange([BitConverter]::GetBytes([int32]0))
    $bh.AddRange([BitConverter]::GetBytes([int32]0))

    # AND mask: 1bpp, rows padded to 4 bytes, all zeros (opaque via alpha)
    $andRowBytes = [Math]::Ceiling($size / 8.0) * 4
    $andMask = New-Object 'byte[]' ([int]($andRowBytes * $size))

    $result = New-Object 'System.Collections.Generic.List[byte]'
    $result.AddRange($bh)
    $result.AddRange($xor)
    $result.AddRange($andMask)
    return ,$result.ToArray()
}

$sizes = 16, 32, 48
$images = @{}
$total = 0
foreach ($s in $sizes) {
    $images[$s] = New-IconImageBytes $s
    $total += $images[$s].Length
}

$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($ms)

# ICONDIR
$bw.Write([int16]0)          # reserved
$bw.Write([int16]1)          # type: icon
$bw.Write([int16]$sizes.Count)

# Entries
$offset = 6 + 16 * $sizes.Count
foreach ($s in $sizes) {
    $bw.Write([byte]$s)      # width (0 means 256)
    $bw.Write([byte]$s)      # height
    $bw.Write([byte]0)       # color count
    $bw.Write([byte]0)       # reserved
    $bw.Write([int16]1)      # planes
    $bw.Write([int16]32)     # bit count
    $bw.Write([int32]$images[$s].Length)
    $bw.Write([int32]$offset)
    $offset += $images[$s].Length
}

# Image data
foreach ($s in $sizes) {
    $bw.Write([byte[]]$images[$s])
}

$bw.Flush()
$bytes = $ms.ToArray()
if ([System.IO.Path]::IsPathRooted($OutFile)) {
    $full = $OutFile
}
else {
    $full = Join-Path $PSScriptRoot $OutFile
}
$dir = Split-Path $full -Parent
if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
[System.IO.File]::WriteAllBytes($full, $bytes)
$bw.Dispose()
$ms.Dispose()
Write-Output "Wrote $full ($($bytes.Length) bytes)"
