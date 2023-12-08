program t;


procedure main; 
var A, B: integer;
begin 
    A := 7; 
    B := 3; 
    if A + B > 8 then begin
        A := B;
        B := A;
    end else begin 
        B := A;
        A := B;
    end;
    writeln('A: ', A, '; B: ', B);
end;

begin main end.
