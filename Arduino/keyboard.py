import mido
import pygame
import serial

pygame.init()
pygame.display.set_mode((400, 300))
pygame.display.set_caption('MIDI keyboard')
s = serial.Serial('/dev/ttyACM0', 19200)

NOTES = {
    97: 36,
    119: 37,
    115: 38,
    101: 39,
    100: 40,
    102: 41,
    116: 42,
    103: 43,
    121: 44,
    104: 45,
    117: 46,
    106: 47,
    107: 48,
}

if __name__ == '__main__':
    while True:
        events = pygame.event.get()
        for e in events:
            if e.type not in (pygame.KEYDOWN, pygame.KEYUP):
                continue
            note = NOTES.get(e.key)
            if not note:
                continue
            if e.type == pygame.KEYDOWN:
                s.write(mido.Message('note_on', note=note).bytes())
                s.write(mido.Message('note_on', note=note, channel=1).bytes())
            elif e.type == pygame.KEYUP:
                s.write(mido.Message('note_off', note=note).bytes())
                s.write(mido.Message('note_off', note=note, channel=1).bytes())
