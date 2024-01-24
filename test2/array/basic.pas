program arr;

const ArrayCount = 4;
var i: integer;
    a: array [0..ArrayCount] of integer;
begin
    for i := 0 to ArrayCount 
    do a[i] := i;

    for i := 0 to ArrayCount 
    do writeln(a[i]);
end.

