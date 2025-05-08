# gnome-apps-next

![Image](https://github.com/user-attachments/assets/75cb6702-05a0-41d0-9d99-d0ca772ab0d2)

A new app store idea for GNOME. This is very much work-in-progress,
and nothing about the current state of the project is final; I intend
to add everything necessary to eclipse gnome-software in
functionality, at least as far as flatpak is concerned. Much of the
flatpak related code is referenced from gnome-software, which you can
find [here](https://gitlab.gnome.org/GNOME/gnome-software). Thanks to
everyone in the GNOME development community for creating such an
awesome desktop environment!

If you would like to try this project on your local machine, clone it
on the cli and type these commands inside the project root:

```sh
meson setup build
ninja -C build
./build/src/gnome-apps-next
```

