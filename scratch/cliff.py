from matplotlib import pyplot
import numpy

t = numpy.linspace(0, 16, 1000)
z = numpy.ones_like(t)
z = numpy.cos(0.5 * numpy.pi * (t - 15))
z[t < 15] = 1

pyplot.plot(t, z)
pyplot.show()
