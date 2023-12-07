program Pyramid;


procedure NextFrame(iter: int32);
var i : integer;
begin
    while iter > 0 do 
        iter := iter - 1; 

    for i := 0 to 40 do 
        writeln;
end;


var 
    waitTime: int32;
begin
    waitTime := 3000000;

    { what a throwback }
    while true do 
    begin 
        WriteLn('    *');
        WriteLn('   ***');
        WriteLn('  *****');
        WriteLn(' *******');
        WriteLn('*********');
        WriteLn(' *******'#10 +
                '  *****'#10 + 
                '   ***'#10 +
                '    *'
        );
        NextFrame(waitTime);

        WriteLn('     ');
        WriteLn('   ***');
        WriteLn(' ******');
        WriteLn('********');
        WriteLn('*********');
        WriteLn(' ********'#10 +
                '  *****'#10 + 
                '   ***'#10 +
                '    '
        );
        NextFrame(waitTime);

        WriteLn('     ');
        WriteLn('    *****');
        WriteLn('  ********');
        WriteLn('**********');
        WriteLn('***********');
        WriteLn('*********'#10 +
                ' ******'#10 + 
                '   **'#10 +
                '    '
        );
        NextFrame(waitTime);

        WriteLn('     ');
        WriteLn('  *******');
        WriteLn('**********');
        WriteLn('***********');
        WriteLn('***********');
        WriteLn('***********'#10 +
                ' ********'#10 + 
                '    **'#10 +
                '    '
        );
        NextFrame(waitTime);

        WriteLn('     ');
        WriteLn('***********');
        WriteLn('***********');
        WriteLn('***********');
        WriteLn('***********');
        WriteLn('***********'#10 +
                '***********'#10 + 
                '    '#10
        );
        NextFrame(waitTime);

        WriteLn('     ');
        WriteLn('    *****');
        WriteLn('  ********');
        WriteLn('**********');
        WriteLn('***********');
        WriteLn('*********'#10 +
                ' ******'#10 + 
                '   **'#10 +
                '    '
        );
        NextFrame(waitTime);

        WriteLn('     ');
        WriteLn('  *******');
        WriteLn('**********');
        WriteLn('***********');
        WriteLn('***********');
        WriteLn('***********'#10 +
                ' ********'#10 + 
                '    **'#10 +
                '    '
        );
        NextFrame(waitTime);
    end;
end.

