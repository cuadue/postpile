from matplotlib import pyplot
import numpy

t = numpy.linspace(0, 4*numpy.pi, 2001)
x = numpy.cos(t)
y = numpy.sin(t)
y[y < 0.1] = 0

l = numpy.sin(t) - 0.1
l *= 2
l[y < 0.1] = 0
l[l > 1] = 1

pyplot.plot(t, x)
pyplot.plot(t, y)
pyplot.plot(t, l)
pyplot.show()
