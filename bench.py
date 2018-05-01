import timeit, codecs

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
#tsts = ["str128.json", "twit.json", "chatt1r.json", tter.json","canada.json", "citm_catalog.json", "twitter.json"]
#for z in ["ujson", "rapidjson", "mrjson", "u2json"]:
for tst in tsts:
  print ("Test",tst,"loads")
  for mod in mods:
    setup2 = setup.replace("ZZZ",mod).replace("YYY",tst)
    #print (min(timeit.Timer('json.dumps(z)', setup=setup2).repeat(10, 1)))
    print ("  ",(min(timeit.Timer(mod+'.loads(s)', setup=setup2).repeat(100, 1))), "  \t", mod)
for tst in tsts:
  print ("Test",tst,"dumps")
  for mod in mods:
    setup2 = setup.replace("ZZZ",mod).replace("YYY",tst)
    print ("  ",(min(timeit.Timer(mod+'.dumps(obj)', setup=setup2).repeat(100, 1))), "  \t", mod)
