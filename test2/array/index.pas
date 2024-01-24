program index;

var a: array['a'..'z'] of char;
    i: char;
begin
    for i := 'a' to 'z' 
    do a[i] := i;

    writeln;
    for i := 'a' to 'z'
    do write(a[i], ', ');
    writeln;

end.
