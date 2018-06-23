import timeit, codecs

reqs = """
python setup.py install
pip install simplejson
pip install usjon
pip install python0rapidjson
"""

setup = u'''
import codecs, json
import ZZZ
f = codecs.open( "bench/json/YYY", "rb", encoding="utf-8")
s = f.read()
f.close()
obj = json.loads(s)
'''
mods = ["json", "simplejson", "rapidjson", "ujson", "mrjson"]
tsts = ["str128.json", "canada.json", "twit.json", "twitter.json", "citm_catalog.json"]

for tst in tsts:
  print ("Test",tst,"loads")
  for mod in mods:
    setup2 = setup.replace("ZZZ",mod).replace("YYY",tst)
    print ("  ",(min(timeit.Timer(mod+'.loads(s)', setup=setup2).repeat(100, 1))), "  \t", mod)

for tst in tsts:
  print ("Test",tst,"dumps")
  for mod in mods:
    setup2 = setup.replace("ZZZ",mod).replace("YYY",tst)
    print ("  ",(min(timeit.Timer(mod+'.dumps(obj)', setup=setup2).repeat(100, 1))), "  \t", mod)

