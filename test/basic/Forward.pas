program fwd;


function Test: int32; forward;

procedure Main;
var Ret: int32;
begin 
    Ret := test;
end;


function Test: int32; 
begin 
    exit(7);
end;


begin Main end.
