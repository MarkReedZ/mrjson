## MrJSON

MrJSON is a JSON encoder and decoder written in C/C++ with bindings for Python 2.5+ and 3.  I needed faster performance for long strings and floats and MrJSON benchmarks at 2-3 times faster than the fastest python parsers for my use cases.

To install it just run Pip as usual:

```sh
    $ pip install mrjson
```

## Usage

May be used as a replacement for json:

```python
    >>> import mrjson as json
    >>> json.dumps([{"key": "value"}, 81, True])
    '[{"key":"value"},81,true]'
    >>> json.loads("""[{"key": "value"}, 81, true]""")
    [{'key': 'value'}, 81, True]
```

## Encoder option differences

**ensure_ascii** defaults to true in the base json module, but defaults to false here for space and performance reasons. 

**indent**,  **separators**, and **sortKeys** are not supported as pretty printing doesn't need the performance. Write an issue if you have a use case.

**allow_nan** is unsupported as NaN and infinity are supported by default

## Benchmarks		

Run bench.py to test MrJSON against some other modules - test your own files on your own machine as results can vary significantly. MrJSON does particularly well decoding long strings and floating point numbers thanks to intel's AVX2 instructions and [Milo Yip at Tencent](https://github.com/Tencent/rapidjson) for publishing C++ code implementing Florian Loitsch's float to string algorithms. 

#### Loads

Only 128 byte long strings. 

<img src="bench/png/str128-2.png" width="40%" />

Mostly floating point numbers - canada.json from [The Native JSON Benchmark](https://github.com/miloyip/nativejson-benchmark), the fastest C++ JSON parser comes in at 7.9 milliseconds on this machine for comparison

<img src="bench/png/canada-loads.png" width="40%" />

A single tweet from twitter - twit.json

<img src="bench/png/twit-loads.png" width="40%" />

citm_catalog.json from [The Native JSON Benchmark](https://github.com/miloyip/nativejson-benchmark)

<img src="bench/png/citm-catalog-loads.png" width="40%" />

#### Dumps

Only 128 byte long strings. 

<img src="bench/png/str128-dumps.png" width="40%" />

Mostly floating point numbers - canada.json from [The Native JSON Benchmark](https://github.com/miloyip/nativejson-benchmark)

<img src="bench/png/canada-dumps.png" width="40%" />

A single tweet from twitter - twit.json

<img src="bench/png/twit-dumps.png" width="40%" />

citm_catalog.json from [The Native JSON Benchmark](https://github.com/miloyip/nativejson-benchmark)

<img src="bench/png/citm-catalog-dumps.png" width="40%" />

