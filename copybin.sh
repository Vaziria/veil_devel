rm -rf ~/workspace/dist/bin
mkdir -p ~/workspace/dist/bin

cp ./src/{veild,veil-tx,veil-cli,qt/veil-qt} ~/workspace/dist/bin | true
cp ./src/{veild.exe,veil-tx.exe,veil-cli.exe,qt/veil-qt.exe} ~/workspace/dist/bin | true