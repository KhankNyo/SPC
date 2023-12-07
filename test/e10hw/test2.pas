program t;


procedure die; begin die; end;



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
    { look at stack dump for A and B }
    die;
end;

begin main end.
