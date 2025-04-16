This is a hack to use the Voltera One for the dispensing of glue to mount diamonds 


```gcode
This is what I want for my movement 

G54
G0 X7.385 Y0
Z15
Z5
Z0.1
G1 Z0 F200
(Extrude a little material)

G0 Z5
X2.385
Z0.1
G1 Z0 F200
(Extrude a little material)

G0 Z5
X-2.615
Z0.1
G1 Z0 F200
(Extrude a little material)

G0 Z5
Z15
M5
G28
M30



This is the Voltera specific code:


(Axis Alignment)
V3 Z 
(starting position)
V1 X95 Y105
(start motor at 100 units)
V110 R100
(fast descent to near-surface position, surface at -0.6)
V1 Z-0.5 F100
(slow descent to working depth)
V1 Z-0.8 F20
(start moving slowly)
V1 X95 Y100 F50
(up to safe distance)
V1 Z3 F100
(turn off drill)
V110 R1
(go home)
V1 X0 Y0
```
