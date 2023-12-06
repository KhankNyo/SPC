program p;



procedure main;
var int8: int64;
    a: int8;        { should error here }
    integer: int32; { should not be an error (fpc does not give an error) }
begin 
    a := 0;
    int8 := 0;
    integer := 1;
end;

begin main end.
