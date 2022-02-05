@pushd %~dp0
pandoc --metadata title="Minimize to tray" -s README.md -o docs/index.html
@popd
