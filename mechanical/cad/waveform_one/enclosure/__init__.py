"""Printable enclosure parts."""

from build123d import Location, Rot

# Rotations from assembly position to print position.
FRONT_FACE_DOWN = Location(Rot(90, 0, 0))  # front face (Y=0) onto the bed
REAR_FACE_DOWN = Location(Rot(-90, 0, 0))  # rear face (max Y) onto the bed
AS_MODELLED = Location()
