

function fib(a: uint32): uint32;
begin
    if a < 2 then
        Exit(a);
    Exit(fib(a - 1) + fib(a - 2));
end;


procedure main;
var
    a: uint32;
begin
    a := fib(35);
end;


begin
    main;
end;
