program AddBools;

var 
    a: boolean;
begin
    { should be ok }
    a := 1 + 3 = 4;

    { should be true }
    a := a or false;

    { should be false }
    a := true and false;

    // should error
    // a := 1;
end;
