program Variable;

procedure t;
var 
    int8: int64;        { no error, int8 is a variable in this scope, but still a type at global }
    a: int8;            { error: int8 is not a type in this scope }
    integer: integer;   { no error, integer is a variable in this scope, but a type at global }
begin end;


begin t end.

