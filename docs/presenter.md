# Presenter

A slide clicker. The device pairs with a computer over Bluetooth Low Energy as
a keyboard (the HID over GATT profile every current Mac and PC already has a
driver for), and each tap sends one key. Nothing is installed on the computer.

The app is the only part of this firmware that uses Bluetooth, and only on the
Sticky: `-DPRESENTER_BLE=1` in `[sticky_common]` links it against the NimBLE
stack the prebuilt ESP32-S3 core already carries. The X4 Pro and the simulator
build the same screens with no radio; the simulator pretends a computer is
linked and says so on screen.

## What it sends

| On the device                  | Key sent                                   |
| ------------------------------ | ------------------------------------------ |
| NEXT, the right or down key    | Right arrow (or Page Down)                  |
| PREVIOUS, the left or up key   | Left arrow (or Page Up)                    |
| BLACK                          | `B`                                        |

Arrows are the default because PowerPoint, Keynote and Google Slides all take
them. Page Up and Page Down are what most commercial clickers send, for
programs that want those. `B` blacks the screen in PowerPoint and Keynote, and
in Google Slides' presenter view; a second `B` brings it back.

Starting a presentation is left out on purpose: the shortcut differs by program
and by system (F5, Cmd+Enter, Cmd+Option+P...), and a button that works in one
and does something else in another is worse than none.

## The clock

It starts itself with the first NEXT, so there is nothing to remember. A tap on
the clock pauses it or starts it again; RESET appears while it is paused. With
a limit set it counts down by default (or up, as an option), draws a thick frame
at five minutes left, turns solid black at two, and shows the minutes over time
after that, each change with a long buzz. It redraws once a minute: e-ink
cannot show seconds without flashing.

While a talk is open the device does not go to sleep.

## Pairing and security

- Pairing is "just works": bonded and encrypted (LE Secure Connections), with
  no code to type, because the device has no way to enter one. Anyone in range
  while the app is open and waiting could pair with it the first time. Once a
  computer is paired, the link is encrypted.
- Up to three computers are remembered (the core's `CONFIG_BT_NIMBLE_MAX_BONDS`).
  FORGET PAIRED COMPUTERS clears them on the device; remove "Sticky Presenter"
  in the computer's Bluetooth settings too, or it will keep trying the old keys.
- The radio is on only while the app is open. Leaving the app restarts the
  device quietly, as after Wi-Fi, so the radio's memory comes back whole.
- It advertises as "Sticky Presenter" with the keyboard appearance; it sends no
  data other than the key reports and a battery level the host may read.

## Verified and not

Verified: the code builds for the Sticky with Bluetooth linked in; the clock,
the warnings and the key reports are covered by `host-tests/presenter`; the
screens are checked in the simulator.

Not yet verified on a device: pairing with macOS and Windows, the keys arriving
in each program, reconnection after the computer sleeps, range, and battery
cost. Known general limits of any Bluetooth clicker apply: Google Slides needs
its browser window to have focus, some managed (corporate) computers block new
Bluetooth keyboards, and macOS may open its "identify your keyboard" assistant
the first time, which can simply be closed.
