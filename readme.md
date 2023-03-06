# EPOS
External cheat engine.

## Build
1. Install [Python 3][py3] to `C:\Python`.
2. Clone this repository to `C:\Workspace\epos`.

```cmd
git clone git@github.com:qis/epos C:/Workspace/epos
cd C:\Workspace\epos
git submodule update --init --depth 1
```

3. Install dependencies using [Conan][conan].

<!--
* Set the system environment variable `CONAN_USER_HOME_SHORT` to `None`.
* Upgrade pip with `python -m pip install --upgrade pip`.
* Upgrade conan with `pip install conan --upgrade`.
-->

```cmd
cd C:\Workspace\epos
conan install . -if third_party -pr conan.profile
```

[py3]: https://www.python.org/downloads/windows/
[conan]: https://conan.io/center/
