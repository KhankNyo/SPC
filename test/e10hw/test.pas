program t;


procedure main; 
var 
    B, A: integer;
begin 
    A := 2;
    B := 5;
    if A + B > 7 then begin
        A := B;
        B := A;
    end else begin
        B := A * B;
        A := B + 5;
    end;
    writeln('A: ', A, '; B: ', B);
end;

begin main end.
