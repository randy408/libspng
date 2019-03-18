# Contributing

If you intend to make major changes you should let others know by creating a
[new issue](https://gitlab.com/randy408/libspng/issues).

# Fork

Fork this project on Gitlab and clone your repository.

```
git clone https://gitlab.com/username/libspng.git
cd libspng
git remote add upstream https://gitlab.com/randy408/libspng.git
```

Create a debug build

```
meson -Ddev_build=true --buildtype=debug build
cd build
```

Enable ASan/UBSan

```
meson configure -Db_sanitize=address,undefined
```

# Code

* Coding style should be consistent with the existing style.

* Use C99 standard code.

* Code must be free of undefined behavior

* Code must conform to the rules of the [CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard).


# Test

Start with running Clang Static Analyzer

`ninja scan-build`

Run the testuite

`ninja test`

All tests should pass, some may fail from [known bugs](https://gitlab.com/randy408/libspng/issues?scope=all&utf8=%E2%9C%93&state=opened&label_name[]=bug).

Some runtime errors reported by ASan/UBSan do not fail a test but are considered
bugs, check build/meson-logs/testlog.txt for any errors.

# Push

Create a merge request once you've pushed your local commits.
