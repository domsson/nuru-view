<img width="100%" src="nuru-cat.png" alt="nuru-cat">

Display [nuru images](https://github.com/domsson/nuru) in your terminal.

## Status / Overview 

Early work in progress. Prototype alpha stage kinda stuff. Don't use yet.

## Dependencies / Requirements

- Terminal that supports 256 colors ([8 bit color mode](https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit))
- Requires `TIOCGWINSZ` to be supported (to query the terminal size)

## Building / Running

You can compile it with the provided `build` script.

    chmod +x ./build
    ./build

## Installing

When asking nuru-cat to display images that use palettes, it will look for 
those in `$XGD_CONFIG_HOME/glyphs` and `$XGD_CONFIG_HOME/colors` accordingly.
Hence, consider copying the provided palettes there. Assuming `~/.config` to 
be `$XGD_CONFIG_HOME`, that would be:

    mkdir ~/.config/nuru
    cp -r ./nup/* ~/.config/nuru

## Usage

    nuru-cat [OPTIONS...] image-file

Options:

  - `-C`: clear the console before printing
  - `-c FILE`: path to color palette file to use
  - `-g FILE`: path to glyph palette file to use
  - `-h`: print help text and exit
  - `-i`: show image information and exit
  - `-V`: print version information and exit

## Support

[![ko-fi](https://www.ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/L3L22BUD8)

