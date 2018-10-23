# hq

A HTML processor inspired by jq (https://github.com/stedolan/jq)

## Building & Usage

### Building

#### Dependencies
- meson
- modest (https://github.com/lexborisov/Modest)

#### Build
`meson build && ninja -C build`

The executable will be built to `build/hq`.

### Usage

#### Dependencies
- modest (https://github.com/lexborisov/Modest)

#### Use

Application help text: 
```
hq (html query) - commandline HTML processor © Robin Broda, 2018
Usage: build/hq [options] <selector> <mode> [mode argument]

Options:
  -h, --help
    show this text
  -f, --file
    file to read (defaults to stdin)

  <selector>
    CSS selector to match against
  <mode>
    processing mode
    may be one of { data, text, attr }:
      data - return raw html of matching elements
      text - return inner text of matching elements
      attr - return attribute value of matching elements
        <mode argument: attr>
          attribute to return

Examples:
  curl -sSL https://example.com | build/hq a data
  curl -sSL https://example.com | build/hq a attr href
```

Example usage:

`curl -s https://coderobe.net | hq a data`
```
<a href="https://keybase.io/coderobe">Keybase (coderobe)</a>
<a href="https://github.com/coderobe">Github (coderobe)</a>
<a href="https://twitter.com/coderobe">Twitter (coderobe)</a>
``` 


`curl -s https://coderobe.net | hq a text` 
```
Keybase (coderobe)
Github (coderobe)
Twitter (coderobe)
```

`curl -s https://coderobe.net | hq a attr href` 
```
https://keybase.io/coderobe
https://github.com/coderobe
https://twitter.com/coderobe
```

You get the idea.

## License

This work, written by Robin Broda (coderobe) in 2018, is licensed under the terms of the GNU Affero General Public License v3.0
