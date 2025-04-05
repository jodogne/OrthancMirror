#!/usr/bin/env python3

import sympy

x = sympy.symbols('x')
(rescaleSlope, rescaleIntercept) = sympy.symbols('rescaleSlope rescaleIntercept')
(windowCenter, windowWidth) = sympy.symbols('windowCenter windowWidth')

t1 = rescaleSlope * x + rescaleIntercept

# Slide 19 of Session 8 of LSINC1114
low = windowCenter - windowWidth / 2.0
high = windowCenter + windowWidth / 2.0
t2 = 255.0 * (t1 - low) / (high - low)

print('MONOCHROME1:', sympy.expand(255 - t2))
print('MONOCHROME2:', sympy.expand(t2))
