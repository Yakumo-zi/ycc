#!/bin/bash
cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
EOF
assert() {
  expected="$1"
  input="$2"

  ./ycc "$input" > tmp.s || exit
  gcc -static -o tmp tmp.s tmp2.o
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 1 '{ return 1>=1; }'
assert 0 '{ return 1>=2; }'

assert 3 '{ int a; a=3; return a; }'
assert 3 '{ int a=3; return a; }'
assert 8 '{ int a=3; int z=5; return a+z; }'

assert 3 '{ int a=3; return a; }'
assert 8 '{ int a=3; int z=5; return a+z; }'
assert 6 '{ int a; int b; a=b=3; return a+b; }'
assert 3 '{ int foo=3; return foo; }'
assert 8 '{ int foo123=3; int bar=5; return foo123+bar; }'

assert 1 '{ return 1; 2; 3; }'
assert 2 '{ 1; return 2; 3; }'
assert 2 '{ if (2-1) return 2; return 3; }'
assert 4 '{ if (0) { 1; 2; return 3; } else { return 4; } }'
assert 3 '{ if (1) { 1; 2; return 3; } else { return 4; } }'

assert 55 '{ int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }'
assert 3 '{ for (;;) return 3; return 5; }'

assert 10 '{ int i=0; while(i<10) i=i+1; return i; }'

assert 3 '{ {1; {2;} return 3;} }'

assert 10 '{ int i=0; while(i<10) i=i+1; return i; }'
assert 55 '{ int i=0; int j=0; while(i<=10) {j=i+j; i=i+1;} return j; }'

assert 3 '{ int x=3; return *&x; }'
assert 3 '{ int x=3; int *y=&x; int **z=&y; return **z; }'
assert 5 '{ int x=3; int y=5; return *(&x+1); }'
assert 3 '{ int x=3; int y=5; return *(&y-1); }'
assert 5 '{ int x=3; int y=5; return *(&x-(-1)); }'
assert 5 '{ int x=3; int *y=&x; *y=5; return x; }'
assert 7 '{ int x=3; int y=5; *(&x+1)=7; return y; }'
assert 7 '{ int x=3; int y=5; *(&y-2+1)=7; return x; }'
assert 5 '{ int x=3; return (&x+2)-&x+3; }'
assert 8 '{ int x, y; x=3; y=5; return x+y; }'
assert 8 '{ int x=3, y=5; return x+y; }'
assert 3 '{ return ret3(); }'
assert 5 '{ return ret5(); }'

echo OK
