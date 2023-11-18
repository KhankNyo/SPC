


var
    test, sum, a: uint32;
begin
    { address 2 on stack }
    a := (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + 1)))))))))))))))))))); 

    { address 1 on stack }
    sum := 0;
    for sum := 0 to 20 do
    begin end;

    { address 0 on stack }
    test := sum = a;
end.
