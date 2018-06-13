# coding=UTF-8

import mrjson as j
import inspect

def raises( o, f, exc,details ):
  try:
    f(o)
    if len(o) > 100: o = o[:100]
    print("ERROR ", o, " didn't raise exception")
  except Exception as e:
    if len(o) > 100: o = o[:100]
    if type(e) != exc:
      print("ERROR",o," rose wrong exception",type(e),e)
    if str(e) != details:
      print("ERROR",o," rose wrong exception details: ",e,"expected",details)

def eq( a, b ):
  if a != b:
    cf = inspect.currentframe()
    print( "ERROR Line", cf.f_back.f_lineno, a, "!=", b )
    return -1
  return 0

print("Running tests...")
objs = [
-1, 12, 1,
-132123123123,
1.002, -1.31,
-312312312312.31,
2**31, 2**32, 2**32-1,
1337E40, 1.337E40, 3937e+43, 7e+3, 
7e-3, 1e-1, 1e-4,
[1, 2, 3, 4],
[1, 2, 3, [4,5,6,7,8]],
{"k1": 1, "k2": 2, "k3": 3, "k4": 4},
'\u273f\u2661\u273f',  # ✿♡✿
"\x00", "\x19", 
"afsd \x00 fdasf",
"\xe6\x97\xa5\xd1\x88",
"\xf0\x90\x8d\x86",
"\xf3\xbf\xbf\xbffadfadsfas",
"fadsfasd\xe6\x97\xa5\xd1\x88",
[[[[]]]] * 20,
[31337.31337, 31337.31337, 31337.31337, 31337.31337] * 10,
{'a': -12345678901234.56789012},
"fadfasd \\ / \r \t \n \b \fxx fadsfas",
"Räksmörgås اسامة بن محمد بن عوض بن لادن",
None, True,False,float('inf'),
[18446744073709551615, 18446744073709551615, 18446744073709551615],
]

# NaN != Nan so check str rep
o = j.loads( j.dumps( float('nan') ) )
eq( str(o), 'nan' )

for o in objs:
  try:
    eq( o, j.loads(j.dumps(o)) )
  except Exception as e:
    print( "ERROR",str(e), o )

raises( "NaNd", j.loads, ValueError, "JSON_BAD_IDENTIFIER" )  
raises( "[", j.loads, ValueError, "JSON_UNEXPECTED_END" )  
raises( "]", j.loads, ValueError, "JSON_STACK_UNDERFLOW" )  
raises( "{", j.loads, ValueError, "JSON_UNEXPECTED_END" )  
raises( "}", j.loads, ValueError, "JSON_STACK_UNDERFLOW" )  
raises( "[1,2,,]", j.loads, ValueError, "JSON_UNEXPECTED_CHARACTER," )  
raises( "[1,z]", j.loads, ValueError, "JSON_UNEXPECTED_CHARACTER" )  

raises( "["*(1024*1024), j.loads, ValueError, "JSON_STACK_OVERFLOW" )  

class JSONTest:
  def __json__(self):
    return '{"k":"v"}'
eq( j.dumps({u'key': JSONTest()}),'{"key":{"k":"v"}}')

print("Done")
