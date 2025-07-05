# These are just convenience scripts, NOT a build system!

appid := env("BAZAAR_APPID", "io.github.kolunmi.Bazaar")

alias run := run-base

run-base: build-base
    ./build/src/bazaar

build-base:
    meson setup build --wipe
    ninja -C build

build-flatpak $manifest="./build-aux/flatpak/io.github.kolunmi.Bazaar.Devel.json" $branch="stable":
    #!/usr/bin/env bash
    mkdir -p ".flatpak-builder"
    FLATPAK_BUILDER_DIR=$(realpath ".flatpak-builder")
    cd "$(dirname "${manifest}")"
    FLATPAK_BUILDER="flatpak-builder"
    BUILDER_ARGS=()
    BUILDER_ARGS+=("--default-branch=${branch}")
    BUILDER_ARGS+=("--state-dir=${FLATPAK_BUILDER_DIR}/flatpak-builder")
    BUILDER_ARGS+=("--user")
    BUILDER_ARGS+=("--ccache")
    BUILDER_ARGS+=("--force-clean")
    BUILDER_ARGS+=("--install")
    BUILDER_ARGS+=("--disable-rofiles-fuse")
    BUILDER_ARGS+=("${FLATPAK_BUILDER_DIR}/build-dir")
    BUILDER_ARGS+=("$(basename "${manifest}")")

    if which flatpak-builder &>/dev/null ; then
        flatpak-builder "${BUILDER_ARGS[@]}"
        exit $?
    else
        flatpak install -y --noninteractive \
        org.gnome.Platform//master \
        org.gnome.Sdk//master \
        org.freedesktop.Platform//24.08 \
        org.freedesktop.Sdk.Extension.llvm18//24.08 \
        org.freedesktop.Sdk.Extension.rust-stable//24.08
        flatpak run org.flatpak.Builder "${BUILDER_ARGS[@]}"
        exit $?
    fi

build-flatpakref:
    #!/usr/bin/env bash
    set -e
    mkdir -p ".flatpak-builder"
    FLATPAK_BUILDER_DIR=$(realpath ".flatpak-builder")
    flatpak build-export repo "${FLATPAK_BUILDER_DIR}/build-dir"
    flatpak build-bundle \
      repo repo \
      io.github.kolunmi.Bazaar.Devel

build-rpm:
    #!/usr/bin/env bash
    mkdir -p rpmbuild
    podman run --rm -i -v .:/build:Z -v ./rpmbuild:/root/rpmbuild:Z registry.fedoraproject.org/fedora:latest <<'EOF'
    set -xeuo pipefail
    dnf install -y rpmdevtools
    mkdir -p $HOME/rpmbuild/SOURCES
    RPMDIR="/build/build-aux/rpm"
    cp "${RPMDIR}"/* $HOME/rpmbuild/SOURCES
    spectool -agR "${RPMDIR}"/bazaar.spec
    dnf builddep -y "${RPMDIR}"/bazaar.spec
    rpmbuild -bb "${RPMDIR}"/bazaar.spec
    EOF
