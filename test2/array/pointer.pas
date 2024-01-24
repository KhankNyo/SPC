program ElementPointer;


var a: array[0..3] of integer;
    i: integer;
    p: ^integer;
begin
    p := @(a[2]);
    for i := 0 to 3 
    do a[i] := i;

    p^ := (a[3]);

    for i := 0 to 3 
    do writeln(a[i]);
end.
