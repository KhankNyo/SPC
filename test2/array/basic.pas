program arr;

const ArrayCount = 4;
type IntegerArray = array[0..ArrayCount] of integer;
var i: integer;
    a: IntegerArray;
begin
    for i := 0 to ArrayCount 
    do a[i] := i;

    for i := 0 to ArrayCount 
    do writeln(a[i]);

    writeln(
        'Array size: ', sizeof(IntegerArray),
        #10'Element count: ', sizeof(IntegerArray) / sizeof(IntegerArray[0])
    );
end.

