program log;



var gFail: Boolean = false;
    gCallCount: Integer = 0;


function failOnCall(callCount: Integer): Boolean;
begin gFail := true; exit(true) end;

function main: String;
begin 
    if false and failOnCall(1) then exit('false and fn');
    if true or failOnCall(2) 
    then exit('passed')
    else exit('true or fn');
end;




var errMsg: String;
begin 
    errMsg := main();
    if gFail 
    then writeln('Logical: lazy eval: ', gCallCount)
    else writeln('Logical: ', errMsg);
end.

