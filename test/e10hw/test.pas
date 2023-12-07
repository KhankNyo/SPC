program t;


procedure die; begin die; end;

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
    A := A;
    B := B;

    // call die to view stack content which contains A and B
    die;
end;

begin main end.
