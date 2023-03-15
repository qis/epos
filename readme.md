# EPOS
External cheat engine.

## Build
1. Install [Python 3][py].
2. Install [Conan][conan].

```cmd
pip install "conan<2.0.0"
```

3. Install [Visual Studio][vs] with C++ and CMake support.
4. Clone project. Use `x64 Native Tools Command Prompt for VS 2022`.

```cmd
git clone https://github.com/qis/epos epos
cd epos
```

5. Install dependencies.

```cmd
conan install . -if third_party -pr conan.profile
```

[py]: https://www.python.org/downloads/windows/
[vs]: https://visualstudio.microsoft.com/vs/
[conan]: https://conan.io/center/
