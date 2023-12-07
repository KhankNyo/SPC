program FibonacciBenchmark;

function fib(a: uint32): uint32;
begin
    if a < 2 then
        Exit(a);
    Exit(fib(a - 1) + fib(a - 2));
end;


procedure main;
var return: int32;
begin
    return := fib(35);
    if 9227465 <> return 
    then writeln('failed: fib(35) = ', return)
    else writeln('passed: fib(35) = ', return);
end;


begin
    main;
end.
