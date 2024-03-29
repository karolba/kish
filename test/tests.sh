#!/bin/bash
KISH=${1:-kish}
tmpfile=$(mktemp -t kish-test.XXXXXXXXXX)
passed=0 failed=0
nl='
'
ktest() {
        stdout= stderr= errcode= reason=
        stdout=$("$KISH" -c "$1" 2> "$tmpfile")
        errcode=$?
        stderr=$(cat "$tmpfile")
        if [ $# = 4 ]; then
            expected_errcode=$4
        else
            expected_errcode=0
        fi
        if [ $errcode != $expected_errcode ]; then
                reason="$reason  \$? = $errcode$nl"
        fi
        if [ "$stdout" != "$2" ]; then
                reason="  stdout:$nl"
                reason+="    expected: [$2]$nl"
                reason+="    but got:  [$stdout]$nl"
        fi
        if [ "$stderr" != "$3" ]; then
                reason="  stderr:$nl"
                reason+="    expected: [$3]$nl"
                reason+="    but got:  [$stderr]$nl"
        fi
        if [ "$reason" ]; then
                printf "TEST FAILED: '%s'\\n" "$1"
                printf '%s' "$reason"
                failed=$(echo "1+$failed" | bc)
        else
                passed=$(echo "1+$passed" | bc)
        fi
        echo "DEBUG: Passed=$passed, failed=$failed"
}
trap 'echo Passed tests: $passed, failed tests: $failed; rm -f $tmpfile' EXIT

export LANG=C

# ktest usage: 
#  ktest <kish commans> [expected stdout] [expected stderr] [expexted $?]
ktest 'true'
ktest 'false' '' '' 1
ktest 'true | true'
ktest 'false | false' '' '' 1
ktest 'true || true'
ktest 'true || false'
ktest 'false || true'
ktest 'false; echo $?' '1'
ktest 'echo a | cat && echo b | cat; echo c | cat && echo d | cat' $'a\nb\nc\nd'
ktest 'false || true || false; echo "$?" | cat' '0'
ktest 'true && false && true' '' '' 1
ktest 'false || echo ok' 'ok'
ktest 'echo 123 > /dev/stderr | cat' '' '123'
ktest 'echo "abc\\def"' 'abc\def'
ktest 'a=1; echo a=$a' 'a=1'
ktest 'echo '\"\'\"\'\"\' \'\"
ktest 'echo "abc\\\\def"' 'abc\\def'
ktest 'a=1 b=1\ 2; printf "%s|" "$a"$b"$b"'"'\$b'" '11|21 2$b|'
ktest "printf '%s\\n' 'abc"'\\'"def'" 'abc\\def'
ktest '{ echo ab; echo cd; } | rev | tac' $'dc\nba'
ktest '{ echo ab; echo cd; } >/dev/stderr' '' $'ab\ncd'
ktest '{ echo ab; echo cd; } >/dev/stderr | cat' '' $'ab\ncd'
ktest 'a="""1"; { echo $a; } | { a=2; cat; echo $a; }' $'1\n2'
ktest 'echo 12 | { cat; echo 34; } | rev' $'21\n43'
ktest 'echo 12 | if true; then rev; fi' '21'
ktest 'echo 12 | if false; then cat; fi'
ktest 'a="echo a  b" b="rev"; $a | $b' 'b a'
ktest 'var=if; $var' '' 'if: No such file or directory' 127
ktest '{ echo x; } > /dev/stderr' '' 'x'
ktest '{ echo x; } > /dev/stdout' 'x'
ktest 'true > /dev/stdout'
ktest 'if true; then { echo a; echo b > /dev/stderr; echo c; } > /dev/null; fi' '' 'b'
ktest 'if true; then echo x; fi > /dev/stdout' 'x'
ktest 'a=true; if $a; then echo "[$a]"; fi' '[true]'
ktest 'a=true; if $a; then echo "[$a]"; fi | cat' '[true]'
ktest '{ true; } true' '' 'Syntax error: {}-lists cannot take arguments' 1
ktest 'a=1 {' '' '{: No such file or directory' 127
ktest 'UNIQUE_ENV_VAR=123 OTHER_VAR=456 env | grep "UNIQUE_ENV_VAR"' 'UNIQUE_ENV_VAR=123'
ktest ">'${tmpfile}-creation' && < '${tmpfile}-creation' && rm '${tmpfile}-creation'"
ktest '>>/dev/stdin _var_=1 </dev/stdin && echo $_var_' '1'
ktest '>>/dev/stdin var=1 </dev/stdin | if false; then true; fi'
ktest 'if false; then echo 1; else echo 2; fi' '2'
ktest '
    a=1
    if [ "$a" = 1 ]; then
        echo $a
    fi | if true; then
        a=2
        cat
        echo $a
    fi
    ' $'1\n2'
ktest '
    a="echo a"
    if $a 1 && false; then 
        $a
    else
        $a "$a"
    fi
    ' $'a 1\na echo a'
ktest '
    a="echo a"
    if $a 1 && false; then
        $a
    else
        $a "$a"
    fi | cat
    ' $'a 1\na echo a'
ktest '
    a="echo a"
    {
        {
            if $a 1 && false; then
                $a
            else
                $a "$a"
            fi
        } | cat
    }
    ' $'a 1\na echo a'
ktest '
    if false; then
        echo 1
    elif true; then
        echo 2
    else
        echo 3
    fi' 2
ktest '
    if true; then
        echo 1
    elif true; then
        echo 2
    else
        echo 3
    fi' 1
ktest '
    if false; then
        echo 1
    elif false; then
        echo 2
    else
        echo 3
    fi' 3
ktest '
    four=4
    if false; then
        echo 1
    elif false; then
        echo 2
    elif false; then
        echo 3
    elif false; true; then
        echo $four
    else
        echo 5
    fi' 4
ktest '
    if false; then
        echo 1
    elif false; then
        echo 2
    else
        echo 3
    fi | cat' 3
ktest '
    four=4
    if false; then
        echo 1
    elif false; then
        echo 2
    elif false; then
        echo 3
    elif true; then
        echo $four
    else
        echo 5
    fi | cat' 4
ktest '
    a=true
    while $a; do
        echo in loop
        a=false
    done
    ' 'in loop'
ktest '
    a=false
    until $a; do
        echo in loop
        a=true
    done
    ' 'in loop'
ktest '
    a=true
    while $a; do
        echo in $a loop
        a=false
    done
    ' 'in true loop'
ktest 'a=false; while $a; do echo test; done'
ktest 'a=true; until $a; do echo test; done'
ktest 'a=true
    while $a; do
        echo $a
        a=false
    done
    echo $a | cat
    ' $'true\nfalse'
ktest 'a=false
    until $a; do
        echo $a
        a=true
    done
    echo $a | cat
    ' $'false\ntrue'
ktest 'a=true; while $a; do a=false; echo test; done > /dev/stderr' '' 'test'
ktest 'a=true; while $a; do echo "$a"; a=false; done > /dev/stderr | rev' '' 'true'
ktest 'then' '' "Syntax error: Unexpected token 'then'" 1
ktest 'elif' '' "Syntax error: Unexpected token 'elif'" 1
ktest 'else' '' "Syntax error: Unexpected token 'else'" 1
ktest 'fi' '' "Syntax error: Unexpected token 'fi'" 1
ktest 'do' '' "Syntax error: Unexpected token 'do'" 1
ktest 'done' '' "Syntax error: Unexpected token 'done'" 1
ktest 'if true; then echo x; fi arg' '' "Syntax error: 'fi' cannot take arguments" 1
ktest 'if true; then echo x; fi { arg; }' '' "Syntax error: 'fi' cannot take arguments" 1
ktest 'while false; do x; done arg' '' "Syntax error: 'done' cannot take arguments" 1
ktest 'while false; do x; done { arg; }' '' "Syntax error: 'done' cannot take arguments" 1
ktest 'until false; do x; done arg' '' "Syntax error: 'done' cannot take arguments" 1
ktest 'until false; do x; done { arg; }' '' "Syntax error: 'done' cannot take arguments" 1
ktest '! cmd1 | ! cmd2' '' "Syntax error: Unexpected token '!'" 1
ktest '! true' '' '' 1
ktest '! false' '' '' 0
ktest '! ! true' '' '' 0
ktest '! ! ! ! true' '' '' 0
ktest '! true | true' '' '' 1
ktest '! false | false' '' '' 0
ktest 'if ! true || ! true || ! ! false; then echo wrong; fi' '' ''
ktest 'for x in; do echo test; done' ''
ktest 'for x in; do echo test; done | cat' ''
ktest 'stdin=/dev/stdin; for x in; do echo test; done < $stdin' ''
ktest 'for x in "1 2" 3; do echo $x; done' $'1 2\n3'
ktest 'for x in "1 2" 3; do echo $x; done | cat' $'1 2\n3'
ktest 'for x in "1 2" 3; do echo $x; done | cat' $'1 2\n3'
ktest $'for x in "1 2" 3\n\n do\n\n\n echo $x\n\n\n done | cat' $'1 2\n3'
ktest 'a="1 2" b="3 4"; for x in $a"$b"; do echo $x; done' $'1\n23 4'
ktest 'for x in 1 2 3; do echo -$x- >> /dev/stderr; echo "[$x]"; done | grep -v 1' $'[2]\n[3]' $'-1-\n-2-\n-3-'
ktest 'for x do echo $x; done' ''
ktest ':' ''
ktest 'false; :' ''
ktest ': should ignore arguments' ''
ktest 'echo $(echo x)' x
ktest 'echo "$(echo x)"' x
ktest 'echo _"$(echo x)_"' _x_
ktest 'echo _$(echo x >/dev/stderr)_' __ x
ktest 'echo $(echo $(echo $(echo x)))' x
ktest 'echo $(echo x) >/dev/stderr' '' x
ktest 'echo asda | { a=$(read a; echo "[$a]"); echo "{$a}"; }' '{[asda]}'
ktest 'a=$(false)' '' '' 1
ktest 'a=$(true)' '' '' 0
ktest 'if a=$(true); then echo ok; fi' ok
ktest 'if a=$(false); then echo ok; else echo not-ok; fi' not-ok
ktest 'printf "[%s]\\n" "1" "" "2"' $'[1]\n[]\n[2]'
ktest "echo '' '' '' 1" '   1'
ktest "echo 1 '' '' 2 2" '1   2 2'
ktest "printf %s, '' '' 2 2" ',,2,2,'
ktest 'f() { echo function-invocation; }; f' 'function-invocation'
ktest 'f() { echo complex function invocation in a pipe; }; echo $(f | cat) | cat' 'complex function invocation in a pipe'
ktest 'echo $(f(){ echo in command subst; }; f)' 'in command subst'
ktest 'echo $(f(){ echo in command subst; }; f > /dev/null | f)' 'in command subst'
ktest 'f() { echo 1; }; f () { echo 2; } | f(){ echo 3; }; f' 1
ktest 'f() { echo [$#]; }; f 1 2; echo $(f 1 2 3)' $'[2]\n[3]'

[ $failed -eq 0 ]
