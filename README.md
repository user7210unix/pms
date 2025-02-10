# Pack My Sh*t
## What is PMS?
PMS is essentially a minimal package manager. Okay, not really, for now, it just runs basic build scripts in the form of a .json.
It is also the official package manager of the famous LearnixOS, the greatest distro (in production) of all time.
## Why make this?
Hi I'm cowmonk, the founder and maintainer of this beauty, being a gigachad LFS user, I felt it was time to start making my own package manager, and with the firey passion to do something in C, pms came into fruition.
My idea was simple: **What makes other package managers so bad?** Well, here are some of the things I compiled:
- It's slow as hell! (I'm looking at you apt)
- It's not intuitive enough!
- It's not SUCKLESS! (Over engineered!)
- and much more! I wouldn't want to get into a long rant about it.

My aim is to basically do the opposite all of these, although the slowness part is definitely going to depend on hardware limitations as this is supposed to be a source based package manager (there is a way of installing binaries through changing the build json).
However my biggest reason was #3, there are rarely any "suckless"-like package managers, so I really wanted to make one. Since we already have suckless core (I use it on my LFS system), it's time to also expand that amazing ecosystem.
### Where the name originates
I have to give credit because the name pms originally came from Learnix's discord server from a **very** active member of the community, if you want to know him, you will have to 👊 SMASH 👊 THAT SUBSCRIBE BUTTON and join LearnixTV's discord server 😜.
As also the creator of LearnixOS, although it's kinda in a perpetual shadow prison because I haven't really gotten any time because of work and among other things, pms was originally made as an idea for LearnixOS, in which was supposed to be LFS based. Of course, seeing how pms is now in LearnixOS repository, LearnixOS has moved from being LFS based, to Gentoo based, to then being Void based, to finally now being back to being LFS-based. So this is the official package manager of LearnixOS.
## Planned features
- ~~Json package builds~~ TOML package builds
  - No longer that we must SUFFER with the terrible looks and garbage looking ebuilds and PKGBUILDS from arch, and the difficulty of understanding what it really does (I'm looking at you gentoo, it's okay but I really don't like the fact you can't see what enabling USE flags will do in ebuilds). I am also hopping it makes the barrier to entry a lot easier, creating an "AUR" or "Gentoo Overlays" of some sort. In the beginning of pms, we decided to use json, but after a lot of consideration, toml is a lot more lightweight and easier to use, so we have decided to use toml instead of json and it still looks nice and human readable.
- Minimal to the bone!
  - For now, I am trying to get the base stuff down, creating a package manager is not easy but I will try my best to follow suckless philosophy and make it easy to hack. Additionally I do want to minimize the dependencies and make sure it's as lightweight as I can, obviously, my choice in using json as the pkg build was obviously a cosmetic choice. Comments are seen throughout the project and they can help some people understand the mess of a job I am doing.
- So easy to use, even a baby can use it!
  - As this is more of a build script package manager, it will be sourced based, however using gentoo's portage manager as a guide, I want to make pms have less sloc and minimal whilst the tools and utility of the package will be as similar or even better than portage, such as syncing and updating packages, (and maybe even USE flags). This will come with time, as I will have to figure out how to implement a USE flag system to pms.
- And much more to come!
# Building
### Prerequisites:
- C compiler (such as clang, gcc, or tcc)
- C libraries
- make
- curl
- ~~cjson~~
- tomlc99[^1]
#### Runtime Dependencies
- tar
- unzip
#### Optional dependencies
- git (repo support and development versions of packages, which is coming soon)
### Actually building it
You will either download the tar from the releases, or get the development pms through git.
Either way, the process will be basically the same. Here's the process (for the development version):
```bash
git clone https://github.com/Rekketstone/pms.git pms
cd pms
make
sudo/doas make install
```
It is recommended to edit the config.h like how you do with dwm. Make changes as you wish.
# Usage
### The pkg.tomlfile
The naming scheme can be anything you desire, but to be frank, the hope is that the naming will be like this in pratice: {package_name}-{ver}.json
Here is an example json package file for your viewing desires
###### mypackage-1.0.0.toml
```toml
title = "mypackage"
version = "1.0.0"

[build]
sources = ["https://github.com/xyz/xyz/whatever.tar.gz"]
patches = ["https://xyz.com/patches/veryimportantsecuritypatch.patch"]

commands = [
    "tar -xvf 1.0.0.tar.gz",
    "cd mypackage-1.0.0",
    "make"
]

[install]
commands = [
    "make install"
]

[uninstall]
# Optional (we will implement a porg like way of uinstallation, but if the package requires it, you can add it here)
commands = [
    "make uninstall"
]

[dependencies]
# Optional (if the package requires any dependencies, you can add it here)
packages = [
    "curl-0.0.1",
    "tar-0.6.9",
    "make-4.3"
]
```
### Running pms
For now pms is very bare, with only four arguments that you can parse to it. You can only run build scripts for now following the mypackage.toml example.
```bash
$ pms --version
pms - Pack My Sh*t version: 0.0.1-beta
$ pms --help
Usage: pms [options] <pkgbuild.toml>
Options:
    -h, --help      Display this help message
    -v, --version   Display version information
$ pms --quiet neofetch.json
Fetching sources... Skipping download: 7.1.0.tar.gz already downloaded!
neofetch-7.1.0/
neofetch-7.1.0/.github/
neofetch-7.1.0/.github/ISSUE_TEMPLATE.md
neofetch-7.1.0/.github/PULL_REQUEST_TEMPLATE.md
neofetch-7.1.0/.travis.yml
neofetch-7.1.0/CONTRIBUTING.md
neofetch-7.1.0/LICENSE.md
neofetch-7.1.0/Makefile
neofetch-7.1.0/README.md
neofetch-7.1.0/neofetch
neofetch-7.1.0/neofetch.1
```
Report any bugs and help make this project become an actual usable source based package manager!
##### Current SLOC count (including header files, Dec 5, 24): ~614
# Credits
It's because of these projects that pms can actually function, so thanks :-)
- ~~[CJSON](https://github.com/DaveGamble/cJSON)~~
  - ~~"Ultralightweight JSON parser in ANSI C"~~
- [tomlc99](https://github.com/cktan/tomlc99)
[^1]: I have decided to use tomlc99 instead of cjson, as it is more lightweight and easier to use.
```
