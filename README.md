# bazaar

![Image](https://github.com/user-attachments/assets/1e305d42-8907-49d0-8522-ad9f9ed483a4)

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
./build/src/bazaar
```

