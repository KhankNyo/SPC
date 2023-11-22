

function fib(a: uint32): uint32;
begin
    if a < 2 then
        Exit(a);
    Exit(fib(a - 1) + fib(a - 2));
end;


procedure die;
begin die end;



procedure main;
var
    a: uint32;
begin
    if fib(35) = 9227465 then
        Exit
    else die;
end;


begin
    main;
end;
