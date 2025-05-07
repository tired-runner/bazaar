# gnome-apps-next

A new app store idea for GNOME. This is very much work-in-progress,
and nothing about the current state of the project is final; I intend
to add everything necessary to eclipse gnome-software in
functionality, at least as far as flatpak is concerned.

If you would like to try this project on your local machine, either
clone the repo in gnome-builder and press run, or clone it on the cli
type these commands inside the project root:
```sh
meson setup build
ninja -C build
./build/src/gnome-apps-next
```
