# Program Change

**What to know before you start:**

- A Program Change encoder selects a patch on a synth or a plug-in. The MIDI channel is the address of the device.
- Most synths hold more than 128 patches. A bank number selects the set, and the program number selects the patch in the set.
- The Twister does not read Program Change. The ring shows the number you set, and not the patch the synth is on.

## Setup

1. In the Midi Fighter Utility, set an encoder to **Program Change**.
2. Set its MIDI channel to the channel of your synth.
3. Turn the encoder. The Twister sends the patch number, 0 to 127.

The MIDI number does nothing for this type. The channel alone selects the device.

A Program Change encoder is an absolute encoder. The sensitivity, the detent and the end dead zone all work as they do on a CC encoder. The value stops at 0 and 127, and does not wrap.

## Banks

The Twister sends three messages as one group, in the order the MIDI standard requires.

Order|Message|MIDI
--|--|--
1|Bank Select MSB|CC 0
2|Bank Select LSB|CC 32
3|Program Change|The patch number

The two bank numbers do not come from the Program Change encoder. They come from other encoders on the same bank page, with the type CC, the number 0 or 32, and the same MIDI channel. The Twister reads the position of those encoders and sends it.

A group is three encoders.

Encoder|Type|Number|Job
--|--|--|--
1|CC|0|The bank, high half
2|CC|32|The bank, low half
3|Program Change|Ignored|The patch, and the group

Put all three on one bank page and one MIDI channel.

You can leave out the parts your synth does not use.

Encoders in the group|What the Twister sends
--|--
CC 0, CC 32 and Program Change|All three messages
CC 0 and Program Change|Bank Select MSB, then the Program Change
Program Change alone|The Program Change

A bank encoder on its own sends only its CC, as any CC encoder does. The group goes out when you turn the Program Change encoder.

With a shift layer, the Twister reads the layer you see. This applies when a bank encoder has the same MIDI channel on both layers.

## Re-send Program

Give the switch the action **Re-send Program**. A press then sends the same three messages again.

Use it in two cases:

- You switched the synth on after you set the encoders.
- You moved a bank encoder, and you want the new bank without a change to the patch number.

This action takes no MIDI feedback, and the LED does not change state with it.

## Two things that surprise people

**The encoder does not send the same patch number twice.** Many synths load the patch again on each Program Change they read. That cuts a held note, and on some synths it takes a moment. So the Twister sends only on a change of the number. A slow turn that lands on the same number stays quiet. Re-send Program is the way to force it.

**Nothing comes back from the synth.** The Twister does not read Program Change, so the ring holds the number you set. A patch change at the synth itself puts the two out of step, until you turn the encoder again.
