# Bazaar

<div align="center">
<img src="data/icons/hicolor/scalable/apps/io.github.kolunmi.Bazaar.svg" width="128" height="128" />
</div>

A new app store for GNOME with a focus on discovering and installing
applications and add-ons from Flatpak remotes, particularly
[Flathub](https://flathub.org/), while putting a strong emphasis on
supporting the developers that make the Linux desktop possible. Bazaar
also allows distributors to populate and theme a "curated" tab for
their users with easy to understand YAML files, enhancing the
personality of the app.

Bazaar is fast and highly multi-threaded, guaranteeing a smooth
experience in the UI. You can queue as many downloads as you wish and
run them while perusing Flathub's latest releases. This is due to the
UI being completely decoupled from all backend operations.

It runs as a service, meaning state will be maintained even if you
close all windows, and implements the gnome-shell search provider dbus
interface. A krunner
[plugin](https://github.com/ublue-os/krunner-bazaar) was written for
use on the KDE Plasma desktop.

<img width="1750" height="1044" alt="Image" src="https://github.com/user-attachments/assets/aa5c07bc-693d-4c77-91d5-6d126ff4fda7" />
<img width="888" height="806" alt="Image" src="https://github.com/user-attachments/assets/e19e7d54-89fb-433b-897c-eebc7c207cd2" />
<img width="1806" height="946" alt="Image" src="https://github.com/user-attachments/assets/c56f7c7c-2eb6-4615-9bca-7112dfad80bc" />
<img width="1771" height="1271" alt="Image" src="https://github.com/user-attachments/assets/79b04065-3e1b-4f5d-8260-a32e81c5736b" />

Thanks to everyone in the GNOME development community for creating
such an awesome desktop environment!

If you would like to try this project on your local machine, clone it
on the cli and type these commands inside the project root:

```sh
meson setup build --prefix=/usr/local
ninja -C build
sudo ninja -C build install
bazaar window --auto-service
```

## Supporting

If you would like to support me and the development of this
application (Thank you!), I have a ko-fi here!
https://ko-fi.com/kolunmi

## Instructions for Distributors

If you are a user, this section should be irrelevant to you, but it
still may be interesting.

### Configuring the Browser

Note: the screenshots below are extremely outdated.

As of writing, the browser does not show anything unless you customize
it by detailing "sections" inside any number of YAML configuration
files at locations of your choosing. I will walk you through how to
create one of these YAML files, but first Bazaar needs to know where
to look for them.

Inside `meson_options.txt` you will see an option named
`hardcoded_content_config_path`. This compile time option optionally
specifies a filepath from which to load the "default" configuration at
runtime. This file is NOT embedded into the binary. In case you are
unfamiliar with meson options, simply add the following to your setup
command:

```sh
meson setup build -Dhardcoded_content_config_path=/path/to/config
```

The other way to specify configuration files is with the
`--extra-content-config` cli argument. It takes a single filepath
argument, and you can use this option multiple times.

By the way, every configuration is independently and continuously
monitored for filesystem events throughout the lifetime of the
process. So, in the following tutorial, you can simply save the
configuration whenever you want to see your changes; Bazaar will
automatically reload the relevant parts of the browser.

Once you are set up, copy and paste this snippet into the
configuration and save it:

```yaml
sections:
  - title: "Section #1"
    subtitle: "The first section"
    description: "These are some of my favorite apps!"
    rows: 3
    # use the file:// prefix for a local file
    banner: https://pixls.us/articles/processing-a-nightscape-in-siril/resultat_03_final.jpg
    # can be "fill", "contain", "cover", or "scale-down"
    banner-fit: cover
    appids:
      - com.usebottles.bottles
      - io.mgba.mGBA
      - net.pcsx2.PCSX2
      - org.blender.Blender
      - org.desmume.DeSmuME
      - org.duckstation.DuckStation
      - org.freecad.FreeCAD
      - org.gimp.GIMP
      - org.gnome.Builder
      - org.gnome.Loupe
      - org.inkscape.Inkscape
      - org.kde.krita
```

Your window should now look something like this:

![Image](https://github.com/user-attachments/assets/26cd8977-95d0-4e21-903f-e7cc7abbd776)

You can play around with this a bit and customize it to fit your
needs, or even add new sections (There is no limit!). Unfortunately,
it may still seem a little visually bland. That's where custom css
comes in!

Delete the contents of your configuration and paste this snippet
instead:

```yaml
sections:
  - title: "Section #1"
    subtitle: "The first section"
    description: "These are some of my favorite apps!"
    rows: 3
    # use the file:// prefix for a local file
    banner: https://pixls.us/articles/processing-a-nightscape-in-siril/resultat_03_final.jpg
    # can be "fill", "contain", "cover", or "scale-down"
    banner-fit: cover
    appids:
      - com.usebottles.bottles
      - io.mgba.mGBA
      - net.pcsx2.PCSX2
      - org.blender.Blender
      - org.desmume.DeSmuME
      - org.duckstation.DuckStation
      - org.freecad.FreeCAD
      - org.gimp.GIMP
      - org.gnome.Builder
      - org.gnome.Loupe
      - org.inkscape.Inkscape
      - org.kde.krita
    classes:
      - my-section

css: |
  .my-section {
     border: 5px solid var(--accent-fg-color);
     background: linear-gradient(#ff7788, #553377);
  }
  .my-section entry {
     background-color: alpha(var(--accent-bg-color), 0.05);
  }
  .my-section banner {
     opacity: 0.8;
  }
  .my-section title {
     border-bottom: 2px solid var(--accent-fg-color);
  }
```

Now Bazaar should look like this:

![Image](https://github.com/user-attachments/assets/2fb5c399-380a-48be-be6c-c5c035c2b4a0)

Feel free to modify this to your liking. You can even add animations,
etc. These are the current custom css names at your disposal:

* banner
* banner-text
* banners
* description
* entry
* entry-grid
* subtitle
* title

To learn more about how to use css in gtk, see the following
resources:

* https://docs.gtk.org/gtk4/css-overview.html
* https://docs.gtk.org/gtk4/css-properties.html

To be continued
