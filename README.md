# mod_my_echo

an endpoint mod for FreeSWITCH

## Usage

```
git submodule add https://github.com/zhanwang-sky/mod_my_echo.git src/mod/endpoints/mod_my_echo
git apply src/mod/endpoints/mod_my_echo/patch
```

then

```
./bootstrap.sh
./configure
make
make mod_my_echo-install
```

in fs_cli:

```
load mod_my_echo
unload mod_my_echo
```

have fun🌞.
