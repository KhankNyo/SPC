

function fib(a: uint32): uint32;
begin
    if a < 2 then
        Exit(a);
    Exit(fib(a - 1) + fib(a - 2));
end;


procedure die;
begin die end;



procedure main;
begin
    if 9227465 <> fib(35) then die;
end;


begin
    main;
end;
