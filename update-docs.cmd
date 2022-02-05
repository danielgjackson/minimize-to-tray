@pushd %~dp0
pandoc -s README.md -o docs/index.html
@popd
