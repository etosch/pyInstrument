Install
=======
`make`

How to use
==========

Basic:
```
>>> import instrument
>>> co = compile("print \"asdf\"", "asdf", "exec")
>>> oc = compile("print \"fdsa\"", "asdf", "exec")
>>> instrument.prefixinject(oc, co)
>>> exec(co)
fdsa
asdf
>>>
```

To capture the arguments:
```
def foo(x):
...   print "Hello, World!"
... 
>>> co = compile("print __1__", "asdf", "exec")
>>> instrument.prefixinject(co, foo.fun_code)
>>> foo(4)
4
Hello, World!
```