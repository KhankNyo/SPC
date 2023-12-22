program Tricky;


type FnPtr = function: Integer;


function A: Integer;
begin 
    exit(5);
end;

function B: FnPtr;
begin 
    exit(@A);
end;

procedure main;
begin 
    writeln(B()());
end;

begin main end.
