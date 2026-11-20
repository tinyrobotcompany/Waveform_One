"""Waveform One Mini vertical enclosure; millimetres, X right/Y up/Z rear."""
from dataclasses import asdict, dataclass
import json
import math
from pathlib import Path

@dataclass(frozen=True)
class Dimensions:
    # Drawing-backed PCB and LED frame; LCD glass/depth and clearances provisional.
    lcd_w: float = 180
    lcd_h: float = 115
    lcd_d: float = 22
    led_w: float = 191.8
    led_h: float = 95.8
    led_d: float = 12.7
    screen_window_w: float = 154
    screen_window_h: float = 86
    border: float = 10
    separator: float = 12
    depth: float = 75
    wall: float = 3
    rear_clearance: float = 35
    print_limit: float = 245

    @property
    def width(self): return max(self.lcd_w,self.led_w)+2*self.border
    @property
    def height(self): return self.lcd_h+self.led_h+self.separator+2*self.border
    @property
    def led_y(self): return self.border+self.led_h/2
    @property
    def lcd_y(self): return self.border+self.led_h+self.separator+self.lcd_h/2
    def validate(self):
        if any(not math.isfinite(v) or v<=0 for v in asdict(self).values()):raise ValueError('Positive finite dimensions required')
        if self.border < self.wall+6 or self.separator < 2*self.wall:raise ValueError('Insufficient enclosure/retainer clearance')
        if self.screen_window_w>=self.lcd_w or self.screen_window_h>=self.lcd_h:raise ValueError('LCD opening must retain a bezel')
        if max(self.width,self.height,self.depth)>self.print_limit:raise ValueError('Outside print envelope')
        if self.depth<max(self.lcd_d,self.led_d)+self.rear_clearance+2*self.wall:raise ValueError('Insufficient cable/service depth')
        return self
    def report(self):
        self.validate()
        wide=self.lcd_w+self.led_w+self.separator+2*self.border
        return dict(status='UNVALIDATED_PROTOTYPE',units='mm',parameters=asdict(self),
                    enclosure=dict(width=self.width,height=self.height,depth=self.depth),
                    comparison=dict(same_components_side_by_side_width=wide,width_reduction_percent=100*(1-self.width/wide)),
                    outstanding=['LCD glass/depth measurement','mounting and cable fit','mute-switch cutout selection','thermal test','acoustic/recognition baseline','electrical safety and stability'])


def geometry(d):
    from build123d import Box,Cylinder,Pos,Align
    d.validate()
    def box(w,h,z,x,y,depth):
        return Pos(x,y,depth)*Box(w,h,z,align=(Align.CENTER,Align.CENTER,Align.MIN))
    # Shell is printed front-down; open rear. Independent back attaches to four bosses.
    shell=box(d.width,d.height,d.depth,0,d.height/2,0)-box(d.width-2*d.wall,d.height-2*d.wall,d.depth,0,d.height/2,d.wall)
    shell-=box(d.led_w-2,d.led_h-2,d.wall+2,0,d.led_y,-1)
    shell-=box(d.screen_window_w,d.screen_window_h,d.wall+2,0,d.lcd_y,-1)
    holes=[(x,y) for x in (-d.width/2+6,d.width/2-6) for y in (6,d.height-6)]
    for x,y in holes:
        shell+=box(9,9,d.depth-d.wall,x,y,d.wall)
        shell-=Pos(x,y,d.depth-12)*Cylinder(1.25,13,align=(Align.CENTER,Align.CENTER,Align.MIN))
    back=box(d.width,d.height,d.wall,0,d.height/2,0)
    for x,y in holes:back-=Pos(x,y,-1)*Cylinder(1.7,d.wall+2,align=(Align.CENTER,Align.CENTER,Align.MIN))
    # Passive rear ventilation and a generous service opening, no connector fit claim.
    for x in range(-70,71,14):
        for y in (25,d.height-25):back-=box(7,22,d.wall+2,x,y,-1)
    back-=box(65,22,d.wall+2,0,d.height/2,-1)
    # Two removable retainer bars, slotted for M3 hardware. Position to measured frame.
    bar=box(80,12,4,0,0,0)
    for x in (-28,28):bar-=box(15,3.4,6,x,0,-1)
    return {'shell':shell,'back':back,'retainer':bar}


def export(destination):
    from build123d import export_step,export_stl
    d=Dimensions().validate();destination=Path(destination);destination.mkdir(parents=True,exist_ok=True)
    for name,part in geometry(d).items():
        if not part.is_valid or len(part.solids())!=1 or part.volume<=0:raise ValueError('Invalid solid: '+name)
        export_step(part,str(destination/(name+'.step')))
        export_stl(part,str(destination/(name+'.stl')),tolerance=0.1,angular_tolerance=0.2)
    (destination/'dimensions.json').write_text(json.dumps(d.report(),indent=2)+'\n')
    # Dimensioned drawing uses the same parameters as the CAD.
    rectangles=''.join(f'<rect x="{30+(d.width-w)/2}" y="{30+d.height-y-h/2}" width="{w}" height="{h}" fill="{colour}"/>' for w,h,y,colour in [(d.screen_window_w,d.screen_window_h,d.lcd_y,'#344b38'),(d.led_w-2,d.led_h-2,d.led_y,'#94be6a')])
    svg=f'''<svg xmlns="http://www.w3.org/2000/svg" width="600" height="720" viewBox="0 0 280 330"><rect width="280" height="330" fill="#f5f6f2"/><g font-family="sans-serif" font-size="6" fill="#152019"><text x="30" y="12">Waveform One Mini — prototype</text><text x="30" y="23">{d.width:.1f} W × {d.height:.1f} H × {d.depth:.1f} D mm</text><rect x="30" y="30" width="{d.width}" height="{d.height}" fill="#162019"/>{rectangles}<text x="30" y="292">Current P4 display + current 64×32 P3 panel</text><text x="30" y="303">UNVALIDATED: measure glass, depth and fixings before print</text><text x="30" y="314">No firmware/display-driver change required by this layout</text></g></svg>'''
    (destination/'front.svg').write_text(svg)

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('destination',type=Path)
    export(parser.parse_args().destination)
