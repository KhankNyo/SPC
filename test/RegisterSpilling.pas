


var
    test, sum, a, i: int32;
begin
    a := 1 + (2 + (3 + (4 + (5 + (6 + (7 + (8 + (9 + (10 
        + (11 + (12 + (13 + (14 + (15 + (16 + (17 + (18 + (19)))))))))
    )))))))));
    for i := 0 to 19 do 
        sum := sum + i;
    test := sum = a;
end.
