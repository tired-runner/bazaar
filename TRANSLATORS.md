# Instructions for Translators

Thank you for your interest in translating Bazaar! 🏷️🗺️💜

Some basic rules:
* You must be fluent in the language you contribute
* You may not use llms to generate the strings (I could do that). If
  you do, I will ban you from the project
  
## Basic Process

Fork the project (so you can open a pr later) and clone the repo. Then
make sure your current directory is the bazaar project root:
```
# Replace '...' with the URL of your Bazaar fork
# for which you have write permissions
git clone ...
cd bazaar
```

Once you've done that, add your language name to `po/LINGUAS`. For
example, if you are adding a Spanish translation, insert `es` into the
newline-separated list, keeping it sorted alphabetically. So if the
`po/LINGUAS` file currently looks like this:

```
# Please keep this file sorted alphabetically.
ab
en_GB
ms
```

you will edit the file to look like this:
```
# Please keep this file sorted alphabetically.
ab
en_GB
es
ms
```

Next, setup the project with meson with the `im_a_translator` flag set
to `true` and enter the build directory:
```sh
meson setup build -Dim_a_translator=true
cd build
```

Run this command to generate the main `pot` (**P**ortable **O**bject
**T**emplate) file:
```sh
meson compile bazaar-pot
```

You might get a bunch of output complaining that the `blp` extension
is unknown. You can ignore this.

Finally, still inside the build directory, run the following command
to update and/or create the `po` (**P**ortable **O**bject) files:
```sh
meson compile bazaar-update-po
```

You are now ready to open your `po` file in your text editor and begin
translating. When you are done, commit your changes and submit a pull
request on github.
