## MrJSON

MrJSON is a JSON encoder and decoder written in C/C++ with bindings for Python 2.5+ and 3.  I needed faster performance for long strings and floats and MrJSON benchmarks at 2-3 times faster than the fastest python parsers for my use cases.

To install it just run Pip as usual:

```sh
    $ pip install mrjson
```

## Usage

May be used as a drop in replacement for most other JSON parsers for Python:

```python
    >>> import mrjson
    >>> mrjson.dumps([{"key": "value"}, 81, True])
    '[{"key":"value"},81,true]'
    >>> mrjson.loads("""[{"key": "value"}, 81, true]""")
    [{'key': 'value'}, 81, True]
```

## Encoder options

### ensure_ascii

Note that this defaults to true in the base json module, but now defaults to false for space and performance reasons. When true the output is ASCII with unicode characters embedded as \uXXXX and when false the output is a UTF-8 unicode string.

## Benchmarks		

Run bench.py to test MrJSON against some other modules - add your own json files and test on your own machine. MrJSON does particularly well with long strings and floating point data. 

str128.json contains only 128 byte long strings

<img src="bench/png/str128.png?1" width="40%" />



```
Test chatt1r.json loads
   6.73860777169466e-05      json
   7.03359255567193e-05      simplejson
   7.519498467445374e-05     rapidjson
   5.598808638751507e-05     ujson
   3.799807745963335e-05     u2json
   3.725104033946991e-05     mrjson

Test good-twitter.json loads
   0.05545995500870049       json
   0.05261462996713817       simplejson
   0.05022532900329679       rapidjson
   0.038334943004883826      ujson
   0.03875326598063111       u2json
   0.036198586924001575      mrjson

Test canada.json loads
   0.05989120702724904       json
   0.062270147958770394      simplejson
   0.06638678594026715       rapidjson
   0.028981538955122232      ujson
   0.02978257299400866       u2json
   0.016870283987373114      mrjson

Test citm_catalog.json loads
   0.011749677010811865      json
   0.015783209004439414      simplejson
   0.015123145072720945      rapidjson
   0.0121881109662354        ujson
   0.012583973002620041      u2json
   0.012809511972591281      mrjson

Test str128.json loads
   0.0004310370422899723     json
   0.0006197909824550152     simplejson
   0.0009168480755761266     rapidjson
   0.0007450750563293695     ujson
   0.00018622400239109993    u2json
   0.0001710739452391863     mrjson

Test twitter.json loads
   0.006027659052051604      json
   0.005926289944909513      simplejson
   0.007613075082190335      rapidjson
   0.005504615022800863      ujson
   0.005361802992410958      u2json
   0.005703269969671965      mrjson
```

### Encoding JSON

```
Test chatt1r.json dumps
   5.583604797720909e-05     json
   7.539801299571991e-05     simplejson
   5.188398063182831e-05     rapidjson
   2.4904031306505203e-05    ujson
   2.5484943762421608e-05    u2json
   2.1144980564713478e-05    mrjson

Test good-twitter.json dumps
   0.04047138593159616       json
   0.049327224027365446      simplejson
   0.023623212007805705      rapidjson
   0.024509022943675518      ujson
   0.024716764921322465      u2json
   0.015038975980132818      mrjson

Test canada.json dumps
   0.10872392205055803       json
   0.14630893198773265       simplejson
   0.026365612051449716      rapidjson
   0.029522896045818925      ujson
   0.031309623969718814      u2json
   0.011635600938461721      mrjson

Test citm_catalog.json dumps
   0.012513294932432473      json
   0.02278400701470673       simplejson
   0.005273798014968634      rapidjson
   0.005882436991669238      ujson
   0.006042705965228379      u2json
   0.0024987950455397367     mrjson

Test str128.json dumps
   0.0010118939680978656     json
   0.0011141080176457763     simplejson
   0.0012484139297157526     rapidjson
   0.000511158024892211      ujson
   0.0005099490517750382     u2json
   0.0004787800135090947     mrjson

Test twitter.json dumps
   0.005095881060697138      json
   0.005828171968460083      simplejson
   0.0033260660711675882     rapidjson
   0.00311158609110862       ujson
   0.0031881609465926886     u2json
   0.0021164900390431285     mrjson
```

