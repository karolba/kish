#!/bin/bash
KISH=${1:-./kish}
tmpfile=$(mktemp -t kish-test.XXXXXXXXXX)
declare -i passed=0 failed=0
ktest() {
	local stdout= stderr= errcode= reason=
	stdout=$("$KISH" -c "$1" 2> "$tmpfile")
	errcode=$?
	stderr=$(cat "$tmpfile")
	if [[ $errcode -ne ${4:-0} ]]; then
		reason+="  \$? = $errcode"$'\n'
	fi
	if [[ "$stdout" != "$2" ]]; then
		reason+=$'  stdout:\n'
		reason+="    expected: [$2]"$'\n'
		reason+="    but got:  [$stdout]"$'\n'
	fi
	if [[ "$stderr" != "$3" ]]; then
		reason+=$'  stderr:\n'
		reason+="    expected: [$3]"$'\n'
		reason+="    but got:  [$stderr]"$'\n'
	fi
	if [[ $reason ]]; then
		printf "TEST FAILED: '%s'\\n" "$1"
		printf '%s' "$reason"
		(( failed += 1 ))
	else
		(( passed += 1 ))
	fi
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
ktest 'echo "abc\\def"' 'abc\def'
#ktest 'echo "abc\\\\def"' 'abc\def' # TODO: '\\' -> '\' in echo
ktest 'a=1 b=1\ 2; printf "%s|" "$a"$b"$b"'"'\$b'" '11|21 2$b|'
ktest "printf '%s\\n' 'abc"'\\'"def'" 'abc\\def'
ktest '{ echo ab; echo cd; } | rev | tac' $'dc\nba'
ktest '{ echo ab; echo cd; } >/dev/stderr' '' $'ab\ncd'
ktest 'a="""1"; { echo $a; } | { a=2; cat; echo $a; }' $'1\n2'
ktest 'a="""1"; if [ "$a" = 1 ]; then echo $a; fi | if true; then a=2; cat; echo $a; fi' $'1\n2'
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
ktest 'if false; then :; fi test' '' 'Syntax error: fi cannot take arguments' 1
ktest '{ true; } true' '' 'Syntax error: {}-lists cannot take arguments' 1
ktest 'a=1 {' '' '{: No such file or directory' 127
ktest 'UNIQUE_ENV_VAR=123 OTHER_VAR=456 env | grep "UNIQUE_ENV_VAR"' 'UNIQUE_ENV_VAR=123'
ktest ">'${tmpfile}-creation' && < '${tmpfile}-creation' && rm '${tmpfile}-creation'"
ktest '>>/dev/stdin _var_=1 </dev/stdin && echo $_var_' '1'
ktest '>>/dev/stdin var=1 </dev/stdin | if false; then true; fi'