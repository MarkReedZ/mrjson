import timeit, codecs

reqs = """
python setup.py install
pip install simplejson
pip install ujson
pip install python-rapidjson
pip install mrpacker
pip install pysimdjson
"""

setup = u'''
import codecs, json, mrpacker
import ZZZ
f = codecs.open( "bench/json/YYY", "rb", encoding="utf-8")
s = f.read()
f.close()
obj = json.loads(s)
packed = mrpacker.pack(obj)
'''
mods = ["json", "simplejson", "rapidjson", "ujson", "mrjson", "simdjson"]
tsts = ["str128.json", "canada.json", "twit.json", "twitter.json", "citm_catalog.json"]

for tst in tsts:
  print ("Test",tst,"loads")
  
  for mod in mods:
    setup2 = setup.replace("ZZZ",mod).replace("YYY",tst)
    print ("  ",(min(timeit.Timer(mod+'.loads(s)', setup=setup2).repeat(10, 1))), "  \t", mod)
  setup2 = setup.replace("ZZZ","mrpacker").replace("YYY",tst)
  print ("  ",(min(timeit.Timer('mrpacker.unpack(packed)', setup=setup2).repeat(10, 1))), "  \t", "mrpacker")

for tst in tsts:
  print ("Test",tst,"dumps")
  for mod in mods:
    setup2 = setup.replace("ZZZ",mod).replace("YYY",tst)
    print ("  ",(min(timeit.Timer(mod+'.dumps(obj)', setup=setup2).repeat(10, 1))), "  \t", mod)
  setup2 = setup.replace("ZZZ","mrpacker").replace("YYY",tst)
  print ("  ",(min(timeit.Timer('mrpacker.pack(obj)', setup=setup2).repeat(10, 1))), "  \t", "mrpacker")

