$files = Get-ChildItem -Recurse -Include *.go,*.cpp -File |
    Where-Object {
        $_.FullName -notmatch '\\dll\\detours' -and
        $_.FullName -notmatch '\\dll\\build'
    }

$lineCount = 0
foreach ($file in $files) {
    $lineCount += (Get-Content $file.FullName -Raw).Split("`n").Count
}

"Total lines: $lineCount"
