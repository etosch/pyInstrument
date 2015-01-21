import instrument
import sys
import dis

def trace(frame, a, b):
    print frame.f_locals
    return lambda _1, _2, _3: None

class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y
    
def l1_distance(p1, p2):
    bar = "bar!"
    return abs(p1.x - p2.x) + abs(p1.y - p2.y)

code = compile("""foo = 1; print 'TYPES:', type(__1__), type(__2__)""", "", "single")
bar = instrument.prefixinject(code, l1_distance.func_code)

sys.settrace(trace)
sys.settrace(None)
sys.settrace(lambda a, b, c: None)
print "**********Running with a trace function that does nothing.**********"
print l1_distance(Point(1,1), Point(2,2))
sys.settrace(None)
print "**********Running without a trace function.**********"
print l1_distance(Point(1,1), Point(2,2))
